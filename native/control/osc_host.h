// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "app_engine.h"
#include "osc_service.h"
namespace wirescape::osc {
class Host {
 std::unique_ptr<Service> service_;AppEngine*engine_=nullptr;bool authorized_=false;std::string last_statistics_;
 static int deliver(void*,uint32_t,const ws2_event&,uint64_t) noexcept;
public:
 explicit Host(bool authorized=false):authorized_(authorized){}
 ~Host();
 void authorize(bool value){stop();authorized_=value;}
 bool authorized()const{return authorized_;}
 static bool needs_lan(const std::string&document);
 bool start(AppEngine*,const std::string&document,uint16_t port_override,std::string&error);
 void stop();
 std::string diagnostics()const;
};
}
