// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "../io/net/osc_service.h"
#include "app_engine.h"
#include <memory>
#include <string>
#include <vector>
namespace wirescape::osc {
// Control-owner host; worker delivery retains the graph generation from start.
class Host {
    std::unique_ptr<Service> service_;
    AppEngine *engine_ = nullptr;
    uint64_t engine_generation_ = 0;
    bool authorized_ = false;
    static int deliver(void *, uint32_t, const ws2_event &, uint64_t) noexcept;

  public:
    explicit Host(bool confirmed_lan = false) : authorized_(confirmed_lan) {}
    ~Host() { stop(); }
    Host(const Host &) = delete;
    Host &operator=(const Host &) = delete;
    void start(AppEngine *, const std::string &document);
    void stop() noexcept;
    std::vector<RouteState> states() const;
};
} // namespace wirescape::osc
