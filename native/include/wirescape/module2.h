/* SPDX-License-Identifier: MIT */
#ifndef WIRESCAPE_MODULE2_H
#define WIRESCAPE_MODULE2_H
#include <stdint.h>
#include <stddef.h>
#if defined(_WIN32)
#define WS2_CALL __cdecl
#define WS2_EXPORT __declspec(dllexport)
#else
#define WS2_CALL
#define WS2_EXPORT __attribute__((visibility("default")))
#endif
#ifdef __cplusplus
extern "C" {
#endif
#define WS_ABI2_VERSION 2u
#define WS2_MAX_CHANNELS 16u
#define WS2_MAX_PORTS 32u
#define WS2_MAX_PARAMS 32u
#define WS2_MAX_PAYLOAD 1472u
#define WS2_DIRECT_PORT UINT32_MAX
enum ws2_result { WS2_OK=0, WS2_INVALID=1, WS2_CAPACITY=2, WS2_UNSUPPORTED=3, WS2_EXCEPTION=4, WS2_STATE=5 };
enum ws2_port_kind { WS2_SIGNAL=1, WS2_EVENT=2 };
enum ws2_hint { WS2_ANY, WS2_AUDIO, WS2_PITCH, WS2_CV_BI, WS2_CV_UNI, WS2_GATE, WS2_TRIGGER, WS2_EV_NOTES, WS2_EV_MIDI, WS2_EV_MESSAGE };
enum ws2_poly { WS2_POLY, WS2_SUM, WS2_FIRST };
enum ws2_signal_flags { WS2_CONSTANT=1u, WS2_CONNECTED=2u };
enum ws2_param_flags { WS2_MODULATABLE=1u, WS2_PER_VOICE=2u, WS2_AUDIO_RATE_OK=4u, WS2_STEPPED=8u, WS2_AUTOMATABLE=16u, WS2_HIDDEN=32u };
enum ws2_taper { WS2_LINEAR, WS2_LOG, WS2_EXP };
enum ws2_memory_hint { WS2_FAST=1, WS2_LARGE=2 };
enum ws2_event_type { WS2_NOTE_ON=1, WS2_NOTE_OFF, WS2_NOTE_CHOKE, WS2_NOTE_EXPRESSION, WS2_PARAM, WS2_PARAM_MOD, WS2_TRANSPORT_EVENT, WS2_MIDI, WS2_MESSAGE };
enum ws2_expression { WS2_EX_PITCH, WS2_EX_PRESSURE, WS2_EX_TIMBRE, WS2_EX_PAN };
enum ws2_message_kind { WS2_MSG_OSC=1, WS2_MSG_UDP, WS2_MSG_TEXT, WS2_MSG_BYTES, WS2_MSG_STRUCT };
enum ws2_transport_flags { WS2_PLAYING=1u, WS2_RECORDING=2u, WS2_LOOPING=4u };

typedef struct ws2_signal { uint32_t struct_size; const float *const *data; uint32_t channels,flags; } ws2_signal;
typedef struct ws2_signal_out { uint32_t struct_size; float *const *data; uint32_t channels,flags,max_channels; } ws2_signal_out;
typedef struct ws2_port {
    uint32_t struct_size; const char *id,*name; uint32_t kind,hint,poly;
    const char *stereo_pair; float unpatched_value;
} ws2_port;
typedef struct ws2_param {
    uint32_t struct_size; const char *id,*name,*unit;
    float minimum,maximum,initial; uint32_t legacy_index,taper; float curve,step;
    uint32_t flags,option_count; const char *const *options; uint32_t decimals;
    int (WS2_CALL *value_to_text)(float,char*,uint32_t);
} ws2_param;
/* An event belongs to one process call. MESSAGE data is borrowed from that
   call's bounded arena. Never retain it after process returns. */
typedef struct ws2_event {
    uint32_t struct_size,type,offset,port,flags;
    int32_t note_id,channel,key; uint32_t source_id,index;
    float value,value2; uint32_t expression,message_kind,bytes;
    const uint8_t *data; uint32_t ump[4],ump_words;
} ws2_event;
typedef struct ws2_event_list { uint32_t struct_size,count; const ws2_event *items; } ws2_event_list;
typedef struct ws2_event_out {
    uint32_t struct_size; void *ctx; uint32_t port;
    int (WS2_CALL *push)(void*,uint32_t,const ws2_event*);
} ws2_event_out;
typedef struct ws2_transport {
    uint32_t struct_size,flags; double bpm,beat,bar_start;
    uint32_t ts_num,ts_den; double loop_start,loop_end;
} ws2_transport;
typedef struct ws2_host {
    uint32_t struct_size; void *ctx;
    void *(WS2_CALL *alloc)(void*,size_t,size_t,uint32_t);
} ws2_host;
typedef struct ws2_config {
    uint32_t struct_size; double sample_rate;
    uint32_t max_block_frames,max_channels; uint64_t seed;
    const uint8_t *data; uint32_t bytes;
} ws2_config;
typedef struct ws2_memory { uint32_t struct_size; size_t fast_bytes,large_bytes,alignment; } ws2_memory;
typedef struct ws2_ports {
    uint32_t struct_size,input_count,output_count;
    const ws2_port *inputs,*outputs;
} ws2_ports;
typedef struct ws2_writer {
    uint32_t struct_size; void *ctx;
    int (WS2_CALL *write)(void*,const void*,uint32_t);
} ws2_writer;
typedef struct ws2_reader {
    uint32_t struct_size; void *ctx;
    int (WS2_CALL *read)(void*,void*,uint32_t);
    uint32_t bytes;
} ws2_reader;
typedef struct ws2_process {
    uint32_t struct_size,frames; uint64_t frame_position,generation; double sample_rate;
    uint32_t channels,input_count,output_count,param_count;
    const ws2_signal *inputs; ws2_signal_out *outputs; const ws2_signal *params;
    const ws2_event_list *event_inputs; const ws2_event_out *event_outputs;
    ws2_event_list events; const ws2_transport *transport;
} ws2_process;
typedef struct ws2_descriptor {
    uint32_t struct_size,abi_version; const char *id,*name; uint32_t version;
    uint32_t input_count,output_count,param_count;
    const ws2_port *inputs,*outputs; const ws2_param *params;
    void *(WS2_CALL *create)(const ws2_host*);
    void (WS2_CALL *destroy)(void*);
    int (WS2_CALL *query_ports)(void*,const ws2_config*,ws2_ports*);
    int (WS2_CALL *memory_requirements)(const ws2_config*,ws2_memory*);
    int (WS2_CALL *prepare)(void*,const ws2_config*,const ws2_host*);
    void (WS2_CALL *reset)(void*);
    int (WS2_CALL *process)(void*,ws2_process*);
    int (WS2_CALL *save_state)(void*,const ws2_writer*);
    int (WS2_CALL *load_state)(void*,const ws2_reader*);
    uint32_t (WS2_CALL *latency_frames)(void*);
} ws2_descriptor;
typedef const ws2_descriptor *(WS2_CALL *ws2_entry_fn)(void);
#ifndef WS_MODULE2_VERSION_NAME
#define WS_MODULE2_VERSION_NAME ws_module2_abi_version
#endif
WS2_EXPORT uint32_t WS2_CALL WS_MODULE2_VERSION_NAME(void);
#ifndef WS_MODULE2_ENTRY_NAME
#define WS_MODULE2_ENTRY_NAME ws_module2_entry
#endif
WS2_EXPORT const ws2_descriptor *WS2_CALL WS_MODULE2_ENTRY_NAME(void);
#ifdef __cplusplus
}
#endif
#endif
