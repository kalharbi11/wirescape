// SPDX-License-Identifier: MIT
#pragma once
#include "wirescape_sdk.h"
#include "../voice/allocator.h"
namespace ws2test {
struct NotesToCv {
    static constexpr ws::Info info{"test.notes_to_cv","Notes to CV",1};
    static constexpr std::array inputs{ws::port("notes","Notes",WS2_EV_NOTES,WS2_POLY,0,nullptr,WS2_EVENT)};
    static constexpr std::array outputs{ws::port("pitch","Pitch",WS2_PITCH),ws::port("gate","Gate",WS2_GATE),ws::port("velocity","Velocity",WS2_CV_UNI),ws::port("pressure","Pressure",WS2_CV_UNI),ws::port("timbre","Timbre",WS2_CV_UNI),ws::port("notes","Notes",WS2_EV_NOTES,WS2_POLY,0,nullptr,WS2_EVENT)};
    static constexpr std::array<ws2_param,0>params{};
    wirescape::voice::Allocator allocator{};
    void prepare(const ws::Setup&s){wirescape::voice::Config cfg;cfg.max_voices=static_cast<uint8_t>(std::min(s.max_channels(),s.config.bytes?uint32_t(s.config.data[0]):8u));if(!cfg.max_voices)cfg.max_voices=1;allocator.configure(cfg);}
    void reset(){allocator.reset();}
    void process(ws::Block&b){
        for(uint32_t p=0;p<5;++p){b.out(p).set_channels(allocator.voice_count());b.out(p).mark_constant(false);}b.out(5).set_channels(0);
        b.split([&](uint32_t first,uint32_t end){for(uint32_t i=first;i<end;++i){
            for(int c=0;c<allocator.voice_count();++c){const auto&v=allocator.voice(c);
                b.out(0).ch(c)[i]=v.pitch;b.out(1).ch(c)[i]=v.gate&&!(v.trigger&&v.retrigger)?1.f:0.f;
                b.out(2).ch(c)[i]=v.velocity;b.out(3).ch(c)[i]=v.pressure;b.out(4).ch(c)[i]=v.timbre;
            }allocator.clear_triggers();
        }},[&](const ws2_event&e){
            wirescape::voice::Note note{e.source_id,e.note_id,static_cast<int16_t>(e.channel<0?0:e.channel),static_cast<int16_t>(e.key),e.value};
            if(e.type==WS2_NOTE_ON){
                std::array<wirescape::voice::Voice,16>before{};for(int c=0;c<allocator.voice_count();++c)before[c]=allocator.voice(c);
                auto result=allocator.note_on(note);if(result.status!=wirescape::voice::Status::Ok)return;
                if(result.stolen){auto old=e;old.type=WS2_NOTE_CHOKE;old.source_id=before[result.voice].source_id;old.note_id=before[result.voice].note_id;old.key=before[result.voice].key;old.channel=result.voice;b.emit(5,old);}
                auto out=e;out.channel=result.voice;b.emit(5,out);
            }else if(e.type==WS2_NOTE_OFF){
                int voice=-1;for(int c=0;c<allocator.voice_count();++c){const auto&v=allocator.voice(c);if(v.source_id==e.source_id&&((e.note_id>=0&&v.note_id==e.note_id)||(e.note_id<0&&v.key==e.key&&v.channel==note.channel)))voice=c;}
                allocator.note_off(note);if(voice>=0){auto out=e;out.channel=voice;b.emit(5,out);}
            }else if(e.type==WS2_NOTE_CHOKE){if(e.source_id==UINT32_MAX)allocator.reset();else allocator.release_all_from_source(e.source_id);b.emit(5,e);}
            else if(e.type==WS2_NOTE_EXPRESSION){for(int c=0;c<allocator.voice_count();++c){const auto&v=allocator.voice(c);if(v.source_id==note.source_id&&v.note_id==note.note_id){float pressure=e.expression==WS2_EX_PRESSURE?e.value:v.pressure,timbre=e.expression==WS2_EX_TIMBRE?e.value:v.timbre;allocator.expression(note,pressure,timbre);break;}}}
        });
    }
};
struct Rows {
    static constexpr ws::Info info{"test.rows","Rows",1};
    static constexpr std::array<ws2_port,0>inputs{};
    static constexpr std::array outputs{ws::port("row_0","Row 1",WS2_CV_UNI),ws::port("row_1","Row 2",WS2_CV_UNI),ws::port("row_2","Row 3",WS2_CV_UNI)};
    static constexpr std::array<ws2_param,0>params{};
    uint32_t count=3;std::array<float,3>values{{.1f,.2f,.3f}};
    void prepare(const ws::Setup&s){count=s.config.bytes?s.config.data[0]:3;}
    void reset(){}
    int query_ports(const ws2_config&c,ws2_ports&p){uint32_t rows=c.bytes?c.data[0]:3;if(!rows||rows>3)return WS2_INVALID;p={sizeof(p),0,rows,nullptr,outputs.data()};return WS2_OK;}
    void process(ws::Block&b){for(uint32_t p=0;p<count;++p){b.out(p).set_channels(b.channels());b.out(p).mark_constant();for(uint32_t c=0;c<b.channels();++c)b.out(p).ch(c)[0]=values[p];}}
    int save_state(ws::Writer w){if(!w.u32(1)||!w.u32(count))return WS2_CAPACITY;for(uint32_t i=0;i<count;++i)if(!w.f32(values[i]))return WS2_CAPACITY;return WS2_OK;}
    int load_state(ws::Reader r){uint32_t version=0,rows=0;if(!r.u32(version))return WS2_INVALID;if(version!=1)return WS2_UNSUPPORTED;if(!r.u32(rows))return WS2_INVALID;if(rows!=count)return WS2_UNSUPPORTED;std::array<float,3>staged{};for(uint32_t i=0;i<rows;++i)if(!r.f32(staged[i])||!std::isfinite(staged[i]))return WS2_INVALID;values=staged;return WS2_OK;}
};
}
