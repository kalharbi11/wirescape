/* SPDX-License-Identifier: MIT */
#include "wirescape/module2.h"
#include <stddef.h>
typedef char signal_header_at_zero[(offsetof(ws2_signal,struct_size)==0)?1:-1];
typedef char event_header_at_zero[(offsetof(ws2_event,struct_size)==0)?1:-1];
int main(void){ws2_signal s={0};ws2_event e={0};s.struct_size=sizeof(s);e.struct_size=sizeof(e);return WS_ABI2_VERSION==2 && s.struct_size>0 && e.struct_size>0?0:1;}
