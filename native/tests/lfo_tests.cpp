#include "wirescape/engine.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

extern "C" const ws_module_descriptor *WS_CALL ws_test_lfo_entry();
extern "C" const ws_module_descriptor *WS_CALL ws_test_vco_entry();
extern "C" const ws_module_descriptor *WS_CALL ws_test_vca_entry();
extern "C" const ws_module_descriptor *WS_CALL ws_test_output_entry();
static void require(bool ok, const char *message) {
    if (!ok) throw std::runtime_error(message);
}
using Engine = std::unique_ptr<ws_engine, decltype(&ws_engine_destroy)>;

static void send(const ws_module_descriptor *d, void *s, uint32_t id, float value) {
    ws_module_event e{WS_PARAMETER, 0, 0, 0, 0};
    e.parameter_id = id; e.value = value; d->event(s, &e);
}
static std::vector<float> render(const ws_module_descriptor *d, void *s, uint32_t frames,
                                uint32_t block) {
    std::vector<float> result(frames), temp(block);
    float *outputs[] = {temp.data()};
    for (uint32_t pos = 0; pos < frames;) {
        const uint32_t n = std::min(block, frames - pos);
        ws_process_context c{sizeof(c), n, pos, 1};
        d->process(s, nullptr, outputs, &c);
        std::copy_n(temp.data(), n, result.data() + pos);
        pos += n;
    }
    return result;
}
static void processor() {
    const auto *d = ws_test_lfo_entry();
    require(!std::strcmp(d->id, "essentials.lfo") && !std::strcmp(d->name, "LFO") &&
            d->abi_version == WS_ABI_VERSION && d->input_count == 0 &&
            d->output_count == 1 && d->parameter_count == 3, "LFO descriptor");
    require(!std::strcmp(d->outputs[0].id, "control") && d->outputs[0].kind == WS_CONTROL,
            "LFO control output");
    const float mins[] = {.01f, 0, -1}, maxs[] = {20, 1, 1}, initial[] = {1, 1, 0};
    for (uint32_t i = 0; i < 3; ++i)
        require(d->parameters[i].minimum == mins[i] && d->parameters[i].maximum == maxs[i] &&
                d->parameters[i].initial == initial[i], "LFO parameter contract");

    void *s = d->create(48000);
    ws_module_config config{sizeof(config), 512, 48000, 1};
    require(s && d->prepare(s, &config) == 0, "LFO create and prepare");
    float guarded[98]{}; guarded[0] = 42; guarded[2] = 44; guarded[97] = 43;
    float *out[] = {guarded + 1}; ws_process_context c{sizeof(c), 0, 0, 1};
    d->process(s, nullptr, out, &c);
    require(guarded[0] == 42 && guarded[2] == 44 && guarded[97] == 43,
            "zero-frame output guards");
    c.frames = 1; d->process(s, nullptr, out, &c);
    require(guarded[1] == 0 && guarded[0] == 42 && guarded[2] == 44 && guarded[97] == 43,
            "first sample at phase zero and output guards");
    auto quarter = render(d, s, 11999, 512);
    require(std::abs(quarter.back() - 1.0f) < .0002f, "one Hz quarter cycle");
    d->destroy(s);

    // With an offset of .75 the positive crest reaches 1.75, proving output is not clipped.
    s = d->create(48000); d->prepare(s, &config); send(d, s, 2, .75f);
    auto shifted = render(d, s, 12000, 512);
    require(*std::max_element(shifted.begin(), shifted.end()) > 1.74f, "unclipped offset plus depth");
    send(d, s, 1, 0); send(d, s, 2, -.25f); d->reset(s);
    auto flat = render(d, s, 100, 37);
    require(std::all_of(flat.begin(), flat.end(), [](float x) { return x == -.25f; }),
            "zero depth and reset targets snap");
    d->destroy(s);
}

static std::vector<float> frequency_run(double rate, uint32_t block) {
    const auto *d = ws_test_lfo_entry(); void *s = d->create(rate);
    ws_module_config cfg{sizeof(cfg), 512, rate, 1}; d->prepare(s, &cfg);
    send(d, s, 0, 3.0f);
    auto result = render(d, s, static_cast<uint32_t>(rate / 12.0), block);
    d->destroy(s); return result;
}
static std::vector<float> ramp_run(double rate, uint32_t block) {
    const auto *d = ws_test_lfo_entry(); void *s = d->create(rate);
    ws_module_config cfg{sizeof(cfg), 512, rate, 1}; d->prepare(s, &cfg);
    (void)render(d, s, 1, 1); // Establish rendered state before changing the target.
    send(d, s, 1, 0);
    auto result = render(d, s, static_cast<uint32_t>(rate * .005), block);
    d->destroy(s); return result;
}

static std::vector<float> graph(const char *directory) {
    ws_engine_config cfg{sizeof(cfg), 48000, 128, 8, 64};
    Engine engine(ws_engine_create(&cfg), ws_engine_destroy);
    for (auto d : {ws_test_vco_entry(), ws_test_vca_entry(), ws_test_output_entry()})
        require(ws_engine_register_module(engine.get(), d) == WS_OK, "register graph module");
#ifdef _WIN32
    const std::string path = std::string(directory) + "/ws_lfo.dll";
#else
    const std::string path = std::string(directory) + "/libws_lfo.so";
#endif
    require(ws_engine_load_module(engine.get(), path.c_str()) == WS_OK, "load LFO DLL");
    require(ws_engine_add_node(engine.get(), 1, "essentials.vco") == 0 &&
            ws_engine_add_node(engine.get(), 2, "essentials.lfo") == 0 &&
            ws_engine_add_node(engine.get(), 3, "essentials.vca") == 0 &&
            ws_engine_add_node(engine.get(), 4, "essentials.output") == 0, "LFO graph nodes");
    require(ws_engine_connect(engine.get(), 1, 0, 3, 0) == 0 &&
            ws_engine_connect(engine.get(), 2, 0, 3, 1) == 0 &&
            ws_engine_connect(engine.get(), 3, 0, 4, 0) == 0 &&
            ws_engine_prepare(engine.get(), 4, 0) == 0, "LFO to VCA graph routing");
    std::vector<float> result(4096);
    for (uint32_t pos = 0; pos < result.size();) {
        const auto n = std::min<uint32_t>(128, static_cast<uint32_t>(result.size() - pos));
        require(ws_engine_render(engine.get(), result.data() + pos, n) == 0, "LFO graph render");
        pos += n;
    }
    return result;
}

int main(int argc, char **argv) {
    try {
        processor();
        for (double rate : {44100.0, 48000.0, 96000.0}) {
            require(frequency_run(rate, 512) == frequency_run(rate, 37),
                    "frequency phase is partition invariant");
            const auto ramp = ramp_run(rate, 512);
            require(ramp == ramp_run(rate, 37), "parameter ramp is partition invariant");
            const double first = std::sin(6.2831853071795864769 / rate) *
                                 (1.0 - 1.0 / static_cast<uint32_t>(rate * .005));
            require(std::abs(ramp.front() - first) < 1e-7 && ramp.back() == 0,
                    "5 ms depth ramp duration and endpoint");
            const auto frequency = frequency_run(rate, 512);
            const double expected = std::sin(6.2831853071795864769 * 3.0 *
                                             (frequency.size() - 1) / rate);
            require(std::abs(frequency.back() - expected) < 1e-6,
                    "3 Hz phase progression at sample rate");
        }
        const auto *d = ws_test_lfo_entry(); void *s = d->create(48000);
        ws_module_config cfg{sizeof(cfg), 128, 48000, 1}; d->prepare(s, &cfg);
        send(d, s, 0, 3); auto before = render(d, s, 25, 25); d->reset(s);
        auto after = render(d, s, 25, 25);
        require(before == after, "reset restores phase zero and snaps parameter target");
        d->destroy(s);
        auto output = graph(argc > 1 ? argv[1] : ".");
        require(std::any_of(output.begin(), output.end(), [](float x) { return std::abs(x) > 1e-5f; }),
                "generic DLL LFO to VCA graph output");
        std::cout << "PASS LFO descriptor, phase, ramps, reset and generic graph\n";
        return 0;
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n'; return 1;
    }
}
