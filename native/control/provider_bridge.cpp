#include "provider_bridge.h"
#include <cmath>
#include <limits>

namespace wirescape {
namespace {
constexpr uint64_t billion = 1000000000ull;
bool add_ok(uint64_t a, uint64_t b, uint64_t &sum) noexcept {
    if (b > UINT64_MAX - a) return false;
    sum = a + b;
    return true;
}
}

ProviderBridge::ProviderBridge(ws_engine *engine, Config config) noexcept
    : engine_(engine), config_(config) {
    if (!valid_config(config)) return;
    generation_ = ws_engine_generation(engine_);
    last_forwarded_ = config.epoch_frame;
    status_ = Status::active;
}

bool ProviderBridge::valid_config(Config c) const noexcept {
    if (!engine_ || !c.sample_rate_hz || c.sample_rate_hz > 384000 ||
        !c.max_block_frames || !c.max_per_flush || c.max_per_flush > capacity ||
        c.horizon_frames < c.max_block_frames ||
        c.horizon_frames > UINT64_MAX - c.max_block_frames ||
        c.epoch_frame > UINT64_MAX - c.horizon_frames - c.max_block_frames)
        return false;
    ws_meter meter{sizeof(ws_meter)};
    return ws_engine_get_meter(engine_, &meter) == WS_OK &&
           c.epoch_frame >= meter.frame_position;
}

bool ProviderBridge::convert(uint64_t ns, uint64_t &frame) const noexcept {
    if (ns < config_.epoch_ns) return false;
    const uint64_t delta = ns - config_.epoch_ns;
    const uint64_t seconds = delta / billion;
    if (seconds > UINT64_MAX / config_.sample_rate_hz) return false;
    const uint64_t whole = seconds * config_.sample_rate_hz;
    const uint64_t fractional = ((delta % billion) * config_.sample_rate_hz) / billion;
    uint64_t offset;
    return add_ok(whole, fractional, offset) &&
           add_ok(config_.epoch_frame, offset, frame);
}

void ProviderBridge::insert(Pending item) noexcept {
    std::size_t at = size_;
    while (at && (pending_[at - 1].event.frame > item.event.frame ||
           (pending_[at - 1].event.frame == item.event.frame &&
            pending_[at - 1].serial > item.serial))) {
        pending_[at] = pending_[at - 1];
        --at;
    }
    pending_[at] = item;
    ++size_;
}

void ProviderBridge::stop_generation() noexcept {
    if (status_ != Status::active) return;
    status_ = Status::stopped;
    size_ = 0;
    ++counts_.generation_stops;
}

void ProviderBridge::stop_with_panic() noexcept {
    if (status_ != Status::active) return;
    status_ = Status::stopped;
    size_ = 0;
    ++counts_.loss_panics;
    // Exactly one owner invokes panic. Its generation increment is not an
    // acknowledgement that the render consumer has applied the voice reset.
    ws_engine_panic(engine_);
}

bool ProviderBridge::generation_matches() noexcept {
    if (status_ != Status::active) return false;
    if (ws_engine_generation(engine_) == generation_) return true;
    stop_generation();
    return false;
}

bool ProviderBridge::submit(const Input &input) noexcept {
    if (!generation_matches()) return false;
    if ((input.type != WS_PARAMETER && input.type != WS_NOTE_ON &&
         input.type != WS_NOTE_OFF && input.type != WS_GATE) ||
        !std::isfinite(input.value)) {
        if (input.type == WS_NOTE_OFF || input.type == WS_GATE) stop_with_panic();
        return false;
    }
    uint64_t frame;
    if (!convert(input.timestamp_ns, frame)) {
        if (input.type == WS_NOTE_OFF || input.type == WS_GATE) stop_with_panic();
        return false;
    }
    ws_meter meter{sizeof(ws_meter)};
    if (ws_engine_get_meter(engine_, &meter) != WS_OK) { stop_generation(); return false; }
    if (!generation_matches()) return false;
    uint64_t farthest;
    if (!add_ok(meter.frame_position, config_.horizon_frames, farthest)) {
        stop_with_panic(); return false;
    }
    if (frame > farthest) {
        ++counts_.backpressure;
        // This proof has no retry channel. Losing a release is unsafe.
        if (input.type == WS_NOTE_OFF || input.type == WS_GATE) stop_with_panic();
        return false;
    }
    ws_event event{sizeof(ws_event), frame, input.node_id, input.type,
                   input.parameter_id, input.value, input.source_id,
                   input.note_id, generation_};
    if (input.type == WS_PARAMETER) {
        for (std::size_t i = 0; i < size_; ++i) {
            const auto &old = pending_[i].event;
            if (old.type == WS_PARAMETER && old.node_id == event.node_id &&
                old.parameter_id == event.parameter_id &&
                old.source_id == event.source_id) {
                for (std::size_t j = i + 1; j < size_; ++j) pending_[j - 1] = pending_[j];
                --size_;
                ++counts_.coalesced;
                break;
            }
        }
    }
    if (size_ == capacity || serial_ == UINT64_MAX) {
        stop_with_panic(); return false;
    }
    insert({event, serial_++});
    return true;
}

uint32_t ProviderBridge::flush() noexcept {
    if (!generation_matches()) return 0;
    uint32_t sent = 0, examined = 0;
    while (size_ && examined < config_.max_per_flush) {
        if (!generation_matches()) break;
        ws_meter meter{sizeof(ws_meter)};
        if (ws_engine_get_meter(engine_, &meter) != WS_OK) { stop_generation(); break; }
        uint64_t safe_frame;
        if (!add_ok(meter.frame_position, config_.max_block_frames, safe_frame)) {
            stop_with_panic(); break;
        }
        auto event = pending_[0].event;
        for (std::size_t j = 1; j < size_; ++j) pending_[j - 1] = pending_[j];
        --size_;
        ++examined;
        const uint64_t minimum = forwarded_any_ && last_forwarded_ > safe_frame
                                     ? last_forwarded_ : safe_frame;
        if (event.frame < minimum) {
            if (event.type == WS_PARAMETER) {
                event.frame = minimum;
                ++counts_.late_control;
            } else if (event.type == WS_NOTE_ON) {
                ++counts_.stale_note_on;
                continue;
            } else {
                stop_with_panic();
                break;
            }
        }
        if (!generation_matches()) break;
        const int result = ws_engine_enqueue(engine_, &event);
        if (result == WS_STATE || ws_engine_generation(engine_) != generation_) {
            stop_generation(); break;
        }
        if (result != WS_OK) { stop_with_panic(); break; }
        last_forwarded_ = event.frame;
        forwarded_any_ = true;
        ++counts_.forwarded;
        ++sent;
    }
    return sent;
}

bool ProviderBridge::resume_after_quiescent_barrier(Config config) noexcept {
    if (status_ != Status::stopped || !valid_config(config)) return false;
    config_ = config;
    generation_ = ws_engine_generation(engine_);
    last_forwarded_ = config.epoch_frame;
    forwarded_any_ = false;
    size_ = 0;
    status_ = Status::active;
    return true;
}
} // namespace wirescape
