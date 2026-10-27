#pragma once
#include <algorithm>
#include <cstdint>
namespace wirescape {
// Sample-count linear transition. Initial/reset settings snap; repeated targets
// do not postpone completion. No allocation, clocks, locks or block dependence.
class LinearRamp {
    double current_, target_, step_ = 0;
    uint32_t duration_ = 240, remaining_ = 0;
    bool rendered_ = false;
public:
    explicit LinearRamp(double initial) : current_(initial), target_(initial) {}
    void prepare(double rate) { duration_ = std::max(1u, static_cast<uint32_t>(rate * .005)); reset(); }
    void reset() { current_ = target_; remaining_ = 0; rendered_ = false; }
    void jump(double value) { current_ = target_ = value; remaining_ = 0; }
    void set(double value) {
        if (value == target_) return;
        target_ = value;
        if (!rendered_) { current_ = value; remaining_ = 0; return; }
        remaining_ = duration_; step_ = (target_ - current_) / remaining_;
    }
    double value() const { return current_; }
    double next() {
        rendered_ = true;
        if (remaining_ && !--remaining_) current_ = target_;
        else if (remaining_) current_ += step_;
        return current_;
    }
};
}
