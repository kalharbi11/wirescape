// SPDX-License-Identifier: GPL-3.0-only
#include "../engine2/engine2.h"
#include "../io/midi/module_test_support.h"
#include "../tests2/allocation_watch.h"
#include "runtime_patch.h"
#include <cmath>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>
#include <stdexcept>
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

// Cover the Studio-saved JSON -> native loader seam, not only manually built
// engine2 graphs. Each note must remain audible in a four-channel rotating CV
// patch, including with Notes to CV between MIDI to CV and the consumers.
static void saved_midi_cv_patch(AppEngine *catalog, const ws_engine_config &cfg) {
    const Json base = Json::parse(R"({
      "format":"wirescape.native.patch","version":2,"settings":{},
      "modules":[
        {"id":1,"module":"midi.in","version":1},
        {"id":2,"module":"midi.to_cv","version":1},
        {"id":3,"module":"essentials.vco","version":1},
        {"id":4,"module":"essentials.adsr","version":1},
        {"id":5,"module":"essentials.vca","version":1},
        {"id":6,"module":"essentials.output","version":1}],
      "connections":[
        {"kind":"event","from":{"module":1,"port":"midi"},"to":{"module":2,"port":"midi"}},
        {"kind":"signal","from":{"module":2,"port":"pitch"},"to":{"module":3,"port":"pitch"}},
        {"kind":"signal","from":{"module":2,"port":"gate"},"to":{"module":4,"port":"gate"}},
        {"kind":"signal","from":{"module":3,"port":"audio"},"to":{"module":5,"port":"audio"}},
        {"kind":"signal","from":{"module":4,"port":"envelope"},"to":{"module":5,"port":"gain_cv"}},
        {"kind":"signal","from":{"module":5,"port":"audio"},"to":{"module":6,"port":"audio"}}],
      "outputs":[{"module":6,"port":"audio","bus_channel":0}]
    })");
    for (const bool notes_route : {false, true}) {
        Json patch = base;
        const uint32_t cv = notes_route ? 7u : 2u;
        if (notes_route) {
            patch["modules"].push_back({{"id", 7}, {"module", "essentials.notes_to_cv"}, {"version", 1}});
            patch["connections"][1]["from"]["module"] = 7;
            patch["connections"][2]["from"]["module"] = 7;
            patch["connections"].push_back({{"kind", "event"},
                                            {"from", {{"module", 2}, {"port", "notes"}}},
                                            {"to", {{"module", 7}, {"port", "notes"}}}});
        }
        std::string error;
        Owner loaded(wirescape::document::load_runtime_patch(patch.dump(), catalog, cfg, error), app_engine_destroy);
        check(bool(loaded), "saved MIDI CV graph loads without port conversion");
        auto &graph = app_engine_native(loaded.get());
        // AppEngine reserves producer 1 for MIDI ingress before the loader prepares
        // the graph; registration after prepare is intentionally forbidden.
        constexpr uint32_t producer = 1;
        const uint8_t keys[]{60, 62, 64, 67, 69};
        float samples[128]{};
        for (uint32_t i = 0; i < 5; ++i) {
            const uint32_t lane = i % 4;
            check(graph.enqueue(producer, 1, WS2_DIRECT_PORT, midi1(0x90, keys[i], 110), graph.meter().frame) == WS2_OK,
                  "saved graph accepts MIDI note-on");
            {
                allocation_watch::Scope scope;
                check(app_engine_render(loaded.get(), samples, 128) == WS_OK && scope.allocations() == 0,
                      "saved graph renders without allocations");
            }
            check(graph.output_channels(cv, 0) == 4 && graph.output_channels(3, 0) == 4,
                  "saved graph preserves polyphonic pitch across load");
            check(graph.output_sample(cv, 1, lane, 127) == 1.f, "saved graph gate follows rotating lane");
            check(std::abs(graph.output_sample(cv, 0, lane, 127) - float(keys[i] - 60) / 12.f) < 1e-5f,
                  "saved graph pitch CV follows every key");
            bool audible = false;
            for (float sample : samples) {
                check(std::isfinite(sample), "saved graph finite mono audio");
                audible |= std::abs(sample) > 1e-6f;
            }
            check(audible, "saved pitch and gate reach audible output on every rotating voice");
            check(graph.enqueue(producer, 1, WS2_DIRECT_PORT, midi1(0x80, keys[i], 0), graph.meter().frame) == WS2_OK,
                  "saved graph accepts MIDI note-off");
            check(app_engine_render(loaded.get(), samples, 128) == WS_OK, "saved graph renders note release");
            check(graph.output_sample(cv, 1, lane, 127) == 0.f, "saved graph releases each lane");
        }
        check(graph.meter().faults == 0 && graph.meter().dropped == 0, "saved graph fault-free");
    }
}

int main(int argc,char**argv){try{
 check(argc==2,"demo argument");std::ifstream input(argv[1]);std::string bytes((std::istreambuf_iterator<char>(input)),{});check(!bytes.empty(),"read demo");
 ws_engine_config cfg{sizeof(cfg),48000,128,64,2048};Owner catalog(app_engine_create(&cfg),app_engine_destroy);std::string error;
 saved_lfo_patch(argv[1], catalog.get(), cfg);
 saved_midi_cv_patch(catalog.get(), cfg);
 // Qt's inspection-only legacy formats must never become executable native graphs.
 const auto fixtures = std::filesystem::path(argv[1]).parent_path().parent_path() / "tests/fixtures/legacy_documents";
 for (const char *name : {"patch_v1.json", "patch_v2.json", "patch_v3_unknown.json", "visual_study_v4.json",
                          "future_patch_v99.json", "malformed_patch.json"}) {
     std::ifstream fixture(fixtures / name, std::ios::binary);
     check(bool(fixture), "read original legacy fixture");
     const std::string original((std::istreambuf_iterator<char>(fixture)), {});
     const auto retained = original;
     const auto count = app_engine_module_count(catalog.get());
     Owner rejected(wirescape::document::load_runtime_patch(original, catalog.get(), cfg, error), app_engine_destroy);
     check(!rejected && !error.empty(), "legacy/visual/future/malformed fixture refuses execution with diagnostic");
     check(original == retained && app_engine_module_count(catalog.get()) == count,
           "refusal leaves source bytes and live catalogue unchanged");
 }

 auto load=[&](const std::string&source){return Owner(wirescape::document::load_runtime_patch(source,catalog.get(),cfg,error),app_engine_destroy);};
 auto v1=load(bytes);check(bool(v1),"v1 loader");auto source=Json::parse(bytes),patch=source;patch["version"]=2;patch["settings"]={{"max_channels",16},{"feedback_block",1},{"seed",1u}};patch["outputs"]=Json::array({{{"module",source["outputNode"]},{"port","audio"},{"bus_channel",0}}});
 for(auto&m:patch["modules"]){auto type=m["type"].get<std::string>();m["module"]=type;m["version"]=1;m["config"]=Json::object();m["parameters"]=Json::object();for(auto it=m["values"].begin();it!=m["values"].end();++it)m["parameters"][app_engine_parameter_id(catalog.get(),type.c_str(),uint32_t(std::stoul(it.key())))]=it.value();m.erase("type");m.erase("values");}
 for(auto&c:patch["connections"]){c["from"]={{"module",c["fromNode"]},{"port",c["fromPort"]}};c["to"]={{"module",c["toNode"]},{"port",c["toPort"]}};c["kind"]="signal";for(auto key:{"fromNode","fromPort","toNode","toPort"})c.erase(key);}
 auto v2=load(patch.dump());check(bool(v2),"v2 loader");float a[128],b[128];
 for(int block=0;block<100;++block){int ra,rb;size_t count;{allocation_watch::Scope watch;ra=app_engine_render(v1.get(),a,128);rb=app_engine_render(v2.get(),b,128);count=watch.allocations();}check(ra==WS_OK&&rb==WS_OK&&count==0,"loader callback allocation-free");for(int i=0;i<128;++i)check(a[i]==b[i]&&std::isfinite(a[i]),"v1/v2 graph equivalence");}
 check(!load("{\"format\":\"wirescape.native.patch\",\"format\":\"wirescape.native.patch\"}"),"duplicate keys rejected");
 for (const auto &raw : {std::string("{\"format\":1,\"\\u0066ormat\":2}"), std::string("{\"x\":[{\"a\":1,\"a\":2}]}")})
     check(!load(raw) && !error.empty(), "escaped and nested duplicate keys refused");
 auto duplicate = patch;
 duplicate["modules"].push_back(duplicate["modules"][0]);
 check(!load(duplicate.dump()), "duplicate module IDs refused");
 auto dangling = patch;
 dangling["connections"][0]["to"]["module"] = 99;
 check(!load(dangling.dump()), "dangling destination refused");
 dangling = patch;
 dangling["outputs"][0]["module"] = 99;
 check(!load(dangling.dump()), "dangling output refused");
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
