// SPDX-License-Identifier: GPL-3.0-only
#include "monitor_log.h"
#include <algorithm>
#include <cstring>
#include <sstream>
#include <iomanip>
std::string format_monitor_entry(const AppMonitorTap&tap,const wirescape::monitor::Entry&e){
 std::ostringstream out;out<<"["<<tap.node<<(tap.prefix.empty()?"":" ")<<tap.prefix<<"] frame "<<e.frame<<" source "<<e.source<<" ";const size_t n=std::min<size_t>(e.bytes&0x7fffffffu,sizeof(e.payload));
 if(e.type&wirescape::monitor::signal_type){float value=0;if(n>=4)std::memcpy(&value,e.payload,4);out<<"signal "<<(e.type&0xffff)<<" ch "<<e.key+1<<" = "<<value;}
 else if(e.type==WS2_MIDI&&n>=4){uint32_t word=0;std::memcpy(&word,e.payload,4);auto type=word>>28,status=(word>>20)&15,ch=(word>>16)&15,key=(word>>8)&127,value=word&127;
  out<<"MIDI"<<type<<" ch "<<ch+1<<" ";if(type==2&&status==11)out<<"CC "<<key<<" "<<float(value)/127<<" ("<<value<<")";else out<<"status "<<status<<" key "<<key<<" value "<<value;
 }else if(e.type==WS2_MESSAGE&&n&&e.payload[0]=='/'){
  const auto*zero=static_cast<const uint8_t*>(std::memchr(e.payload,0,n));size_t length=zero?size_t(zero-e.payload):n;out<<"OSC "<<std::string(reinterpret_cast<const char*>(e.payload),length);
  size_t tag=(length+4)&~size_t(3);if(tag<n&&e.payload[tag]==','){
   auto*end=static_cast<const uint8_t*>(std::memchr(e.payload+tag,0,n-tag));if(end){size_t stop=size_t(end-e.payload),at=(stop+4)&~size_t(3);for(size_t t=tag+1;t<stop;++t){char kind=char(e.payload[t]);out<<" "<<kind<<":";
    if((kind=='f'||kind=='i')&&at+4<=n){uint32_t bits=(uint32_t(e.payload[at])<<24)|(uint32_t(e.payload[at+1])<<16)|(uint32_t(e.payload[at+2])<<8)|e.payload[at+3];at+=4;if(kind=='f'){float f;std::memcpy(&f,&bits,4);out<<f;}else out<<static_cast<int32_t>(bits);}
    else if(kind=='T'||kind=='F'||kind=='N'||kind=='I')out<<(kind=='T'?"true":kind=='F'?"false":kind=='N'?"nil":"impulse");else {out<<"[raw prefix]";break;}
   }}
  }
 }else if((e.type==WS2_NOTE_ON||e.type==WS2_NOTE_OFF||e.type==WS2_NOTE_CHOKE||e.type==WS2_NOTE_EXPRESSION)&&n>=sizeof(wirescape::monitor::NoteSummary)){
  wirescape::monitor::NoteSummary note;std::memcpy(&note,e.payload,sizeof(note));out<<"NOTE "<<e.type<<" ch "<<note.channel+1<<" key "<<note.key<<" id "<<note.note_id<<" value "<<note.value;
 }else{out<<"event "<<e.type<<" bytes "<<n;for(size_t i=0;i<std::min<size_t>(n,16);++i)out<<" "<<std::hex<<std::setw(2)<<std::setfill('0')<<unsigned(e.payload[i]);}
 if(e.bytes&0x80000000u)out<<" [truncated]";return out.str();
}
