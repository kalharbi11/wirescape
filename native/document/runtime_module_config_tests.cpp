// SPDX-License-Identifier: GPL-3.0-only
#include "runtime_module_config.h"
#include <cstdio>
#include <cstdlib>
using namespace wirescape::document;
#define CHECK(x) do{if(!(x))std::abort();}while(0)
int main() try {
 auto encode=[](const char*s){return runtime_module_config("midi.cc_to_cv",nlohmann::json::parse(s));};
 auto bytes=encode(R"({"rows":[{"channel":16,"controller":31,"fourteen_bit":true,"bipolar":true,"smoothing_ms":0}]})");
 wirescape::midi::CcConfiguration config;CHECK(wirescape::midi::cc_configuration(bytes.data(),static_cast<uint32_t>(bytes.size()),config));
 CHECK(config.rows[0].channel==16&&config.rows[0].controller==31&&config.rows[0].fourteen_bit&&config.rows[0].bipolar&&config.rows[0].smoothing_ms==0);CHECK(config.rows[1].controller==2&&config.rows[1].smoothing_ms==5);
 for(const char*s:{R"({"rows":[{"channel":17}]})",R"({"rows":[{"controller":32,"fourteen_bit":true}]})",R"({"rows":[{"controller":1.5}]})",R"({"rows":[{"fourteen_bit":1}]})",R"({"rows":[{"smoothing_ms":-1}]})",R"({"rows":[{"unknown":1}]})",R"({"rows":[{},{},{},{},{},{},{},{},{}]})"}){bool rejected=false;try{encode(s);}catch(const std::exception&){rejected=true;}CHECK(rejected);}

 auto ranged = encode(
     R"({"rows":[{"controller":21,"bipolar":true,"min":4.0,"max":-2.0},{"max":0.5},{"min":3.0},{"min":0.25,"max":0.25}]})");
 wirescape::midi::CcConfiguration range;
 CHECK(wirescape::midi::cc_configuration(ranged.data(), static_cast<uint32_t>(ranged.size()), range));
 CHECK(range.version == 2 && range.rows[0].minimum() == 4 && range.rows[0].maximum() == -2);
 CHECK(range.rows[1].minimum() == 0 && range.rows[1].maximum() == 0.5);
 CHECK(range.rows[2].minimum() == 3 && range.rows[2].maximum() == 1);
 CHECK(range.rows[3].minimum() == 0.25 && range.rows[3].maximum() == 0.25);
 wirescape::midi::CcConfigurationV1 old;
 old.rows[0].bipolar = 1;
 CHECK(wirescape::midi::cc_configuration(reinterpret_cast<const uint8_t *>(&old), sizeof(old), range));
 CHECK(range.rows[0].minimum() == -1 && range.rows[0].maximum() == 1);
 for (const char *s : {R"({"rows":[{"min":"0"}]})", R"({"rows":[{"max":false}]})", R"({"rows":[{"min":null}]})",
                       R"({"rows":[{"max":1e999}]})", R"({"rows":[{"min":-1e999}]})"}) {
     bool rejected = false;
     try {
         encode(s);
     } catch (const std::exception &) {
         rejected = true;
     }
     CHECK(rejected);
 }
 CHECK(runtime_module_config("midi.in",nlohmann::json::parse(R"({"endpoint":"Controller","system_name":"system"})")).empty());
 bool malformed=false;try{encode("{");}catch(const nlohmann::json::parse_error&){malformed=true;}CHECK(malformed);
 auto groove = [](const char *s) { return runtime_module_config("sequencing.groove", nlohmann::json::parse(s)); };
 const auto groove_old = groove(
     R"({"version":1,"bpm":120,"tracks":[{"steps":[{"kind":"note","key":60,"velocity":0.5},{"kind":"tie"},{"kind":"rest"}]}]})");
 groove::Config g;
 CHECK(groove::decode(groove_old.data(), static_cast<uint32_t>(groove_old.size()), g));
 CHECK(g.track_count == 1 && g.tracks[0].steps[0].kind == groove::Note && g.tracks[0].steps[1].kind == groove::Tie &&
       g.tracks[0].steps[3].kind == groove::Rest);
 CHECK(g.tracks[0].steps[0].mod1 == 0 && g.tracks[0].steps[0].mod2 == 0);
 const auto expanded = groove(
     R"({"version":1,"bpm":123,"tracks":[{"mute":true,"armed":false,"steps":[{"kind":"note","key":71,"velocity":0.8,"mod1":0.25,"mod2":0.75}]},{"steps":[{"kind":"note","key":48,"velocity":1,"mod1":1,"mod2":0}]}]})");
 CHECK(groove::decode(expanded.data(), static_cast<uint32_t>(expanded.size()), g));
 CHECK(g.track_count == 2 && g.tracks[0].armed == 0 && g.tracks[0].mute == 1 && g.tracks[0].steps[0].mod1 == .25f &&
       g.tracks[0].steps[0].mod2 == .75f);
 CHECK(g.tracks[0].steps[31].kind == groove::Rest && g.tracks[1].steps[31].kind == groove::Rest);
 for (const char *bad :
      {R"({"version":1,"bpm":120,"tracks":[{"steps":[{"kind":"rest","unexpected":2}]}]})",
       R"({"version":1,"bpm":120,"tracks":[{"steps":[{"kind":"note","key":60,"velocity":1,"mod1":2}]}]})",
       R"({"version":1,"bpm":120,"tracks":[{"armed":1,"steps":[{"kind":"rest"}]}]})",
       R"({"version":1,"bpm":120,"tracks":[{"steps":[{"kind":"rest","key":60}]}]})",
       R"({"version":1,"bpm":120,"tracks":[]})"}) {
     bool rejected = false;
     try {
         groove(bad);
     } catch (const std::exception &) {
         rejected = true;
     }
     CHECK(rejected);
 }
 const auto v2 = nlohmann::json::parse(
     R"({"version":2,"bpm":120,"tracks":[{"length":16,"steps":[{"kind":"note","key":64,"velocity":0.8}]}]})");
 auto accepted_groove = [&](const nlohmann::json &doc) {
     const auto raw = runtime_module_config("sequencing.groove", doc);
     groove::Config out;
     CHECK(groove::decode(raw.data(), static_cast<uint32_t>(raw.size()), out));
     return out;
 };
 auto refused_groove = [&](const nlohmann::json &doc) {
     bool refused = false;
     try {
         runtime_module_config("sequencing.groove", doc);
     } catch (const std::exception &) {
         refused = true;
     }
     CHECK(refused);
 };
 CHECK(accepted_groove(v2).tracks[0].steps[0].key_count == 1);
 for (const auto &notes :
      {nlohmann::json::array({64}), nlohmann::json::array({64, 60, 67}), nlohmann::json::array({64, 60, 62, 67, 71})}) {
     auto doc = v2;
     doc["tracks"][0]["steps"][0]["keys"] = notes;
     const auto decoded = accepted_groove(doc);
     CHECK(decoded.tracks[0].steps[0].key_count == notes.size());
     for (size_t i = 0; i < notes.size(); ++i) CHECK(decoded.tracks[0].steps[0].keys[i] == notes[i].get<int>());
 }
 for (const auto &notes :
      {nlohmann::json::array(), nlohmann::json::array({64, 60, 62, 67, 71, 72}), nlohmann::json::array({64, -1}),
       nlohmann::json::array({64, 128}), nlohmann::json::array({64, 60.5}), nlohmann::json::array({64, "60"}),
       nlohmann::json::array({64, true}), nlohmann::json::array({60, 64}), nlohmann::json::array({64, 64}),
       nlohmann::json::array({64, 60, 60}), nlohmann::json::array({64, 67, 60})}) {
     auto doc = v2;
     doc["tracks"][0]["steps"][0]["keys"] = notes;
     refused_groove(doc);
 }
 for (const char *kind : {"rest", "tie"}) {
     auto doc = v2;
     doc["tracks"][0]["steps"][0] = {{"kind", kind}, {"keys", nlohmann::json::array({64})}};
     refused_groove(doc);
 }
 for (const auto &speed : {1, 2, 4, 8}) {
     auto doc = v2;
     doc["tracks"][0]["div"] = speed;
     CHECK(accepted_groove(doc).tracks[0].div == static_cast<uint32_t>(speed));
 }
 CHECK(accepted_groove(v2).tracks[0].div == 1);
 for (const auto &speed : {nlohmann::json(0), nlohmann::json(3), nlohmann::json(16), nlohmann::json(-2),
                           nlohmann::json(2.0), nlohmann::json("2"), nlohmann::json(true), nlohmann::json(nullptr)}) {
     auto doc = v2;
     doc["tracks"][0]["div"] = speed;
     refused_groove(doc);
 }
 {
     auto doc = nlohmann::json::parse(
         R"({"version":1,"bpm":120,"tracks":[{"steps":[{"kind":"note","key":64,"velocity":0.8}]}]})");
     doc["tracks"][0]["div"] = 2;
     refused_groove(doc);
     doc["tracks"][0].erase("div");
     doc["tracks"][0]["steps"][0]["keys"] = nlohmann::json::array({64});
     refused_groove(doc);
 }
 std::puts("PASS bounded typed MIDI and Groove configurations and invalid row rejection");
} catch(const std::exception& error) {
 std::fprintf(stderr,"FAIL runtime module configuration: %s\n",error.what());return EXIT_FAILURE;
} catch(...) {
 std::fputs("FAIL runtime module configuration: unknown exception\n",stderr);return EXIT_FAILURE;
}
