// SPDX-License-Identifier: GPL-3.0-only
#include "session.h"
#include <iostream>
int main(int argc,char**argv){
    try{
        auto options=wirescape::sidecar::parse_options(argc,argv);
        wirescape::sidecar::Session session(std::move(options));
        wirescape::sidecar::Transport transport(std::cin,std::cout);
        transport.send(session.hello());transport.start();
        auto status=std::chrono::steady_clock::now(),telemetry=status,devices=status;
        for(;;){
            wirescape::sidecar::Request request;
            if(transport.next(request,std::chrono::milliseconds(5))) {
                auto reply=session.reply(request);transport.send(reply);
                if(reply.value("ok",false)&&request.value.contains("type")&&(request.value["type"]=="selectAudioDevice"||request.value["type"]=="setMidiEnabled"))transport.send(session.devices());
            }
            if(transport.eof())break;
            auto now=std::chrono::steady_clock::now();
            if(now-status>=std::chrono::milliseconds(100)){transport.send(session.status(),true);status=now;}
            if(now-telemetry>=std::chrono::milliseconds(34)){transport.send(session.telemetry(),true);telemetry=now;}
            if(now-devices>=std::chrono::milliseconds(100)){if(session.poll())transport.send(session.devices());devices=now;}
        }
        transport.finish();return 0;
    }catch(const std::exception&e){std::cerr<<"wirescape-engine: "<<e.what()<<'\n';return 1;}
}
