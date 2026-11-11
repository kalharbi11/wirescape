// SPDX-License-Identifier: GPL-3.0-only
#include "osc_host.h"
#include <nlohmann/json.hpp>
#include <stdexcept>
namespace wirescape::osc {
int Host::deliver(void *context, uint32_t node, const ws2_event &event, uint64_t timestamp) noexcept {
    auto &self = *static_cast<Host *>(context);
    if (!self.engine_) return WS2_INVALID;
    auto &graph = app_engine_native(self.engine_);
    return graph.enqueue(2, node, WS2_DIRECT_PORT, event, timestamp, engine2::Timing::Timestamped,
                         self.engine_generation_);
}
void Host::start(AppEngine *engine, const std::string &document) {
    stop();
    auto patch = nlohmann::json::parse(document);
    std::vector<Route> routes;
    for (const auto &module : patch.at("modules")) {
        if (module.value("module", std::string{}) != "osc.in") continue;
        const auto config = module.value("config", nlohmann::json::object());
        const uint32_t port = config.value("port", 9000u);
        if (port < 1 || port > 65535) throw std::runtime_error("OSC port outside 1..65535");
        routes.push_back({module.at("id").get<uint32_t>(), uint16_t(port), config.value("lan", false)});
    }
    if (routes.size() > 32) throw std::runtime_error("Too many OSC routes");
    if (routes.empty()) return;
    service_ = std::make_unique<Service>(Sink{this, deliver}, authorized_);
    for (const auto &route : routes)
        if (!service_->add(route)) throw std::runtime_error("Cannot stage OSC route");
    engine_ = engine;
    engine_generation_ = app_engine_native(engine_).meter().generation;
    // Busy ports report unavailable status; they never refuse the patch.
    (void)service_->start();
}
void Host::stop() noexcept {
    if (service_) {
        service_->stop();
        service_.reset();
    }
    engine_ = nullptr;
    engine_generation_ = 0;
}
std::vector<RouteState> Host::states() const { return service_ ? service_->states() : std::vector<RouteState>{}; }
} // namespace wirescape::osc
