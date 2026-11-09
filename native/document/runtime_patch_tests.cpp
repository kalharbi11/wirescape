// SPDX-License-Identifier: GPL-3.0-only
#include "runtime_patch.h"
#include "../engine2/engine2.h"
#include <filesystem>
#include <cstring>
#include "../tests2/allocation_watch.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <cmath>
using Json=nlohmann::json;
using Owner=std::unique_ptr<AppEngine,decltype(&app_engine_destroy)>;
static void check(bool value,const char*why){if(!value)throw std::runtime_error(why);}

static void saved_lfo_patch(const char *demo, AppEngine *catalog, const ws_engine_config &cfg) {
    const auto path = std::filesystem::path(demo).parent_path().parent_path() / "document/ui_starters.json";
    std::ifstream input(path);
    check(bool(input), "open existing saved starters");
    Json starters;
    input >> starters;
    Json patch;
    for (const auto &entry : starters)
        if (entry.at("id") == "qt-lfo-filter") patch = entry.at("patch");
    check(!patch.is_null(), "find existing pre-phase LFO starter");
    std::string error;
    Owner engine(wirescape::document::load_runtime_patch(patch.dump(), catalog, cfg, error), app_engine_destroy);
    check(bool(engine), "existing saved LFO patch loads without migration");
    // Frozen free-run renderer from Lfo::process at pre-change main 7c7d713.
    // Use a same-build reference because libm sin bits may differ across platforms.
    const auto &params = patch.at("modules").at(4).at("parameters");
    check(patch.at("modules").at(4).at("module") == "essentials.lfo" &&
          patch.at("modules").at(4).at("id") == 5 && params.empty(), "saved LFO identity/defaults");
    const float rate = params.value("rate", 1.f), depth = params.value("depth", 1.f);
    const float offset = params.value("offset", 0.f);
    constexpr double tau = 6.283185307179586476925286766559;
    double phase = 0;
    float audio[128];
    auto &native = app_engine_native(engine.get());
    check(native.meter().feedback_edges == 0, "saved patch has no feedback chunking");
    for (uint32_t block = 0; block < 750; ++block) {
        check(app_engine_render(engine.get(), audio, 128) == WS_OK, "render saved LFO patch");
        for (uint32_t i = 0; i < 128; ++i) {
            const float expected = static_cast<float>(std::sin(phase) * depth + offset);
            const float actual = native.output_sample(5, 0, 0, i);
            uint32_t before = 0, after = 0;
            std::memcpy(&before, &expected, sizeof(before));
            std::memcpy(&after, &actual, sizeof(after));
            check(before == after, "saved LFO control bit-identical to pre-phase renderer");
            phase += tau * rate / cfg.sample_rate;
            if (phase >= tau) phase = std::fmod(phase, tau);
        }
    }
    std::cout << "saved qt-lfo-filter loads unchanged; 96000 control samples bit-identical to pre-phase renderer\\n";
}

int main(int argc,char**argv){try{
 check(argc==2,"demo argument");std::ifstream input(argv[1]);std::string bytes((std::istreambuf_iterator<char>(input)),{});check(!bytes.empty(),"read demo");
 ws_engine_config cfg{sizeof(cfg),48000,128,64,2048};Owner catalog(app_engine_create(&cfg),app_engine_destroy);std::string error;
 saved_lfo_patch(argv[1], catalog.get(), cfg);
 auto load=[&](const std::string&source){return Owner(wirescape::document::load_runtime_patch(source,catalog.get(),cfg,error),app_engine_destroy);};
 auto v1=load(bytes);check(bool(v1),"v1 loader");auto source=Json::parse(bytes),patch=source;patch["version"]=2;patch["settings"]={{"max_channels",16},{"feedback_block",1},{"seed",1u}};patch["outputs"]=Json::array({{{"module",source["outputNode"]},{"port","audio"},{"bus_channel",0}}});
 for(auto&m:patch["modules"]){auto type=m["type"].get<std::string>();m["module"]=type;m["version"]=1;m["config"]=Json::object();m["parameters"]=Json::object();for(auto it=m["values"].begin();it!=m["values"].end();++it)m["parameters"][app_engine_parameter_id(catalog.get(),type.c_str(),uint32_t(std::stoul(it.key())))]=it.value();m.erase("type");m.erase("values");}
 for(auto&c:patch["connections"]){c["from"]={{"module",c["fromNode"]},{"port",c["fromPort"]}};c["to"]={{"module",c["toNode"]},{"port",c["toPort"]}};c["kind"]="signal";for(auto key:{"fromNode","fromPort","toNode","toPort"})c.erase(key);}
 auto v2=load(patch.dump());check(bool(v2),"v2 loader");float a[128],b[128];
 for(int block=0;block<100;++block){int ra,rb;size_t count;{allocation_watch::Scope watch;ra=app_engine_render(v1.get(),a,128);rb=app_engine_render(v2.get(),b,128);count=watch.allocations();}check(ra==WS_OK&&rb==WS_OK&&count==0,"loader callback allocation-free");for(int i=0;i<128;++i)check(a[i]==b[i]&&std::isfinite(a[i]),"v1/v2 graph equivalence");}
 check(!load("{\"format\":\"wirescape.native.patch\",\"format\":\"wirescape.native.patch\"}"),"duplicate keys rejected");
 auto bad=patch;bad["version"]=99;check(!load(bad.dump()),"future version refused");bad=patch;bad["modules"][0]["parameters"]={{"unknown",1}};check(!load(bad.dump()),"unknown parameter refused");
 bad=patch;bad["modules"][0]["inactive"]=true;check(!load(bad.dump()),"inactive module refused");bad=patch;bad["modules"][0]["id"]=1.5;check(!load(bad.dump()),"fractional ID refused");
 bad=patch;bad["settings"]["max_channels"]=17;check(!load(bad.dump()),"channel limit");bad=patch;bad["modules"][0]["config"]={{"unknown",1}};check(!load(bad.dump()),"unsupported nonempty config fails closed");
 bad=patch;bad["settings"]["sample_rate"]="48000";check(!load(bad.dump()),"sample rate type");
 auto ccpatch = Json::parse(R"({"format":"wirescape.native.patch","version":2,"settings":{},
  "modules":[{"id":200,"module":"midi.cc_to_cv","version":1,
    "config":{"rows":[{"channel":1,"controller":21,"bipolar":true,"min":3.5,"max":-2.25}]}},
    {"id":201,"module":"essentials.output","version":1}],
  "connections":[],"outputs":[{"module":201,"port":"audio","bus_channel":0}],
  "ui":{"opaque":{"keep":"lossless"}}})");
 auto ccfirst = load(ccpatch.dump());
 check(bool(ccfirst), "CC native range patch load");
 const auto saved = ccpatch.dump();
 auto reloaded = Json::parse(saved);
 check(reloaded["modules"][0]["config"]["rows"][0]["min"] == 3.5 &&
           reloaded["modules"][0]["config"]["rows"][0]["max"] == -2.25 &&
           reloaded["modules"][0]["config"]["rows"][0]["bipolar"] == true &&
           reloaded["ui"]["opaque"]["keep"] == "lossless",
       "CC JSON round-trip preserves rows/unknown UI");
 auto ccsecond = load(reloaded.dump());
 check(bool(ccsecond), "CC native range patch reload");
 bad = reloaded;
 bad["modules"][0]["config"]["rows"][0]["min"] = "3.5";
 check(!load(bad.dump()), "CC nonnumber minimum refused");
 bad = reloaded;
 bad["modules"][0]["config"]["rows"][0]["max"] = "not finite";
 check(!load(bad.dump()), "CC nonnumber maximum refused");
 float stable[128]{};
 {
     allocation_watch::Scope scope;
     check(app_engine_render(ccfirst.get(), stable, 128) == WS_OK &&
               app_engine_render(ccsecond.get(), stable, 128) == WS_OK && scope.allocations() == 0,
           "refusal leaves previously loaded graphs usable");
 }

 check(!load(std::string(wirescape::document::RuntimePatchLimit+1,' ')),"document size limit");check(!load("[]"),"root type");
 std::cout<<"runtime loader v1/v2, validation, noalloc pass\n";
 }catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
