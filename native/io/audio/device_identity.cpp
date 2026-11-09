// SPDX-License-Identifier: GPL-3.0-only
// Pure output-identity rules, shared by the PortAudio service and its link-time fakes.
#include "portaudio_service.h"
namespace wirescape::audio {
std::vector<std::string> device_ids(const std::vector<RawDevice> &devices) {
    std::vector<std::string> ids;
    ids.reserve(devices.size());
    for (size_t i = 0; i < devices.size(); ++i) {
        size_t seen = 1;
        for (size_t j = 0; j < i; ++j)
            seen += devices[j].name == devices[i].name && devices[j].host_api == devices[i].host_api;
        auto id = devices[i].host_api + "|" + devices[i].name;
        if (seen > 1) id += "#" + std::to_string(seen);
        ids.push_back(std::move(id));
    }
    return ids;
}
int resolve_device(const std::vector<Device> &devices, const std::string &selection) {
    for (size_t i = 0; i < devices.size(); ++i)
        if (devices[i].id == selection) return int(i);
    int found = -1;
    for (size_t i = 0; i < devices.size(); ++i)
        if (devices[i].name == selection) {
            if (found >= 0) return -1;
            found = int(i);
        }
    return found;
}
} // namespace wirescape::audio
