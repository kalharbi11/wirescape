// SPDX-License-Identifier: MIT
#pragma once
#include <atomic>
#include <cstdint>
namespace allocation_watch {
extern std::atomic<bool> enabled;
extern std::atomic<uint64_t> count;
extern thread_local bool thread_enabled;
extern thread_local uint64_t thread_count;
struct Scope {
    Scope(){count=0;enabled=true;}
    ~Scope(){enabled=false;}
    uint64_t allocations()const{return count.load();}
};
// For a render thread running concurrently with legal control-thread allocations.
struct ThreadScope {
    ThreadScope() {
        thread_count = 0;
        thread_enabled = true;
    }
    ~ThreadScope() { thread_enabled = false; }
    uint64_t allocations() const { return thread_count; }
};
}
