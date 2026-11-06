// SPDX-License-Identifier: GPL-3.0-only
#include "protocol.h"
#include <set>
#include <stdexcept>
namespace wirescape::sidecar {
Json strict_json(const std::string& bytes) {
    if (bytes.size() > LineLimit) throw std::runtime_error("limit");
    if (bytes.size() >= 3 && bytes.compare(0, 3, "\xef\xbb\xbf") == 0) throw std::runtime_error("invalid");
    std::vector<std::set<std::string>> keys;
    auto callback = [&](int depth, Json::parse_event_t event, Json& value) {
        if (depth > 64) throw std::runtime_error("invalid");
        if (event == Json::parse_event_t::object_start) keys.emplace_back();
        if (event == Json::parse_event_t::key && !keys.back().insert(value.get<std::string>()).second)
            throw std::runtime_error("invalid");
        if (event == Json::parse_event_t::object_end) keys.pop_back();
        return true;
    };
    auto result = Json::parse(bytes, callback);
    if (!result.is_object()) throw std::runtime_error("invalid");
    return result;
}
Transport::Transport(std::istream& in, std::ostream& out) : input_(in), output_(out) {}
Transport::~Transport() { finish(); }
void Transport::start() { writer_ = std::thread([this]{write();}); reader_ = std::thread([this]{read();}); }
void Transport::read() {
    std::string bytes; bytes.reserve(4096); bool oversized = false; char c;
    while (input_.get(c)) {
        if (c != '\n') {
            if (bytes.size() < LineLimit) bytes.push_back(c); else oversized = true;
            continue;
        }
        Request request;
        if (oversized) request.error = "limit";
        else try { request.value = strict_json(bytes); } catch (...) { request.error = "invalid"; }
        bytes.clear(); oversized = false;
        std::unique_lock<std::mutex> lock(mutex_);
        // Busy markers preserve receive order and cannot grow memory unboundedly.
        if (commands_.size() >= PendingLimit) {
            if(request.error.empty())request.error = "busy";
            ready_.wait(lock, [&]{return commands_.size() < PendingLimit;});
        }
        commands_.push_back(std::move(request)); ready_.notify_all();
    }
    std::lock_guard<std::mutex> lock(mutex_); eof_ = true; ready_.notify_all();
}
bool Transport::next(Request& out, std::chrono::milliseconds timeout) {
    std::unique_lock<std::mutex> lock(mutex_);
    ready_.wait_for(lock, timeout, [&]{return !commands_.empty() || eof_;});
    if (commands_.empty()) return false;
    out = std::move(commands_.front()); commands_.pop_front(); ready_.notify_all(); return true;
}
bool Transport::eof() { std::lock_guard<std::mutex> lock(mutex_); return eof_ && commands_.empty(); }
size_t Transport::pending(){std::lock_guard<std::mutex> lock(mutex_);return commands_.size();}
void Transport::send(Json value, bool coalesce) {
    std::unique_lock<std::mutex> lock(mutex_);
    if (coalesce) {
        (value.value("type", "") == "status" ? status_ : telemetry_) = std::move(value);
    } else {
        writable_.wait(lock, [&]{return replies_.size() < PendingLimit;});
        replies_.push_back(std::move(value));
    }
    writable_.notify_all();
}
void Transport::write() {
    for (;;) {
        Json value;
        {
            std::unique_lock<std::mutex> lock(mutex_);
            writable_.wait(lock, [&]{return finished_ || !replies_.empty() || !status_.is_null() || !telemetry_.is_null();});
            if (!replies_.empty()) { value = std::move(replies_.front()); replies_.pop_front(); }
            else if (!status_.is_null()) { value = std::move(status_); status_ = nullptr; }
            else if (!telemetry_.is_null()) { value = std::move(telemetry_); telemetry_ = nullptr; }
            else if (finished_) return;
            writable_.notify_all();
        }
        auto bytes = value.dump();
        if (bytes.size() > LineLimit) throw std::runtime_error("Reply exceeds protocol limit");
        output_ << bytes << '\n'; output_.flush();
    }
}
void Transport::finish() {
    if (reader_.joinable()) reader_.join();
    { std::lock_guard<std::mutex> lock(mutex_); finished_ = true; writable_.notify_all(); }
    if (writer_.joinable()) writer_.join();
}
}
