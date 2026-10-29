// SPDX-License-Identifier: GPL-3.0-only
#include "app_patch.h"
#include "../tests2/allocation_watch.h"
#include <QCoreApplication>
#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QTemporaryDir>
#include <algorithm>
#include <cmath>
#include <iostream>
#include <memory>
#include <stdexcept>
using namespace wirescape::document;
static void require(bool ok,const char*s){if(!ok)throw std::runtime_error(s);}
int main(int argc,char**argv){QCoreApplication app(argc,argv);try{
 require(argc==4,"modules, demo and legacy fixtures arguments");ws_engine_config cfg{sizeof(cfg),48000,128,64,2048};
 std::unique_ptr<AppEngine,decltype(&app_engine_destroy)>cat(app_engine_create(&cfg),app_engine_destroy);
 require(app_engine_module_count(cat.get())==13,"all thirteen ABI2 ports in catalogue");
 QFile file(argv[2]);require(file.open(QIODevice::ReadOnly),"read demo");auto bytes=file.readAll();auto source=QJsonDocument::fromJson(bytes).object();auto patch=read_patch(bytes,app_catalog(cat.get()));require(patch.valid(),"migrate demo");QString error;
 std::unique_ptr<AppEngine,decltype(&app_engine_destroy)>e(build_app_patch(cat.get(),patch,cfg,error),app_engine_destroy);if(!e)std::cerr<<error.toStdString()<<'\n';require(bool(e),"build engine2 demo from loader");
 require(app_engine_native(e.get()).descriptor(1)->abi_version==2,"app uses ABI2, not shim for Essentials");
 auto view=editor_view(patch,cat.get());auto saved=editor_patch(view,cat.get(),PatchDocument{patch.object,bytes,2,{}});require(saved.valid()&&saved.object==patch.object,"v2 editor roundtrip preserves complete object");
 QTemporaryDir temp;require(temp.isValid(),"tempdir");require(save_patch(temp.filePath("demo.json"),patch,&error),"v2 atomic save");
 QFile v2file(temp.filePath("demo.json"));require(v2file.open(QIODevice::ReadOnly),"read saved v2");
 auto v2=read_patch(v2file.readAll(),app_catalog(cat.get()));require(v2.valid()&&v2.source_version==2,"reload v2 document");
 std::unique_ptr<AppEngine,decltype(&app_engine_destroy)>reloaded(build_app_patch(cat.get(),v2,cfg,error),app_engine_destroy);require(bool(reloaded),"build saved v2 through real loader");
 for(const char*name:{"patch_v1.json","patch_v2.json","patch_v3_unknown.json","visual_study_v4.json","future_patch_v99.json","malformed_patch.json"}){
  QFile fixture(QString::fromLocal8Bit(argv[3])+"/"+name);require(fixture.open(QIODevice::ReadOnly),"read legacy fixture");auto original=fixture.readAll();
  auto rejected=read_patch(original,app_catalog(cat.get()));require(!rejected.valid()&&rejected.original_bytes==original,"legacy/future/malformed fixture refuses executable loading and retains bytes");
  require(build_app_patch(cat.get(),rejected,cfg,error)==nullptr,"invalid document never builds a graph");
  require(!save_patch(temp.filePath(name),rejected,&error),"rejected legacy file cannot be saved as native");
 }
 std::unique_ptr<ws_engine,decltype(&ws_engine_destroy)>old(ws_engine_create(&cfg),ws_engine_destroy);
 for(const char*name:{"noise","clock","function_envelope","slew","vcf","ladder","vca","output"}){
#ifdef _WIN32
 auto path=QString("%1/ws_%2.dll").arg(argv[1],name);
#else
 auto path=QString("%1/libws_%2.so").arg(argv[1],name);
#endif
 require(ws_engine_load_module(old.get(),path.toUtf8().constData())==WS_OK,"load ABI1 reference");}
 auto desc=[&](const QString&type){for(uint32_t i=0;i<ws_engine_module_count(old.get());++i){auto*d=ws_engine_module(old.get(),i);if(type==QString::fromUtf8(d->id))return d;}return static_cast<const ws_module_descriptor*>(nullptr);};
 auto node=[&](int id){for(auto v:source["modules"].toArray())if(v.toObject()["id"]==id)return desc(v.toObject()["type"].toString());return static_cast<const ws_module_descriptor*>(nullptr);};
 auto port=[](const ws_port_descriptor*p,uint32_t n,const QString&key){for(uint32_t i=0;i<n;++i)if(key==QString::fromUtf8(p[i].id))return i;throw std::runtime_error("port");};
 for(auto v:source["modules"].toArray()){auto m=v.toObject();require(ws_engine_add_node(old.get(),m["id"].toInt(),m["type"].toString().toUtf8().constData())==WS_OK,"old add");}
 for(auto v:source["connections"].toArray()){auto c=v.toObject();auto a=c["fromNode"].toInt(),b=c["toNode"].toInt();auto*ad=node(a);auto*bd=node(b);require(ws_engine_connect(old.get(),a,port(ad->outputs,ad->output_count,c["fromPort"].toString()),b,port(bd->inputs,bd->input_count,c["toPort"].toString()))==WS_OK,"old connect");}
 require(ws_engine_prepare(old.get(),source["outputNode"].toInt(),0)==WS_OK,"old prepare");
 for(auto v:source["modules"].toArray()){auto m=v.toObject();auto values=m["values"].toObject();for(auto p=values.begin();p!=values.end();++p)require(ws_engine_initialize_parameter(old.get(),m["id"].toInt(),p.key().toUInt(),float(p.value().toDouble()))==WS_OK,"old parameter");}
 float a[128],b[128],c[128];double delta=0;size_t allocations=0;
 for(int block=0;block<750;++block){require(ws_engine_render(old.get(),a,128)==WS_OK,"old render");int result,reload_result;{allocation_watch::Scope watch;result=app_engine_render(e.get(),b,128);reload_result=app_engine_render(reloaded.get(),c,128);allocations+=watch.allocations();}require(result==WS_OK&&reload_result==WS_OK,"app render");for(int i=0;i<128;++i){require(std::isfinite(b[i]),"finite");require(b[i]==c[i],"v1 load vs saved/reloaded v2 render exactly identical");delta=std::max(delta,std::abs(double(a[i])-b[i]));}}
 // Planner decision 2026-10-28b: app comparison only; module equivalence stays exact.
 // PortRamp::update skips explicit .2f for Noise's cached default, retaining double
 // .2, whereas ABI1 LinearRamp adopts double(.2f). See modules2/port_state.h:17.
 std::cout<<"Six Essentials ABI1 -> app engine2 max_abs_error="<<delta<<" allocations="<<allocations<<'\n';require(allocations==0,"no callback allocation");require(delta<=1e-7,"app integration render exceeds planner-authorized bound");
 }catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
