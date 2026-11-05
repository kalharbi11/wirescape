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
 CHECK(runtime_module_config("midi.in",nlohmann::json::parse(R"({"endpoint":"Controller","system_name":"system"})")).empty());
 bool malformed=false;try{encode("{");}catch(const nlohmann::json::parse_error&){malformed=true;}CHECK(malformed);
 auto osc = runtime_module_config(
     "osc.in",
     nlohmann::json::parse(
         R"({"port":9001,"lan":false,"rows":[{"pattern":"/pico1/knob/*","argument":0,"kind":"cv","stale":"zero","timeout_ms":250}]})"));
 wirescape::osc::Configuration osc_config;
 CHECK(wirescape::osc::configuration(osc.data(), static_cast<uint32_t>(osc.size()), osc_config));
 CHECK(osc_config.port == 9001 && !osc_config.lan && osc_config.rows[0].stale == 1 &&
       osc_config.rows[0].timeout_ms == 250 && osc_config.statistics == nullptr);
 for (const char *bad : {R"({"port":0})", R"({"lan":1})", R"({"statistics":1234})", R"({"rows":[{"kind":"message"}]})",
                         R"({"rows":[{"argument":32}]})", R"({"rows":[{"input_min":1,"input_max":1}]})"}) {
     bool rejected = false;
     try {
         runtime_module_config("osc.in", nlohmann::json::parse(bad));
     } catch (const std::exception &) {
         rejected = true;
     }
     CHECK(rejected);
 }
 CHECK(runtime_module_config("monitor", nlohmann::json::parse(R"({"prefix":"Pico"})")).empty());
 std::puts("PASS bounded typed MIDI configuration and invalid row rejection");
} catch(const std::exception& error) {
 std::fprintf(stderr,"FAIL runtime module configuration: %s\n",error.what());return EXIT_FAILURE;
} catch(...) {
 std::fputs("FAIL runtime module configuration: unknown exception\n",stderr);return EXIT_FAILURE;
}
