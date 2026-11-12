// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "../modules2/groove/config.h"
#include "../modules2/midi_cc_to_cv/config.h"
#include "../modules2/osc_in/config.h"
#include <nlohmann/json.hpp>
#include <stdexcept>
#include <string>
#include <vector>
namespace wirescape::document {
// Quiescent JSON-to-module adapter. The JSON shape, not struct padding, is saved.
inline std::vector<uint8_t> runtime_module_config_impl(const std::string &type, const nlohmann::json &value,
                                                       uint32_t instance_id, std::vector<std::string> *ignored_keys) {
    auto require = [](bool ok, const char *s) {
        if (!ok) throw std::runtime_error(s);
    };
    require(value.is_object(), "Module config must be an object");
    if (type == "essentials.chord") {
        require(value.empty(), "Chord has no native configuration keys");
        return {uint8_t(instance_id), uint8_t(instance_id >> 8), uint8_t(instance_id >> 16),
                uint8_t(instance_id >> 24)};
    }
    if (value.empty()) return {};
    if (type == "sequencing.groove") {
        using namespace groove;
        require(value.size() == 3 && value.contains("version") && value.contains("bpm") && value.contains("tracks"),
                "Groove requires version, bpm and tracks");
        require(value.at("version").is_number_integer(), "Invalid Groove version");
        const auto version = value.at("version").get<int64_t>();
        require(version == 1 || version == 2, "Unsupported Groove configuration version");
        require(value.at("bpm").is_number() && std::isfinite(value.at("bpm").get<double>()) &&
                    value.at("bpm").get<double>() >= 20 && value.at("bpm").get<double>() <= 300,
                "Groove BPM outside range");
        const auto &tracks = value.at("tracks");
        require(tracks.is_array() && tracks.size() >= 1 && tracks.size() <= 2, "Groove requires one or two tracks");
        Config config;
        config.bpm = static_cast<float>(value.at("bpm").get<double>());
        config.track_count = static_cast<uint32_t>(tracks.size());
        // v1 had no chance conditions; preserving its old render seed is intentional.
        config.instance_seed = version == 2 ? instance_id : 0;
        config.authored_version = static_cast<uint32_t>(version);
        for (size_t t = 0; t < tracks.size(); ++t) {
            const auto &track = tracks[t];
            require(track.is_object() && track.contains("steps"), "Invalid Groove track");
            for (auto it = track.begin(); it != track.end(); ++it)
                require(it.key() == "steps" || it.key() == "mute" || it.key() == "armed" ||
                            (version == 2 && (it.key() == "length" || it.key() == "div")),
                        "Unknown Groove track setting");
            auto &native = config.tracks[t];
            native.length = version == 1 ? 32u : 16u;
            if (track.contains("length")) {
                const auto &length = track.at("length");
                require(length.is_number_integer(), "Groove length must be integer");
                const auto n = length.get<int64_t>();
                require(n == 8 || n == 16 || n == 32 || n == 64, "Groove length must be 8/16/32/64");
                native.length = static_cast<uint32_t>(n);
            }
            if (track.contains("div")) {
                const auto &speed = track.at("div");
                require(version == 2 && speed.is_number_integer(), "Groove div must be a v2 integer");
                const auto n = speed.get<int64_t>();
                require(n == 1 || n == 2 || n == 4 || n == 8, "Groove div must be 1/2/4/8");
                native.div = static_cast<uint32_t>(n);
            }
            if (track.contains("mute")) {
                require(track.at("mute").is_boolean(), "Groove mute must be boolean");
                native.mute = track.at("mute").get<bool>() ? 1u : 0u;
            }
            if (track.contains("armed")) {
                require(track.at("armed").is_boolean(), "Groove armed must be boolean");
                native.armed = track.at("armed").get<bool>() ? 1u : 0u;
            }
            const auto &steps = track.at("steps");
            const uint32_t limit = version == 1 ? 32u : native.length;
            require(steps.is_array() && steps.size() >= 1 && steps.size() <= limit,
                    "Groove step count outside track length");
            for (size_t i = 0; i < steps.size(); ++i) {
                const auto &s = steps[i];
                require(s.is_object() && s.contains("kind") && s.at("kind").is_string(), "Invalid Groove step");
                const auto kind = s.at("kind").get<std::string>();
                require(kind == "note" || kind == "rest" || kind == "tie", "Unknown Groove step kind");
                for (auto it = s.begin(); it != s.end(); ++it)
                    require(it.key() == "kind" || it.key() == "mod1" || it.key() == "mod2" ||
                                (kind == "note" && (it.key() == "key" || it.key() == "velocity" ||
                                                    (version == 2 && it.key() == "keys"))) ||
                                (version == 2 && it.key() == "gate") ||
                                (version == 2 && kind == "note" && it.key() == "cond"),
                            "Unknown Groove step setting");
                auto &row = native.steps[i];
                row.kind = kind == "note" ? Note : kind == "tie" ? Tie : Rest;
                if (kind == "note") {
                    require(s.contains("key") && s.contains("velocity") && s.at("key").is_number_integer() &&
                                s.at("velocity").is_number(),
                            "Invalid Groove note");
                    const auto key = s.at("key").get<int64_t>();
                    const auto velocity = s.at("velocity").get<double>();
                    require(key >= 0 && key <= 127 && std::isfinite(velocity) && velocity >= 0 && velocity <= 1,
                            "Groove note outside range");
                    row.key = static_cast<int32_t>(key);
                    row.keys[0] = row.key;
                    row.velocity = static_cast<float>(velocity);
                    if (s.contains("keys")) {
                        const auto &keys = s.at("keys");
                        require(version == 2 && keys.is_array() && keys.size() >= 1 && keys.size() <= 5,
                                "Groove keys must be a nonempty array of at most five notes");
                        row.key_count = static_cast<uint8_t>(keys.size());
                        for (size_t k = 0; k < keys.size(); ++k) {
                            require(keys[k].is_number_integer(), "Groove chord keys must be integers");
                            const auto n = keys[k].get<int64_t>();
                            require(n >= 0 && n <= 127, "Groove chord key outside MIDI range");
                            row.keys[k] = static_cast<int32_t>(n);
                            require((k != 0 || n == key) && (k == 0 || n != key) &&
                                        (k < 2 || row.keys[k] > row.keys[k - 1]),
                                    "Groove chord must have root first, distinct sorted remaining keys");
                        }
                    }
                }
                for (const char *name : {"mod1", "mod2"}) {
                    if (!s.contains(name)) continue;
                    const auto &v = s.at(name);
                    require(v.is_number() && std::isfinite(v.get<double>()) && v.get<double>() >= 0 &&
                                v.get<double>() <= 1,
                            "Groove modulation outside range");
                    (std::string(name) == "mod1" ? row.mod1 : row.mod2) = static_cast<float>(v.get<double>());
                }
                if (version == 2 && s.contains("gate")) {
                    const auto &gate = s.at("gate");
                    require(gate.is_number(), "Groove gate must be numeric");
                    const double g = gate.get<double>();
                    require(g == 0.25 || g == 0.5 || g == 0.75 || g == 1., "Groove gate outside quarter steps");
                    row.gate_q = static_cast<uint8_t>(g * 4);
                }
                if (version == 2 && s.contains("cond")) {
                    const auto &cond = s.at("cond");
                    require(kind == "note" && cond.is_object() && cond.contains("type") && cond.at("type").is_string(),
                            "Groove condition needs a note and type");
                    const auto type_name = cond.at("type").get<std::string>();
                    if (type_name == "chance") {
                        require(cond.size() == 2 && cond.contains("p") && cond.at("p").is_number_integer(),
                                "Groove chance requires integer p");
                        const auto p = cond.at("p").get<int64_t>();
                        require(p >= 1 && p <= 99, "Groove chance outside 1..99");
                        row.cond_type = Chance;
                        row.cond_a = static_cast<uint8_t>(p);
                    } else if (type_name == "ab") {
                        require(cond.size() == 3 && cond.contains("a") && cond.contains("b") &&
                                    cond.at("a").is_number_integer() && cond.at("b").is_number_integer(),
                                "Groove A:B requires integers");
                        const auto a = cond.at("a").get<int64_t>();
                        const auto b = cond.at("b").get<int64_t>();
                        require(b >= 2 && b <= 8 && a >= 1 && a <= b, "Groove A:B outside range");
                        row.cond_type = AB;
                        row.cond_a = static_cast<uint8_t>(a);
                        row.cond_b = static_cast<uint8_t>(b);
                    } else if (type_name == "first" || type_name == "not_first") {
                        require(cond.size() == 1, "Groove first condition has no extra keys");
                        row.cond_type = type_name == "first" ? First : NotFirst;
                    } else require(false, "Unknown Groove condition");
                }
            }
        }
        Config validated;
        require(decode(reinterpret_cast<const uint8_t *>(&config), sizeof(config), validated),
                "Invalid Groove configuration");
        std::vector<uint8_t> bytes(sizeof(config));
        std::memcpy(bytes.data(), &config, sizeof(config));
        return bytes;
    }
    if (type == "osc.in") {
        // All authored OSC configuration is validated on the control owner,
        // then copied as bounded private POD for the module's prepare() call.
        wirescape::osc::Configuration config;
        // This authored JSON version is not the private POD version. Restrictive
        // future keys must bump it so older engines refuse instead of broadening behavior.
        if (value.contains("version")) {
            const auto &authored = value.at("version");
            require(authored.is_number_integer() && authored.get<int64_t>() == 1, "Unsupported OSC config version");
        }
        auto record = [&](std::string name) {
            if (!ignored_keys || ignored_keys->size() >= 16) return;
            for (char &ch : name) {
                if (static_cast<unsigned char>(ch) < 0x21 || static_cast<unsigned char>(ch) > 0x7e) ch = '?';
            }
            if (name.size() > 64) name.resize(64);
            ignored_keys->push_back("osc.in#" + std::to_string(instance_id) + "." + name);
        };
        // Future top-level keys do not affect the native execution configuration.
        for (auto it = value.begin(); it != value.end(); ++it)
            if (it.key() != "version" && it.key() != "port" && it.key() != "lan" && it.key() != "rows")
                record(it.key());
        if (value.contains("port")) {
            const auto &port = value.at("port");
            require(port.is_number_integer(), "OSC port must be an integer");
            const auto n = port.get<int64_t>();
            require(n >= 1 && n <= 65535, "OSC port outside 1..65535");
            config.port = static_cast<uint32_t>(n);
        }
        if (value.contains("lan")) {
            require(value.at("lan").is_boolean(), "OSC LAN toggle must be boolean");
            config.lan = value.at("lan").get<bool>() ? 1u : 0u;
        }
        if (value.contains("rows")) {
            const auto &rows = value.at("rows");
            require(rows.is_array() && rows.size() <= config.rows.size(), "At most eight OSC rows");
            for (size_t i = 0; i < rows.size(); ++i) {
                const auto &src = rows.at(i);
                require(src.is_object(), "OSC row must be an object");
                auto &dest = config.rows[i];
                for (auto it = src.begin(); it != src.end(); ++it) {
                    const auto &name = it.key();
                    const auto &item = it.value();
                    if (name == "pattern") {
                        require(item.is_string(), "OSC pattern must be a string");
                        const auto pattern = item.get<std::string>();
                        require(!pattern.empty() && pattern.size() < sizeof(dest.pattern), "OSC pattern too long");
                        std::memcpy(dest.pattern, pattern.c_str(), pattern.size() + 1);
                    } else if (name == "kind" || name == "stale") {
                        require(item.is_string(), "OSC row kind/stale must be a string");
                        const auto val = item.get<std::string>();
                        if (name == "kind") {
                            require(val == "cv" || val == "gate" || val == "trigger", "Invalid OSC row kind");
                            dest.kind = val == "cv" ? 0 : val == "gate" ? 1 : 2;
                        } else {
                            require(val == "hold" || val == "zero", "Invalid OSC timeout policy");
                            dest.stale = val == "zero" ? 1u : 0u;
                        }
                    } else if (name == "enabled") {
                        // Optional; absent means enabled so existing patches keep listening.
                        require(item.is_boolean(), "OSC row enabled must be boolean");
                        dest.enabled = item.get<bool>() ? 1u : 0u;
                    } else if (name == "argument") {
                        require(item.is_number_integer(), "OSC argument index must be integer");
                        const auto n = item.get<int64_t>();
                        require(n >= 0 && n < 32, "OSC argument index outside range");
                        dest.argument = static_cast<uint32_t>(n);
                    } else if (name == "input_min" || name == "input_max" || name == "output_min" ||
                               name == "output_max" || name == "smoothing_ms" || name == "timeout_ms") {
                        require(item.is_number() && std::isfinite(item.get<double>()), "OSC row value must be finite");
                        const auto n = static_cast<float>(item.get<double>());
                        if (name == "input_min") dest.input_min = n;
                        else if (name == "input_max") dest.input_max = n;
                        else if (name == "output_min") dest.output_min = n;
                        else if (name == "output_max") dest.output_max = n;
                        else if (name == "smoothing_ms") dest.smoothing_ms = n;
                        else dest.timeout_ms = n;
                    } else record("rows[" + std::to_string(i) + "]." + name);
                }
            }
        }
        require(wirescape::osc::configuration(nullptr, 0, config), "Invalid OSC module configuration");
        std::vector<uint8_t> bytes(sizeof(config));
        std::memcpy(bytes.data(), &config, sizeof(config));
        return bytes;
    }
    if (type == "midi.in") {
        for (auto it = value.begin(); it != value.end(); ++it)
            require((it.key() == "endpoint" || it.key() == "system_name") && it.value().is_string(),
                    "Invalid MIDI input endpoint configuration");
        if (value.contains("endpoint"))
            require(!value.at("endpoint").get<std::string>().empty(), "Empty MIDI endpoint");
        return {};
    }
    require(type == "midi.cc_to_cv", "Module config needs a module-specific adapter");
    require(value.size() == 1 && value.contains("rows") && value.at("rows").is_array() && value.at("rows").size() <= 8,
            "CC configuration requires at most eight rows");
    wirescape::midi::CcConfiguration config;
    size_t index = 0;
    for (const auto &row : value.at("rows")) {
        require(row.is_object(), "CC row must be an object");
        auto &r = config.rows[index++];
        for (auto it = row.begin(); it != row.end(); ++it) {
            const auto &key = it.key();
            auto &v = it.value();
            if (key == "channel" || key == "controller") {
                require(v.is_number_integer(), "CC channel/controller must be integers");
                auto n = v.get<int64_t>();
                require(n >= 0 && n <= (key == "channel" ? 16 : 127), "CC channel/controller outside range");
                if (key == "channel") r.channel = static_cast<uint8_t>(n);
                else r.controller = static_cast<uint8_t>(n);
            } else if (key == "fourteen_bit" || key == "bipolar") {
                require(v.is_boolean(), "CC toggles must be boolean");
                if (key == "fourteen_bit") r.fourteen_bit = v.get<bool>();
                else r.bipolar = v.get<bool>();
            } else if (key == "smoothing_ms") {
                require(v.is_number(), "CC smoothing must be numeric");
                double n = v.get<double>();
                require(std::isfinite(n) && n >= 0 && n <= 2000, "CC smoothing outside range");
                r.smoothing_ms = static_cast<float>(n);
            } else if (key == "min" || key == "max") {
                require(v.is_number(), "CC range endpoints must be numbers");
                double n = v.get<double>();
                require(std::isfinite(n), "CC range endpoints must be finite");
                if (key == "min") {
                    r.min = n;
                    r.has_min = 1;
                } else {
                    r.max = n;
                    r.has_max = 1;
                }
            } else require(false, "Unknown CC row setting");
        }
    }
    require(wirescape::midi::cc_configuration(nullptr, 0, config), "Invalid CC row combination");
    std::vector<uint8_t> bytes(sizeof(config));
    std::memcpy(bytes.data(), &config, sizeof(config));
    return bytes;
}

// Preserve the original conversion signature and validation for all callers.
inline std::vector<uint8_t> runtime_module_config(const std::string &type, const nlohmann::json &value,
                                                  uint32_t instance_id = 0) {
    return runtime_module_config_impl(type, value, instance_id, nullptr);
}

// Control-thread diagnostics come from the parser's unknown-field fall-through.
inline std::vector<uint8_t> runtime_module_config(const std::string &type, const nlohmann::json &value,
                                                  uint32_t instance_id, std::vector<std::string> *ignored_keys) {
    return runtime_module_config_impl(type, value, instance_id, ignored_keys);
}
}
