// SPDX-License-Identifier: GPL-3.0-only
#include "midi_host.h"
#include <cstdio>
#include <cstdlib>
#include <exception>
#define CHECK(x) do{if(!(x))std::abort();}while(0)
struct Fake: wirescape::midi::Backend {
 int enumerations=0,opens=0;
 std::vector<wirescape::midi::Endpoint>endpoints()override{++enumerations;return {{"Controller","system-name"}};}
 bool open(const wirescape::midi::Endpoint&,wirescape::midi::Callback)override{++opens;return true;}
 void close(const wirescape::midi::Endpoint&)override{}
};
int main(){
 wirescape::midi::Host disabled;CHECK(!disabled.authorized());CHECK(disabled.endpoints().empty());
 auto fake=std::make_unique<Fake>();auto*ptr=fake.get();wirescape::midi::Host host(false,std::move(fake));
 host.restore(R"({"devices":[{"kind":"midi","name":"Controller","system_name":"system-name","enabled":true}]})");
 bool rejected=false;try{host.restore(R"({"devices":[{"kind":"midi","name":123,"enabled":true}]})");}catch(const std::exception&){rejected=true;}CHECK(rejected);CHECK(host.enabled().size()==1);
 CHECK(host.enabled().size()==1);CHECK(host.enabled()[0].system_name=="system-name");
 std::string error;CHECK(!host.start(nullptr,R"({"modules":[]})",error));CHECK(error.find("--confirm-midi")!=std::string::npos);
 CHECK(ptr->enumerations==0&&ptr->opens==0);host.stop();host.stop();
 host.select({"Controller","system-name"},false);CHECK(host.enabled().empty());CHECK(host.start(nullptr,R"({"modules":[]})",error));
 std::puts("PASS host MIDI explicit authorization, saved-name restore and idempotent shutdown");
}
