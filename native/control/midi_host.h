// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "app_engine.h"
#include "midi_backend.h"
#include <memory>
#include <string>
#include <vector>
namespace wirescape::midi {
class Service;struct Counters;
// Control owner only. Stop joins ingress before its borrowed engine is replaced.
class Host {
 std::unique_ptr<Backend> backend_;
 std::unique_ptr<Service> service_;
 AppEngine* engine_=nullptr;
 bool authorized_=false;
 std::vector<Endpoint> enabled_;
 static int deliver(void*,uint32_t,const ws2_event&,uint64_t) noexcept;
public:
 explicit Host(bool authorized=false,std::unique_ptr<Backend> fake={});
 ~Host();
 bool authorized()const noexcept{return authorized_;}
 std::vector<Endpoint> endpoints();
 const std::vector<Endpoint>& enabled()const{return enabled_;}
 void select(Endpoint,bool);
 void restore(const std::string& document);
 bool start(AppEngine*,const std::string& document,std::string& error);
 void stop();
 Counters counters()const noexcept;
 std::string diagnostics()const;
};
}
