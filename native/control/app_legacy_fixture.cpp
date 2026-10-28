// SPDX-License-Identifier: GPL-3.0-only
#include "wirescape/module.h"
#include <algorithm>
static void* WS_CALL create(double){return new float(.25f);}
static void WS_CALL destroy(void*p){delete static_cast<float*>(p);}
static int WS_CALL prepare(void*,const ws_module_config*){return 0;}
static void WS_CALL reset(void*p){*static_cast<float*>(p)=.25f;}
static void WS_CALL event(void*p,const ws_module_event*e){if(e->type==WS_PARAMETER&&e->parameter_id==7)*static_cast<float*>(p)=e->value;}
static void WS_CALL process(void*p,const float*const*,float*const*out,const ws_process_context*c){std::fill_n(out[0],c->frames,*static_cast<float*>(p));}
static const ws_port_descriptor output{"out","Out",WS_AUDIO};
static const ws_parameter_descriptor parameter{7,"Level",0,1,.25f,""};
static const ws_module_descriptor descriptor{sizeof(descriptor),WS_ABI_VERSION,"test.app_legacy","Legacy",0,1,1,nullptr,&output,&parameter,create,destroy,prepare,reset,event,process};
WS_EXPORT const ws_module_descriptor* WS_CALL ws_module_entry(void){return &descriptor;}
