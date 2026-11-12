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
        const uint32_t node = module.at("id").get<uint32_t>();
        const bool lan = config.value("lan", false);
        // Absent `enabled` means enabled; only a module whose eight rows are all
        // explicitly disabled ("Clear") is left unbound and reported as disabled.
        uint32_t disabled = 0;
        if (config.contains("rows") && config.at("rows").is_array())
            for (const auto &row : config.at("rows"))
                if (row.is_object() && row.contains("enabled") && row.at("enabled") == false) ++disabled;
        if (disabled >= 8) {
            RouteState state;
            state.node = node;
            state.port = uint16_t(port);
            state.lan_requested = lan;
            state.state = "disabled";
            disabled_.push_back(state);
        } else routes.push_back({node, uint16_t(port), lan});
    }
    if (routes.size() + disabled_.size() > 32) throw std::runtime_error("Too many OSC routes");
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
    disabled_.clear();
    engine_ = nullptr;
    engine_generation_ = 0;
}
std::vector<RouteState> Host::states() const {
    auto out = service_ ? service_->states() : std::vector<RouteState>{};
    out.insert(out.end(), disabled_.begin(), disabled_.end());
    return out;
}
} // namespace wirescape::osc
