// SPDX-License-Identifier: MIT
#pragma once
#include <atomic>
#include <cstdint>
namespace allocation_watch {
extern std::atomic<bool> enabled;
extern std::atomic<uint64_t> count;
struct Scope {
    Scope(){count=0;enabled=true;}
    ~Scope(){enabled=false;}
    uint64_t allocations()const{return count.load();}
};
}
