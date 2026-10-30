// SPDX-License-Identifier: GPL-3.0-only
#include "dsp.h"
#include "../../io/midi/module_test_support.h"
using namespace wirescape::midi;
static void full_endpoint_channel_matrix() {
    CcConfiguration config;
    config.rows[0].fourteen_bit = 1;
    config.rows[0].smoothing_ms = 0;
    MidiRig<CcToCv> r(reinterpret_cast<const uint8_t *>(&config), sizeof(config));
    for (uint32_t source = 1; source <= 16; ++source)
        for (uint32_t channel = 0; channel < 16; ++channel) {
            r.run({midi1(uint8_t(0xb0 | channel), 33, 17, 0, source)});
            REQUIRE(r.sample(0) == float(17. / 16383.));
            r.run({midi1(uint8_t(0xb0 | channel), 1, 100, 0, source)});
            REQUIRE(r.sample(0) == float((100. * 128 + 17) / 16383.));
        }
    for (uint32_t source = 1; source < 16; ++source) r.run({source_choke(source)});
    for (uint32_t source = 17; source < 32; ++source)
        for (uint32_t channel = 0; channel < 16; ++channel) {
            r.run({midi1(uint8_t(0xb0 | channel), 33, 3, 0, source)});
            REQUIRE(r.sample(0) == float(3. / 16383.)); // no retired source's MSB
        }
    r.run({midi1(0xbf, 33, 4, 0, 16)});
    REQUIRE(r.sample(0) == float((100. * 128 + 4) / 16383.)); // survivor preserved
}
int main() {
    full_endpoint_channel_matrix();
    CcConfiguration config; for (auto& row : config.rows) row.smoothing_ms = 0;
    config.rows[0].fourteen_bit = 1; config.rows[0].channel = 1; config.rows[1].bipolar = 1;
    MidiRig<CcToCv> r(reinterpret_cast<const uint8_t*>(&config), sizeof(config));
    r.run({midi1(0xb0, 1, 127, 2), midi1(0xb0, 33, 127, 3), midi1(0xb1, 1, 0, 4), midi1(0xb0, 2, 127, 5)});
    REQUIRE(r.sample(0, 0, 1) == 0 && r.sample(0, 0, 2) == float(16256. / 16383.) && r.sample(0) == 1);
    REQUIRE(r.sample(1) == 1); r.run({midi1(0xb0, 2, 0)}); REQUIRE(r.sample(1) == -1);
    r.run({midi2(0xb0, 2, 0xffffffffu)}); REQUIRE(r.sample(1) == 1);
    MidiRig<CcToCv> identity(reinterpret_cast<const uint8_t*>(&config), sizeof(config));
    identity.run({midi1(0xb0, 1, 127, 0, 1), midi1(0xb0, 33, 127, 1, 2)});
    REQUIRE(identity.sample(0) == float(127. / 16383.)); // never pair MSB from source A with LSB from B
    CcConfiguration smooth; smooth.rows[0].smoothing_ms = 5;
    MidiRig<CcToCv> s(reinterpret_cast<const uint8_t*>(&smooth), sizeof(smooth)); s.run({midi1(0xb0, 1, 127)}); REQUIRE(s.sample(0) > 0 && s.sample(0) < 1);
    for (int i = 0; i < 32; ++i) s.run({}, 256); REQUIRE(s.sample(0, 0, 255) == 1);
    CcConfiguration bad; bad.rows[0].fourteen_bit = 1; bad.rows[0].controller = 64; REQUIRE(!cc_configuration(reinterpret_cast<const uint8_t*>(&bad), sizeof(bad), bad));
    bad = {}; bad.rows[0].smoothing_ms = std::numeric_limits<float>::quiet_NaN(); REQUIRE(!cc_configuration(reinterpret_cast<const uint8_t*>(&bad), sizeof(bad), bad));
}
