// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <nlohmann/json.hpp>
#include <condition_variable>
#include <deque>
#include <functional>
#include <istream>
#include <mutex>
#include <ostream>
#include <thread>
namespace wirescape::sidecar {
using Json = nlohmann::json;
constexpr size_t LineLimit = 2097152, PendingLimit = 256;
struct Request { Json value; std::string error; };
Json strict_json(const std::string&);
// Reader never owns engine state. Writer alone serializes stdout.
class Transport {
    std::istream& input_; std::ostream& output_;
    std::mutex mutex_; std::condition_variable ready_, writable_;
    std::deque<Request> commands_; std::deque<Json> replies_;
    Json status_, telemetry_;
    bool eof_ = false, finished_ = false;
    std::thread reader_, writer_;
    void read(); void write();
public:
    Transport(std::istream&, std::ostream&);
    ~Transport();
    void start();
    bool next(Request&, std::chrono::milliseconds);
    bool eof();
    size_t pending();
    void send(Json, bool coalesce = false);
    void finish();
};
}
