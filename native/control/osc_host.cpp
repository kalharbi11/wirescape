// SPDX-License-Identifier: GPL-3.0-only
#include "osc_host.h"
#include <nlohmann/json.hpp>
#include <stdexcept>
namespace wirescape::osc {
int Host::deliver(void *context, uint32_t node, const ws2_event &event, uint64_t timestamp) noexcept {
    auto &self = *static_cast<Host *>(context);
    if (!self.engine_) return WS2_INVALID;
    auto &graph = app_engine_native(self.engine_);
    const int result = graph.enqueue(2, node, WS2_DIRECT_PORT, event, timestamp, engine2::Timing::Timestamped,
                                     self.engine_generation_);
    if (result == WS2_OK && self.monitor_) self.monitor_->osc(node, event, timestamp);
    return result;
}
// A message is dropped only if it matches a disabled row and no enabled row;
// unmatched traffic still reaches `messages` so users can discover addresses.
bool Host::accept(void *context, uint32_t node, std::string_view address) noexcept {
    const auto &self = *static_cast<Host *>(context);
    for (const auto &filter : self.filters_) {
        if (filter->node != node) continue;
        bool disabled = false;
        for (uint32_t r = 0; r < 8; ++r)
            if (filter->patterns[r].match(address)) {
                if (filter->enabled[r]) return true;
                disabled = true;
            }
        return !disabled;
    }
    return true;
}
void Host::start(AppEngine *engine, const std::string &document) {
    stop();
    auto patch = nlohmann::json::parse(document);
    std::vector<Route> routes;
    for (const auto &module : patch.at("modules")) {
        if (module.value("module", std::string{}) != "osc.in") continue;
        const auto config = module.value("config", nlohmann::json::object());
        const uint32_t port = config.value("port", 9000u);
        if (port < 1 || port > 65535) throw std::runtime_error("OSC port outside 1..65535");
        const uint32_t node = module.at("id").get<uint32_t>();
        const bool lan = config.value("lan", false);
        // Absent `enabled` means enabled; only a module whose eight rows are all
        // explicitly disabled ("Clear") is left unbound and reported as disabled.
        uint32_t disabled = 0;
        auto filter = std::make_unique<Filter>();
        filter->node = node;
        filter->enabled.fill(true);
        std::array<std::string, 8> patterns;
        for (uint32_t i = 0; i < 8; ++i) patterns[i] = "/pico1/knob/" + std::to_string(i + 1);
        if (config.contains("rows") && config.at("rows").is_array())
            for (size_t i = 0; i < config.at("rows").size() && i < 8; ++i) {
                const auto &row = config.at("rows").at(i);
                if (!row.is_object()) continue;
                if (row.contains("pattern") && row.at("pattern").is_string())
                    patterns[i] = row.at("pattern").get<std::string>();
                if (row.contains("enabled") && row.at("enabled") == false) {
                    filter->enabled[i] = false;
                    ++disabled;
                }
            }
        if (disabled > 0 && disabled < 8) {
            for (uint32_t i = 0; i < 8; ++i)
                if (!filter->patterns[i].compile(patterns[i])) throw std::runtime_error("Invalid OSC row pattern");
            filters_.push_back(std::move(filter));
        }
        if (disabled >= 8) {
            RouteState state;
            state.node = node;
            state.port = uint16_t(port);
            state.lan_requested = lan;
            state.state = "disabled";
            disabled_.push_back(state);
        } else routes.push_back({node, uint16_t(port), lan});
    }
    if (routes.size() + disabled_.size() > 32) throw std::runtime_error("Too many OSC routes");
    if (routes.empty()) return;
    service_ = std::make_unique<Service>(Sink{this, deliver, accept}, authorized_);
    for (const auto &route : routes)
        if (!service_->add(route)) throw std::runtime_error("Cannot stage OSC route");
    engine_ = engine;
    engine_generation_ = app_engine_native(engine_).meter().generation;
    // Busy ports report unavailable status; they never refuse the patch.
    (void)service_->start();
}
void Host::stop() noexcept {
    if (service_) {
        service_->stop();
        service_.reset();
    }
    disabled_.clear();
    filters_.clear();
    engine_ = nullptr;
    engine_generation_ = 0;
}
std::vector<RouteState> Host::states() const {
    auto out = service_ ? service_->states() : std::vector<RouteState>{};
    out.insert(out.end(), disabled_.begin(), disabled_.end());
    return out;
}
} // namespace wirescape::osc
