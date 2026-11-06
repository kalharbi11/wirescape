// SPDX-License-Identifier: GPL-3.0-only
#include "devices.h"
#include "runtime_patch.h"
#include "../io/audio/callback_render.h"
#include "../io/audio/portaudio_service.h"
#include "../tests2/allocation_watch.h"
#include <sstream>
#include <stdexcept>
using namespace wirescape::sidecar;
void check(bool ok){if(!ok)throw std::runtime_error("sidecar unit assertion failed");}
int main(){
    for(const auto&bad:{"{\"x\":1,\"x\":2}","{\"x\":{\"y\":1,\"y\":2}}","[]"}){
        bool refused=false;try{strict_json(bad);}catch(...){refused=true;}check(refused);
    }
    std::string boundary="{\"x\":"+std::string(63,'[')+"0"+std::string(63,']')+"}";
    check(strict_json(boundary).is_object());
    bool too_deep=false;try{strict_json("{\"x\":"+std::string(64,'[')+"0"+std::string(64,']')+"}");}catch(...){too_deep=true;}check(too_deep);
    std::ostringstream source;
    for(int i=0;i<257;++i)source<<"{\"v\":0,\"id\":\"q"<<i<<"\",\"type\":\"panic\"}\n";
    std::istringstream input(source.str());std::ostringstream output;Transport transport(input,output);transport.start();
    auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(5);
    while(transport.pending()<PendingLimit&&std::chrono::steady_clock::now()<deadline)std::this_thread::yield();
    check(transport.pending()==PendingLimit);
    // Reader has parsed the 257th command while the control executor is stalled.
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    for(int i=0;i<257;++i){Request request;check(transport.next(request,std::chrono::seconds(5)));check(request.value["id"]=="q"+std::to_string(i));check(request.error==(i==256?"busy":""));}
    transport.finish();
    ObservedMidi fake(true,false);auto endpoints=fake.endpoints();check(endpoints.size()==1);
    int messages=0;
    wirescape::midi::Callback cb{&messages,[](void*context,const uint8_t*,size_t,uint64_t){++*static_cast<int*>(context);},nullptr};
    check(fake.open(endpoints[0],cb));uint8_t on[]={0x90,60,100},off[]={0x90,60,0};
    check(fake.inject(endpoints[0],on,3,100));check(fake.held()==Json::array({60}));
    check(fake.inject(endpoints[0],off,3,110));check(fake.held().empty());check(messages==2);
    fake.inject(endpoints[0],on,3,120);fake.close(endpoints[0]);check(fake.held().empty());
    ws_engine_config config{sizeof(config),48000,128,64,2048};
    wirescape::audio::EngineOwner catalogue(app_engine_create(&config),app_engine_destroy);
    std::string error;
    const char*patch=R"({"format":"wirescape.native.patch","version":2,"settings":{},"modules":[{"id":1,"module":"essentials.vco","version":2},{"id":7,"module":"essentials.output","version":2}],"connections":[{"kind":"signal","from":{"module":1,"port":"audio"},"to":{"module":7,"port":"audio"}}],"outputs":[{"module":7,"port":"audio","bus_channel":0},{"module":7,"port":"audio","bus_channel":1}]})";
    wirescape::audio::EngineOwner engine(wirescape::document::load_runtime_patch(patch,catalogue.get(),config,error),app_engine_destroy);check(bool(engine));
    float scratch[256]{},output_block[256]{};std::atomic<uint64_t>faults{0};
    check(wirescape::audio::render_callback_block(engine.get(),scratch,128,output_block,128,app_engine_clock_ns(),faults));
    ws_event parameter{};parameter.struct_size=sizeof(parameter);parameter.type=WS_PARAMETER;parameter.node_id=1;
    // The native descriptor's stable legacy_index, independent of catalogue order.
    parameter.parameter_id=app_engine_native_descriptor(engine.get(),"essentials.vco")->params[0].legacy_index;
    parameter.generation=app_engine_generation(engine.get());
    allocation_watch::count.store(0);allocation_watch::enabled.store(true);
    bool okay=true;
    for(int i=0;i<1000;++i){parameter.value=float(220+i%100);okay=okay&&app_engine_enqueue(engine.get(),&parameter)==WS_OK;okay=okay&&wirescape::audio::render_callback_block(engine.get(),scratch,128,output_block,128,app_engine_clock_ns(),faults);}
    allocation_watch::enabled.store(false);check(okay);check(allocation_watch::count.load()==0);check(faults.load()==0);
}
