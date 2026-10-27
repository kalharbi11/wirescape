// SPDX-License-Identifier: GPL-3.0-only
// Focused offline acceptance coverage for the shipped VCF and ladder DLLs.
#include "wirescape/engine.h"
#include "wirescape/module.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
void require(bool okay, const std::string &message) {
    if (!okay) throw std::runtime_error(message);
}
struct EngineDeleter { void operator()(ws_engine *engine) const { ws_engine_destroy(engine); } };
struct Processor {
    const ws_module_descriptor *descriptor;
    void *state = nullptr;
    Processor(const ws_module_descriptor *d, double rate) : descriptor(d) {
        state = descriptor->create(rate);
        require(state != nullptr, "processor create failed");
        ws_module_config config{sizeof(config), 512, rate, 1};
        require(descriptor->prepare(state, &config) == 0, "processor prepare failed");
    }
    ~Processor() { if (state) descriptor->destroy(state); }
    Processor(const Processor &) = delete;
    Processor &operator=(const Processor &) = delete;
    void set(uint32_t id, float value) {
        ws_module_event event{WS_PARAMETER, id, value, 0, 0};
        descriptor->event(state, &event);
    }
    void reset() { descriptor->reset(state); }
    void render(const float *input, float *const *outputs, uint32_t frames, uint64_t position) {
        const float *inputs[] = {input};
        ws_process_context context{sizeof(context), frames, position, 1};
        descriptor->process(state, descriptor->input_count ? inputs : nullptr, outputs, &context);
    }
};

void check_sample(float value, const char *what) {
    if (!std::isfinite(value)) throw std::runtime_error(std::string(what) + " produced non-finite output");
    if (std::abs(value) >= 16.0f) throw std::runtime_error(std::string(what) + " exceeded normal-signal bound");
}

void impulse_cases(const ws_module_descriptor *d, bool vcf, double rate) {
    const std::array<float, 3> cutoffs{{20.0f, 1000.0f,
        static_cast<float>(std::min(18000.0, 0.4 * rate))}};
    const uint32_t output_count = d->output_count;
    constexpr uint32_t tail_frames = 2048;
    std::array<float, 512> input{}, first{}, second{};
    std::array<float *, 2> outputs{{first.data(), second.data()}};
    for (float cutoff : cutoffs) {
        Processor processor(d, rate);
        processor.set(0, cutoff);
        processor.set(1, vcf ? 0.95f : 1.0f);
        if (!vcf) processor.set(2, 0.0f);
        processor.reset(); // Snap initialized targets before the first sample.
        input.fill(0.0f);
        input[0] = 0.25f;
        float overall[2]{};
        std::array<float, tail_frames> final_first{}, final_second{};
        for (uint32_t offset = 0; offset < static_cast<uint32_t>(rate * 2.0); offset += 512) {
            const uint32_t frames = std::min(512u, static_cast<uint32_t>(rate * 2.0) - offset);
            if (offset) input.fill(0.0f);
            processor.render(input.data(), outputs.data(), frames, offset);
            for (uint32_t i = 0; i < frames; ++i) {
                check_sample(first[i], "filter impulse");
                overall[0] = std::max(overall[0], std::abs(first[i]));
                if (output_count > 1) {
                    check_sample(second[i], "VCF second impulse output");
                    overall[1] = std::max(overall[1], std::abs(second[i]));
                }
            }
            const uint32_t tail_begin = static_cast<uint32_t>(rate * 2.0) - tail_frames;
            const uint32_t copy_begin = std::max(offset, tail_begin);
            for (uint32_t sample = copy_begin; sample < offset + frames; ++sample) {
                const uint32_t destination = sample - tail_begin;
                final_first[destination] = first[sample - offset];
                if (output_count > 1) final_second[destination] = second[sample - offset];
            }
        }
        const float quiet_limit0 = std::max(1.0e-7f, overall[0] * 1.0e-3f);
        for (uint32_t i = 0; i < tail_frames; ++i) {
            require(std::abs(final_first[i]) < quiet_limit0, "filter impulse tail did not become quiet");
            if (output_count > 1) {
                const float quiet_limit1 = std::max(1.0e-7f, overall[1] * 1.0e-3f);
                require(std::abs(final_second[i]) < quiet_limit1, "VCF second impulse tail did not become quiet");
            }
        }
    }
}

void modulation_case(const ws_module_descriptor *d, bool vcf, double rate) {
    Processor processor(d, rate);
    std::array<float, 512> input{}, first{}, second{};
    std::array<float *, 2> outputs{{first.data(), second.data()}};
    const uint32_t active_frames = static_cast<uint32_t>(rate * 0.25);
    const std::array<uint32_t, 2> partitions{{1, 17}};
    uint32_t position = 0;
    size_t setting = 0;
    while (position < active_frames) {
        const float cutoff = (setting & 1) ? 18000.0f : 20.0f;
        processor.set(0, std::min(cutoff, static_cast<float>(0.4 * rate)));
        processor.set(1, (setting & 2) ? (vcf ? 0.95f : 1.0f) : 0.0f);
        if (!vcf) processor.set(2, (setting & 4) ? 1.0f : 0.0f);
        const uint32_t frames = std::min(partitions[setting & 1], active_frames - position);
        for (uint32_t i = 0; i < frames; ++i)
            input[i] = 0.2f * std::sin(static_cast<float>(2.0 * 3.14159265358979323846 * 997.0 * (position + i) / rate));
        processor.render(input.data(), outputs.data(), frames, position);
        for (uint32_t i = 0; i < frames; ++i) {
            check_sample(first[i], "modulated filter");
            if (vcf) check_sample(second[i], "modulated VCF second output");
        }
        position += frames;
        ++setting;
    }

    processor.set(0, 1000.0f);
    processor.set(1, vcf ? 0.95f : 1.0f);
    if (!vcf) processor.set(2, 1.0f);
    uint32_t rendered = 0;
    const uint32_t tail_frames = static_cast<uint32_t>(rate * 2.0);
    float peak[2]{};
    std::array<float, 2048> final_first{}, final_second{};
    while (rendered < tail_frames) {
        const uint32_t frames = std::min(512u, tail_frames - rendered);
        input.fill(0.0f);
        processor.render(input.data(), outputs.data(), frames, active_frames + rendered);
        for (uint32_t i = 0; i < frames; ++i) {
            check_sample(first[i], "modulation release");
            peak[0] = std::max(peak[0], std::abs(first[i]));
            if (vcf) {
                check_sample(second[i], "VCF modulation release second output");
                peak[1] = std::max(peak[1], std::abs(second[i]));
            }
        }
        const uint32_t tail_begin = tail_frames - static_cast<uint32_t>(final_first.size());
        const uint32_t copy_begin = std::max(rendered, tail_begin);
        for (uint32_t sample = copy_begin; sample < rendered + frames; ++sample) {
            const uint32_t destination = sample - tail_begin;
            final_first[destination] = first[sample - rendered];
            if (vcf) final_second[destination] = second[sample - rendered];
        }
        rendered += frames;
    }
    for (uint32_t i = 0; i < final_first.size(); ++i) {
        require(std::abs(final_first[i]) < std::max(1.0e-7f, peak[0] * 1.0e-3f), "modulation release tail did not become quiet");
        if (vcf) require(std::abs(final_second[i]) < std::max(1.0e-7f, peak[1] * 1.0e-3f), "VCF release tail did not become quiet");
    }
}

void run_module(ws_engine *loader, const char *id, bool vcf) {
    const ws_module_descriptor *d = nullptr;
    for (uint32_t i = 0; i < ws_engine_module_count(loader); ++i) {
        const auto *candidate = ws_engine_module(loader, i);
        if (candidate && candidate->id && std::strcmp(candidate->id, id) == 0) d = candidate;
    }
    require(d != nullptr, std::string("module not discovered: ") + id);
    require(d->struct_size >= sizeof(ws_module_descriptor) && d->abi_version == WS_ABI_VERSION,
            std::string("invalid descriptor: ") + id);
    require(d->input_count == 1 && d->output_count == (vcf ? 2u : 1u) && d->parameter_count == (vcf ? 2u : 3u),
            std::string("unexpected filter descriptor shape: ") + id);
    for (double rate : {44100.0, 48000.0, 96000.0}) {
        impulse_cases(d, vcf, rate);
        modulation_case(d, vcf, rate);
    }
}
}

int main(int argc, char **argv) {
    try {
        require(argc == 2, "expected module directory argument");
        ws_engine_config config{sizeof(config), 48000.0, 512, 8, 64};
        std::unique_ptr<ws_engine, EngineDeleter> engine(ws_engine_create(&config));
        require(engine != nullptr, "offline loader engine creation failed");
        const std::string directory(argv[1]);
        for (const char *module : {"ws_vcf", "ws_ladder"}) {
            const std::string path = directory + "/" + module;
#ifdef _WIN32
            const std::string library = path + ".dll";
#elif __APPLE__
            const std::string library = directory + "/lib" + module + ".dylib";
#else
            const std::string library = directory + "/lib" + module + ".so";
#endif
            require(ws_engine_load_module(engine.get(), library.c_str()) == WS_OK,
                    std::string("failed to load actual module DLL: ") + library);
        }
        run_module(engine.get(), "essentials.vcf", true);
        run_module(engine.get(), "essentials.ladder", false);
        std::puts("PASS: returned VCF and ladder filter offline acceptance stress");
        return 0;
    } catch (const std::exception &error) {
        std::fprintf(stderr, "FAIL: returned filter acceptance: %s\n", error.what());
        return 1;
    }
}
