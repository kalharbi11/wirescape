// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "wirescape/module2.h"
namespace wirescape::engine2 {
// Bounded UMP classification, shared by ingress and services. MIDI 2 Note On
// velocity zero is a Note On; only MIDI 1 gives it Note Off semantics.
inline bool midi_release(const ws2_event&e) noexcept {
 if(e.type!=WS2_MIDI||!e.ump_words)return false;
 const auto word=e.ump[0],type=word>>28,status=(word>>20)&15;
 if(type==2&&e.ump_words==1){
  if(word&0x00008080u)return false;
  const auto index=(word>>8)&127,value=word&127;
  return status==8||(status==9&&value==0)||(status==11&&(index==120||index==123||(index==64&&value<64)));
 }
 if(type==4&&e.ump_words==2){
  const auto index=(word>>8)&127;
  return status==8||(status==11&&(index==120||index==123||(index==64&&e.ump[1]<0x80000000u)));
 }
 return false;
}
}
