// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "wirescape/module2.h"
#include <cstdint>
#include <memory>
#include <string>
#include <vector>
struct ws_module_descriptor;
namespace wirescape::engine2 {
enum class Timing { Frames, Immediate, Timestamped };
struct Config {
    double sample_rate=48000;
    uint32_t max_block=256,max_channels=16,max_nodes=64;
    uint32_t event_budget=1024,release_reserve=256,ring_capacity=256,future_capacity=4096;
    uint32_t message_arena_bytes=1472*1024,feedback_block=32;
    double bpm=120; bool playing=true;
    uint64_t epoch_ns=0,seed=1;
};
struct Meter {
    uint64_t frame=0,generation=1,applied_generation=1,events=0;
    uint64_t dropped=0,coalesced=0,ring_full=0,stale=0,late=0;
    uint64_t release_emergencies=0,arena_full=0,faults=0;
    uint32_t arena_used=0,feedback_edges=0,latency_frames=0;
    size_t setup_bytes=0;
};
struct Outgoing {
    uint32_t node=0,port=0;uint64_t frame=0,time_ns=0,generation=0;
    ws2_event event{};uint8_t payload[WS2_MAX_PAYLOAD]{};
};
// Construction, registration, graph/configuration/state operations and destruction
// are quiescent. Each producer handle has exactly one writer. Only render owns
// processor state. panic(), enqueue(), pop_egress() and meter() are the explicit
// cross-thread surface; no other methods imply thread safety.
class Engine {
    struct Impl;std::unique_ptr<Impl>p_;
public:
    explicit Engine(Config config={});~Engine();
    Engine(const Engine&)=delete;Engine&operator=(const Engine&)=delete;
    bool add_node(uint32_t id,const ws2_descriptor*,const std::vector<uint8_t>&config={});
    bool add_abi1(uint32_t id,const ws_module_descriptor*);
    bool connect(uint32_t from,uint32_t output,uint32_t to,uint32_t input);
    bool modulate(uint32_t from,uint32_t output,uint32_t to,uint32_t parameter,float amount);
    bool output(uint32_t node,uint32_t port,uint32_t bus_channel);
    int register_producer();
    bool prepare();
    bool set_parameter(uint32_t node,uint32_t index,float plain);
    bool set_channels(uint32_t node,uint32_t channels);
    int enqueue(uint32_t producer,uint32_t node,uint32_t port,const ws2_event&,
                uint64_t time,Timing mode=Timing::Frames,uint64_t generation=0)noexcept;
    int render(float*const*outputs,uint32_t output_channels,uint32_t frames,
               uint64_t callback_time_ns=UINT64_MAX)noexcept;
    bool pop_egress(Outgoing&)noexcept;
    void panic()noexcept;
    void reset();
    void transport(double bpm,bool playing,double beat=0);
    bool publish_clock(uint64_t frame,uint64_t time_ns);
    Meter meter()const noexcept;
    const std::string&error()const;
    const ws2_descriptor*descriptor(uint32_t node)const;
    uint32_t output_channels(uint32_t node,uint32_t port)const;
    bool output_constant(uint32_t node,uint32_t port)const;
    float output_sample(uint32_t node,uint32_t port,uint32_t channel,uint32_t frame)const;
    uint32_t feedback_edges()const;
    std::vector<uint8_t>save_state(uint32_t node);
    int load_state(uint32_t node,const std::vector<uint8_t>&bytes);
    const std::vector<uint8_t>&preserved_state(uint32_t node)const;
    bool reconfigure(uint32_t node,const std::vector<uint8_t>&config);
    uint32_t dropped_connections()const;
};
}
