#include "wirescape/engine.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

extern "C" const ws_module_descriptor *WS_CALL ws_test_scale_offset_entry();
extern "C" const ws_module_descriptor *WS_CALL ws_test_vco_entry();
extern "C" const ws_module_descriptor *WS_CALL ws_test_adsr_entry();
extern "C" const ws_module_descriptor *WS_CALL ws_test_vca_entry();
extern "C" const ws_module_descriptor *WS_CALL ws_test_output_entry();

static void require(bool ok, const char *message) {
    if (!ok) throw std::runtime_error(message);
}
using Engine = std::unique_ptr<ws_engine, decltype(&ws_engine_destroy)>;

static void processor() {
    const auto *d = ws_test_scale_offset_entry();
    require(!std::strcmp(d->id, "essentials.scale_offset") &&
            !std::strcmp(d->name, "Scale/Offset") && d->abi_version == WS_ABI_VERSION &&
            d->input_count == 1 && d->output_count == 1 && d->parameter_count == 2,
            "scale/offset descriptor");
    require(d->inputs[0].kind == WS_CONTROL && d->outputs[0].kind == WS_CONTROL &&
            !std::strcmp(d->inputs[0].id, "control") && !std::strcmp(d->outputs[0].id, "control"),
            "scale/offset ports");
    require(d->parameters[0].minimum == -2 && d->parameters[0].maximum == 2 &&
            d->parameters[0].initial == 1 && d->parameters[1].minimum == -1 &&
            d->parameters[1].maximum == 1 && d->parameters[1].initial == 0,
            "scale/offset parameter ranges");
    void *state = d->create(48000);
    require(state != nullptr, "scale/offset create");
    ws_module_config config{sizeof(config), 32, 48000, 1};
    require(d->prepare(state, &config) == 0, "scale/offset prepare");

    float source[] = {-2, -1, 0, 1, 2, 3};
    float guarded[] = {91, 0, 0, 0, 0, 0, 0, 92};
    const float *inputs[] = {source};
    float *outputs[] = {guarded + 1};
    ws_process_context context{sizeof(context), 6, 0, 1};
    d->process(state, inputs, outputs, &context);
    require(guarded[0] == 91 && guarded[7] == 92, "output guards");
    for (int i = 0; i < 6; ++i) require(guarded[i + 1] == source[i], "default pass-through");

    ws_module_event event{WS_PARAMETER, 0, 0, 0, 0};
    event.parameter_id = 0; event.value = -2; d->event(state, &event);
    event.parameter_id = 1; event.value = .25f; d->event(state, &event);
    d->reset(state); // reset snaps the established targets
    d->process(state, inputs, outputs, &context);
    for (int i = 0; i < 6; ++i)
        require(std::abs(guarded[i + 1] - (source[i] * -2 + .25f)) < 1e-6f,
                "negative gain, offset, unclipped output");

    event.parameter_id = 0; event.value = 0; d->event(state, &event);
    event.parameter_id = 1; event.value = -.5f; d->event(state, &event);
    d->reset(state);
    const float *missing[] = {nullptr};
    d->process(state, missing, outputs, &context);
    for (int i = 0; i < 6; ++i) require(guarded[i + 1] == -.5f, "unpatched input offset");
    d->destroy(state);
}

static std::vector<float> ramped(const ws_module_descriptor *d, double rate, uint32_t block) {
    void *state = d->create(rate);
    ws_module_config config{sizeof(config), 512, rate, 1};
    require(d->prepare(state, &config) == 0, "ramp prepare");
    float input[512]; std::fill_n(input, 512, 1.0f);
    float output[512]{};
    const float *inputs[] = {input}; float *outputs[] = {output};
    ws_process_context context{sizeof(context), 1, 0, 1};
    d->process(state, inputs, outputs, &context);
    ws_module_event event{WS_PARAMETER, 0, 0, 0, 0};
    event.parameter_id = 0; event.value = 0; d->event(state, &event);
    event.parameter_id = 1; event.value = .5f; d->event(state, &event);
    const uint32_t length = static_cast<uint32_t>(rate * .005);
    std::vector<float> result(length);
    for (uint32_t position = 0; position < length;) {
        const auto frames = std::min(block, length - position);
        context.frames = frames; context.frame_position = position + 1;
        d->process(state, inputs, outputs, &context);
        std::copy_n(output, frames, result.data() + position);
        position += frames;
    }
    d->destroy(state);
    return result;
}

static std::vector<float> graph(uint32_t block, const char *directory, float scale, float offset,
                                bool directEnvelope = false) {
    ws_engine_config config{sizeof(config), 48000, 512, 8, 64};
    Engine engine(ws_engine_create(&config), ws_engine_destroy);
    for (auto d : {ws_test_vco_entry(), ws_test_adsr_entry(), ws_test_vca_entry(), ws_test_output_entry()})
        require(ws_engine_register_module(engine.get(), d) == WS_OK, "register graph module");
#ifdef _WIN32
    const std::string path = std::string(directory) + "/ws_scale_offset.dll";
#else
    const std::string path = std::string(directory) + "/libws_scale_offset.so";
#endif
    require(ws_engine_load_module(engine.get(), path.c_str()) == WS_OK, "load scale/offset DLL");
    // The generic loader must expose the same identity and typed one-in/one-out shape.
    require(ws_engine_add_node(engine.get(), 1, "essentials.vco") == 0 &&
            ws_engine_add_node(engine.get(), 2, "essentials.adsr") == 0 &&
            (!directEnvelope ? ws_engine_add_node(engine.get(), 3, "essentials.scale_offset") == 0 : true) &&
            ws_engine_add_node(engine.get(), 4, "essentials.vca") == 0 &&
            ws_engine_add_node(engine.get(), 5, "essentials.output") == 0, "graph nodes");
    const bool controlRoute = directEnvelope
        ? ws_engine_connect(engine.get(), 2, 0, 4, 1) == 0
        : ws_engine_connect(engine.get(), 2, 0, 3, 0) == 0 &&
          ws_engine_connect(engine.get(), 3, 0, 4, 1) == 0;
    require(ws_engine_connect(engine.get(), 1, 0, 4, 0) == 0 && controlRoute &&
            ws_engine_connect(engine.get(), 4, 0, 5, 0) == 0 &&
            ws_engine_prepare(engine.get(), 5, 0) == 0, "ADSR scale/offset VCA routing");
    auto enqueue = [&](uint32_t node, uint32_t parameter, float value) {
        ws_event e{sizeof(e), 0, node, WS_PARAMETER, parameter, value, 0, 0,
                   ws_engine_generation(engine.get())};
        require(ws_engine_enqueue(engine.get(), &e) == 0, "enqueue graph parameter");
    };
    if (!directEnvelope) { enqueue(3, 0, scale); enqueue(3, 1, offset); }
    enqueue(2, 0, .001f); enqueue(2, 1, .001f); enqueue(2, 2, 1); enqueue(2, 3, .001f);
    ws_event gate{sizeof(gate), 0, 2, WS_GATE, 0, 1, 0, 1, ws_engine_generation(engine.get())};
    require(ws_engine_enqueue(engine.get(), &gate) == 0, "enqueue graph gate");
    std::vector<float> rendered(4096);
    for (uint32_t position = 0; position < rendered.size();) {
        const auto frames = std::min<uint32_t>(block, static_cast<uint32_t>(rendered.size() - position));
        require(ws_engine_render(engine.get(), rendered.data() + position, frames) == 0, "graph render");
        position += frames;
    }
    return rendered;
}

int main(int argc, char **argv) {
    try {
        processor();
        const auto *scaleOffset = ws_test_scale_offset_entry();
        for (double rate : {44100.0, 48000.0, 96000.0}) {
            const auto one = ramped(scaleOffset, rate, 512);
            const auto split = ramped(scaleOffset, rate, 37);
            require(one == split, "ramp partition invariance at sample rates");
            require(one.front() > 0.99f && one.front() < 1.0f && one.back() == .5f,
                    "5 ms scale and offset ramps");
            for (size_t i = 1; i < one.size(); ++i)
                require(one[i] <= one[i - 1], "monotonic scale and offset ramp");
        }
        const char *directory = argc > 1 ? argv[1] : ".";
        auto defaults = graph(32, directory, 1, 0);
        auto partitioned = graph(512, directory, 1, 0);
        require(defaults == partitioned, "graph partition invariance");
        auto directEnvelope = graph(32, directory, 1, 0, true);
        require(defaults == directEnvelope, "default scale/offset matches direct ADSR-to-VCA control");
        auto silent = graph(32, directory, 0, 0);
        require(std::all_of(silent.begin(), silent.end(), [](float x) { return x == 0; }),
                "zero scale and offset is silent");
        auto shifted = graph(32, directory, 0, .5f);
        require(std::any_of(shifted.begin(), shifted.end(), [](float x) { return std::abs(x) > .01f; }),
                "offset makes gated graph audible");
        std::cout << "PASS scale/offset descriptor, processing, generic loading and ADSR graph\n";
        return 0;
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
