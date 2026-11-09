// SPDX-License-Identifier: GPL-3.0-only
#include "session.h"
#include <csignal>
#include <iostream>
#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif
int main(int argc, char **argv) {
#ifdef _WIN32
    _setmode(_fileno(stdin), _O_BINARY);
    _setmode(_fileno(stdout), _O_BINARY);
#else
    std::signal(SIGPIPE, SIG_IGN);
#endif
    // cin is tied to cout by default: every read on the reader thread would flush cout,
    // racing the writer thread on the same stream (undefined; it stalls on MSVC).
    std::cin.tie(nullptr);
    try {
        auto options = wirescape::sidecar::parse_options(argc, argv);
        wirescape::sidecar::Session session(std::move(options));
        wirescape::sidecar::Transport transport(std::cin, std::cout);
        transport.send(session.hello());
        transport.start();
        transport.wait_hello(); // strictly before render/pacing and device startup
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
        transport.finish();
        return 0;
    } catch (const std::exception &e) {
        std::cerr << "wirescape-engine: " << e.what() << '\n';
        return 1;
    }
}
