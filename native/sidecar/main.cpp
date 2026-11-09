// SPDX-License-Identifier: GPL-3.0-only
#include "session.h"
#include <csignal>
#include <iostream>
int main(int argc, char **argv) {
#ifndef _WIN32
    std::signal(SIGPIPE, SIG_IGN);
#endif
    try {
        auto options = wirescape::sidecar::parse_options(argc, argv);
        wirescape::sidecar::Session session(std::move(options));
        wirescape::sidecar::Transport transport;
        transport.send(session.hello());
        transport.start();
        transport.wait_hello(); // strictly before render/pacing and device startup
        try {
        session.start();
        auto status = std::chrono::steady_clock::now(), telemetry = status, devices = status;
        for (;;) {
            wirescape::sidecar::Request request;
            if (transport.next(request, std::chrono::milliseconds(5))) {
                auto reply = session.reply(request);
                transport.send(reply);
                transport.complete(request);
                if (reply.value("ok", false) && request.value.contains("type") &&
                    (request.value["type"] == "selectAudioDevice" || request.value["type"] == "setMidiEnabled"))
                    transport.send(session.devices());
            }
            if (transport.eof()) break;
            auto now = std::chrono::steady_clock::now();
            if (now - status >= std::chrono::milliseconds(100)) {
                auto snapshot = session.status();
                const bool lost = snapshot["status"]["audio"]["state"] == "device-lost";
                transport.send(std::move(snapshot), !lost); // Do not coalesce away the loss notification.
                status = now;
            }
            if (now - telemetry >= std::chrono::milliseconds(34)) {
                transport.send(session.telemetry(), true);
                telemetry = now;
            }
            if (now - devices >= std::chrono::milliseconds(100)) {
                if (session.poll()) transport.send(session.devices());
                devices = now;
            }
        }
        session.shutdown();
        } catch (const std::exception &e) {
            // Report the original control error before Transport's destructor
            // waits for stdin EOF and its shutdown watchdog can terminate us.
            std::cerr << "wirescape-engine control failure: " << e.what() << '\n';
            std::cerr.flush();
            throw;
        }
        transport.finish();
        return 0;
    } catch (const std::exception &e) {
        std::cerr << "wirescape-engine: " << e.what() << '\n';
        return 1;
    }
}
