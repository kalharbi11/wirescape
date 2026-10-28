// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <cstdint>
#include <cstddef>
#include <string>
#include <vector>
#include <memory>
namespace wirescape::midi {
struct Endpoint { std::string name, system_name; };
struct Callback {
    void* context = nullptr;
    void (*message)(void*, const uint8_t*, size_t, uint64_t) = nullptr;
};
// Enumeration/open/close are control-side operations. One callback writer per endpoint.
class Backend {
public:
    virtual ~Backend() = default;
    virtual std::vector<Endpoint> endpoints() = 0;
    virtual bool open(const Endpoint&, Callback) = 0;
    virtual void close(const Endpoint&) = 0; // returns only after its callbacks have stopped
};
// False returns null before constructing any library observer or backend object.
std::unique_ptr<Backend> real_backend(bool confirm_midi);
}
