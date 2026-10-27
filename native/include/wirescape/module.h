#ifndef WIRESCAPE_MODULE_H
#define WIRESCAPE_MODULE_H
#include <stdint.h>
#if defined(_WIN32)
#define WS_CALL __cdecl
#define WS_EXPORT __declspec(dllexport)
#else
#define WS_CALL
#define WS_EXPORT __attribute__((visibility("default")))
#endif
#ifdef __cplusplus
extern "C" {
#endif
#define WS_ABI_VERSION 1u
enum ws_port_kind { WS_AUDIO = 1, WS_CONTROL = 2 };
enum ws_event_type { WS_PARAMETER = 1, WS_NOTE_ON = 2, WS_NOTE_OFF = 3, WS_GATE = 4 };
typedef struct ws_parameter_descriptor {
    uint32_t id;
    const char *name;
    float minimum, maximum, initial;
    const char *unit;
} ws_parameter_descriptor;
typedef struct ws_port_descriptor {
    const char *id;
    const char *name;
    uint32_t kind;
} ws_port_descriptor;
typedef struct ws_module_event {
    uint32_t type, parameter_id;
    float value;
    uint32_t source_id, note_id;
} ws_module_event;
typedef struct ws_module_config {
    uint32_t struct_size, max_block_frames;
    double sample_rate;
    uint64_t generation;
} ws_module_config;
typedef struct ws_process_context {
    uint32_t struct_size, frames;
    uint64_t frame_position, generation;
} ws_process_context;
/* All data is immutable and remains valid until the library is unloaded.
   event/process/reset must be bounded, allocation-free and nonblocking.
   create/prepare/destroy execute quiescently and may allocate. Every callback
   must contain its own exceptions; no exception may cross this C ABI. */
typedef struct ws_module_descriptor {
    uint32_t struct_size, abi_version;
    const char *id;
    const char *name;
    uint32_t input_count, output_count, parameter_count;
    const ws_port_descriptor *inputs;
    const ws_port_descriptor *outputs;
    const ws_parameter_descriptor *parameters;
    void *(WS_CALL *create)(double sample_rate);
    void(WS_CALL *destroy)(void *);
    int(WS_CALL *prepare)(void *, const ws_module_config *);
    void(WS_CALL *reset)(void *);
    void(WS_CALL *event)(void *, const ws_module_event *);
    void(WS_CALL *process)(void *, const float *const *, float *const *,
                           const ws_process_context *);
} ws_module_descriptor;
typedef const ws_module_descriptor *(WS_CALL *ws_module_entry_fn)(void);
/* Each module shared library exports precisely this discovery entry point. */
WS_EXPORT const ws_module_descriptor *WS_CALL ws_module_entry(void);
#ifdef __cplusplus
}
#endif
#endif
