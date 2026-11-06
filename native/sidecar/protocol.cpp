// SPDX-License-Identifier: GPL-3.0-only
#include "protocol.h"
#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <set>
#include <stdexcept>
namespace wirescape::sidecar {
Json strict_json(const std::string &bytes) {
    if (bytes.size() > LineLimit) throw std::runtime_error("limit");
    if (bytes.size() >= 3 && bytes.compare(0, 3, "\xef\xbb\xbf") == 0) throw std::runtime_error("invalid");
    std::vector<std::set<std::string>> keys;
    auto callback = [&](int depth, Json::parse_event_t event, Json &value) {
        if ((event == Json::parse_event_t::object_start || event == Json::parse_event_t::array_start) && depth >= 64)
            throw std::runtime_error("invalid");
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
Transport::Transport(std::istream &in, std::ostream &out) : input_(in), output_(out) {}
Transport::~Transport() { finish(); }
void Transport::start() {
    watchdog_ = std::thread([this] { monitor(); });
    writer_ = std::thread([this] { write(); });
    reader_ = std::thread([this] { read(); });
}
void Transport::wait_hello() {
    std::unique_lock<std::mutex> lock(mutex_);
    writable_.wait(lock, [this] { return hello_written_; });
}
void Transport::read() {
    std::string bytes;
    bytes.reserve(4096);
    bool oversized = false;
    char c;
    while (input_.get(c)) {
        if (c != '\n') {
            if (bytes.size() < LineLimit) bytes.push_back(c);
            else oversized = true;
            continue;
        }
        Request request;
        if (oversized) request.error = "limit";
        else try {
                request.value = strict_json(bytes);
            } catch (...) {
                request.error = "invalid";
            }
        bytes.clear();
        oversized = false;
        std::unique_lock<std::mutex> lock(mutex_);
        // Busy markers preserve receive order and cannot grow memory unboundedly.
        if (commands_.size() >= PendingLimit) {
            if (request.error.empty()) request.error = "busy";
            ready_.wait(lock, [&] { return commands_.size() < PendingLimit; });
        }
        if (request.error.empty() && request.value.contains("id") && request.value["id"].is_string()) {
            auto id = request.value["id"].get<std::string>();
            if (id.size() <= 64) {
                if (outstanding_.count(id)) request.error = "duplicate";
                else {
                    outstanding_.insert(id);
                    request.tracked_id = id;
                }
            }
        }
        commands_.push_back(std::move(request));
        ready_.notify_all();
    }
    std::lock_guard<std::mutex> lock(mutex_);
    eof_ = true;
    ready_.notify_all();
}
bool Transport::next(Request &out, std::chrono::milliseconds timeout) {
    std::unique_lock<std::mutex> lock(mutex_);
    ready_.wait_for(lock, timeout, [&] { return !commands_.empty() || eof_; });
    if (commands_.empty()) return false;
    out = std::move(commands_.front());
    commands_.pop_front();
    ready_.notify_all();
    return true;
}
bool Transport::eof() {
    std::lock_guard<std::mutex> lock(mutex_);
    return eof_ && commands_.empty();
}
void Transport::complete(const Request &r) {
    if (r.tracked_id.empty()) return;
    std::lock_guard<std::mutex> lock(mutex_);
    outstanding_.erase(r.tracked_id);
}
size_t Transport::pending() {
    std::lock_guard<std::mutex> lock(mutex_);
    return commands_.size();
}
void Transport::send(Json value, bool coalesce) {
    std::unique_lock<std::mutex> lock(mutex_);
    if (coalesce) {
        (value.value("type", "") == "status" ? status_ : telemetry_) = std::move(value);
    } else {
        writable_.wait(lock, [&] { return replies_.size() < PendingLimit; });
        replies_.push_back(std::move(value));
    }
    writable_.notify_all();
}
void Transport::write() {
    try {
        for (;;) {
            Json value;
            {
                std::unique_lock<std::mutex> lock(mutex_);
                writable_.wait(lock, [&] {
                    return finished_ || !replies_.empty() || !status_.is_null() || !telemetry_.is_null();
                });
                if (!replies_.empty()) {
                    value = std::move(replies_.front());
                    replies_.pop_front();
                } else if (!status_.is_null()) {
                    value = std::move(status_);
                    status_ = nullptr;
                } else if (!telemetry_.is_null()) {
                    value = std::move(telemetry_);
                    telemetry_ = nullptr;
                } else if (finished_) return;
                writable_.notify_all();
            }
            {
                std::lock_guard<std::mutex> lock(mutex_);
                writing_ = true;
                write_deadline_ = std::chrono::steady_clock::now() + std::chrono::seconds(5);
                writable_.notify_all();
            }
            const bool initial_hello = value.is_object() && value.value("type", std::string{}) == "hello";
            auto bytes = value.dump();
            if (bytes.size() > LineLimit) throw std::runtime_error("Reply exceeds protocol limit");
            output_ << bytes << '\n';
            output_.flush();
            if (!output_) throw std::runtime_error("Protocol output unavailable");
            {
                std::lock_guard<std::mutex> lock(mutex_);
                writing_ = false;
                if (initial_hello) hello_written_ = true;
                writable_.notify_all();
            }
        }
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        std::cerr.flush();
        std::_Exit(3);
    }
}
void Transport::monitor() {
    std::unique_lock<std::mutex> lock(mutex_);
    while (!joined_) {
        if (!writing_ && !closing_) {
            writable_.wait(lock);
            continue;
        }
        auto deadline = writing_ ? write_deadline_ : close_deadline_;
        if (closing_) deadline = std::min(deadline, close_deadline_);
        if (writable_.wait_until(lock, deadline) == std::cv_status::timeout &&
            ((writing_ && std::chrono::steady_clock::now() >= write_deadline_) ||
             (closing_ && std::chrono::steady_clock::now() >= close_deadline_))) {
            std::cerr << "Protocol pipe unavailable or shutdown timed out" << '\n';
            std::cerr.flush();
            std::_Exit(3);
        }
    }
}
void Transport::finish() {
    if (!reader_.joinable() && !writer_.joinable() && !watchdog_.joinable()) return;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        closing_ = true;
        close_deadline_ = std::chrono::steady_clock::now() + std::chrono::seconds(5);
        writable_.notify_all();
    }
    if (reader_.joinable()) reader_.join();
    {
        std::lock_guard<std::mutex> lock(mutex_);
        finished_ = true;
        writable_.notify_all();
    }
    if (writer_.joinable()) writer_.join();
    {
        std::lock_guard<std::mutex> lock(mutex_);
        joined_ = true;
        writable_.notify_all();
    }
    if (watchdog_.joinable()) watchdog_.join();
}
} // namespace wirescape::sidecar
