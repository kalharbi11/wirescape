#ifndef WIRESCAPE_ENGINE_H
#define WIRESCAPE_ENGINE_H
#include "module.h"
#if defined(_WIN32) && !defined(WS_ENGINE_BUILD)
#define WS_API __declspec(dllimport)
#else
#define WS_API WS_EXPORT
#endif
#ifdef __cplusplus
extern "C" {
#endif
typedef struct ws_engine ws_engine;
enum ws_result {
    WS_OK = 0,
    WS_INVALID = 1,
    WS_STATE = 2,
    WS_CAPACITY = 3,
    WS_NOT_FOUND = 4,
    WS_GRAPH = 5,
    WS_LIBRARY = 6,
    WS_BUSY = 7
};
typedef struct ws_engine_config {
    uint32_t struct_size;
    double sample_rate;
    uint32_t max_block_frames, max_nodes, max_events;
} ws_engine_config;
typedef struct ws_event {
    uint32_t struct_size;
    uint64_t frame;
    uint32_t node_id, type, parameter_id;
    float value;
    uint32_t source_id, note_id;
    uint64_t generation;
} ws_event;
typedef struct ws_meter {
    uint32_t struct_size;
    uint64_t frame_position, rendered_blocks, late_events;
    float peak;
    uint32_t queued_events;
    uint64_t generation, overflow_events, overload_blocks;
    uint32_t overload_reason; /* 0 none, 1 event budget, 2 graph-span budget */
} ws_meter;
WS_API uint32_t WS_CALL ws_engine_abi_version(void);
WS_API ws_engine *WS_CALL ws_engine_create(const ws_engine_config *);
WS_API void WS_CALL ws_engine_destroy(ws_engine *);
WS_API const char *WS_CALL ws_engine_error(const ws_engine *);
WS_API int WS_CALL ws_engine_load_module(ws_engine *, const char *library_path);
WS_API int WS_CALL ws_engine_register_module(ws_engine *, const ws_module_descriptor *);
WS_API uint32_t WS_CALL ws_engine_module_count(const ws_engine *);
WS_API const ws_module_descriptor *WS_CALL ws_engine_module(const ws_engine *, uint32_t index);
WS_API int WS_CALL ws_engine_add_node(ws_engine *, uint32_t id, const char *module_id);
WS_API int WS_CALL ws_engine_connect(ws_engine *, uint32_t source, uint32_t source_port,
                                     uint32_t destination, uint32_t destination_port);
WS_API int WS_CALL ws_engine_prepare(ws_engine *, uint32_t output_node, uint32_t output_port);
WS_API int WS_CALL ws_engine_enqueue(ws_engine *, const ws_event *);
/* Quiescent setup only: prepared graph, no rendered frames or queued events.
   Validates and applies a parameter without spending the live event budget.
   Caller must stop the producer/device; never invoke concurrently with them. */
WS_API int WS_CALL ws_engine_initialize_parameter(ws_engine *, uint32_t node,
                                                uint32_t parameter, float value);
WS_API int WS_CALL ws_engine_render(ws_engine *, float *output, uint32_t frames);
/* Per render call: at most 256 queue entries examined and 64 graph spans.
   Overload silences the whole call, invalidates queued commands, advances time,
   increments overload_blocks and returns WS_CAPACITY. No partial audible block. */
WS_API int WS_CALL ws_engine_reset(ws_engine *);
WS_API int WS_CALL ws_engine_activate(ws_engine *);
WS_API int WS_CALL ws_engine_deactivate(ws_engine *);
/* Panic is a bounded, out-of-band latch: flushes queued events, invalidates old
   generation commands, and resets module voices at the next render boundary. */
WS_API int WS_CALL ws_engine_panic(ws_engine *);
WS_API uint64_t WS_CALL ws_engine_generation(const ws_engine *);
WS_API int WS_CALL ws_engine_get_meter(const ws_engine *, ws_meter *);
/* Loads ws_vco/ws_adsr/ws_vca/ws_output from directory, creates IDs 1..4,
   connects VCO -> VCA audio, ADSR -> VCA gain, VCA -> Output and prepares. */
WS_API int WS_CALL ws_engine_build_essentials(ws_engine *, const char *module_directory);
/* Lifecycle: graph construction on one control thread, then prepare once.
   One producer may enqueue concurrently with one render consumer. Render accepts
   1..max_block_frames. Events use absolute sample frames and nondecreasing order;
   events at the end of a call affect its next sample. Note-off matches source/id.
   Reset/destroy require producer and consumer to be stopped. Descriptors returned
   above are borrowed until destroy. No audio device is opened by this library. */
#ifdef __cplusplus
}
#endif
#endif
