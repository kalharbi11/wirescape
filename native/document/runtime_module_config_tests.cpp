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
 std::puts("PASS bounded typed MIDI configuration and invalid row rejection");
} catch(const std::exception& error) {
 std::fprintf(stderr,"FAIL runtime module configuration: %s\n",error.what());return EXIT_FAILURE;
} catch(...) {
 std::fputs("FAIL runtime module configuration: unknown exception\n",stderr);return EXIT_FAILURE;
}
