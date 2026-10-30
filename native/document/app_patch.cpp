// SPDX-License-Identifier: GPL-3.0-only
#include "app_patch.h"
#include "runtime_patch.h"
#include <QJsonArray>
#include <QJsonDocument>
#include <memory>
#include <cstring>
namespace wirescape::document {
ModuleCatalog app_catalog(AppEngine*e){
 ModuleCatalog r;
 for(uint32_t i=0;i<app_engine_module_count(e);++i){auto*d=app_engine_module(e,i);ModuleInfo info;
  for(uint32_t p=0;p<d->parameter_count;++p)info.legacy_parameters[QString::number(d->parameters[p].id)]=QString::fromUtf8(app_engine_parameter_id(e,d->id,p));
  if(d->output_count)info.default_output=QString::fromUtf8(d->outputs[0].id);r.insert(QString::fromUtf8(d->id),info);
 }return r;
}
QJsonObject editor_view(const PatchDocument&p,AppEngine*e){
 auto o=p.object;const auto catalog=app_catalog(e);QJsonArray nodes,cables;
 for(const auto& value:o["modules"].toArray()){
  auto m=value.toObject();const auto type=m["module"].toString();m["type"]=type;QJsonObject values;
  const auto params=m["parameters"].toObject();for(auto it=params.begin();it!=params.end();++it){auto key=catalog.value(type).legacy_parameters.key(it.key());if(!key.isEmpty())values[key]=it.value();}
  // Unknown v1 values remain editable without interpreting opaque payloads.
  if(!catalog.contains(type))values=m["preserved"].toObject()["values"].toObject();
  m["values"]=values;nodes.append(m);
 }
 for(const auto& value:o["connections"].toArray()){
  auto c=value.toObject();auto a=c["from"].toObject(),b=c["to"].toObject();c["fromNode"]=a["module"];c["fromPort"]=a["port"];c["toNode"]=b["module"];c["toPort"]=b["port"];
  auto type=QString();for(auto v:nodes)if(v.toObject()["id"]==a["module"])type=v.toObject()["type"].toString();
  int kind=WS_CONTROL;for(uint32_t i=0;i<app_engine_module_count(e);++i){auto*d=app_engine_module(e,i);if(type==QString::fromUtf8(d->id))for(uint32_t p=0;p<d->output_count;++p)if(a["port"]==QString::fromUtf8(d->outputs[p].id))kind=d->outputs[p].kind;}
  if(!catalog.contains(type)){
   QString destination;for(auto v:nodes)if(v.toObject()["id"]==b["module"])destination=v.toObject()["type"].toString();
   for(uint32_t i=0;i<app_engine_module_count(e);++i){auto*d=app_engine_module(e,i);if(destination==QString::fromUtf8(d->id))for(uint32_t p=0;p<d->input_count;++p)if(b["port"]==QString::fromUtf8(d->inputs[p].id))kind=d->inputs[p].kind;}
  }
  if(c["kind"]=="signal")c["kind"]=kind==WS_AUDIO?"audio":"control";cables.append(c);
 }
 o["version"]=1;o["modules"]=nodes;o["connections"]=cables;auto outputs=o["outputs"].toArray();o["outputNode"]=outputs.isEmpty()?0:outputs[0].toObject()["module"].toInt();return o;
}
PatchDocument editor_patch(const QJsonObject&view,AppEngine*e,const PatchDocument&source){
 if(source.source_version!=2)return read_patch(QJsonDocument(view).toJson(),app_catalog(e));
 auto o=view;QJsonArray nodes,cables;auto catalog=app_catalog(e);
 for(auto value:view["modules"].toArray()){
  auto m=value.toObject();const auto type=m["type"].toString();auto params=m["parameters"].toObject();auto values=m["values"].toObject();
  for(auto it=values.begin();it!=values.end();++it)if(catalog.value(type).legacy_parameters.contains(it.key()))params[catalog[type].legacy_parameters[it.key()]]=it.value();
  m["module"]=type;m["version"]=m["version"].toInt(1);m["parameters"]=params;m.remove("values");m.remove("type");nodes.append(m);
 }
 for(auto value:view["connections"].toArray()){auto c=value.toObject();c["from"]=QJsonObject{{"module",c["fromNode"]},{"port",c["fromPort"]}};c["to"]=QJsonObject{{"module",c["toNode"]},{"port",c["toPort"]}};if(c["kind"]=="audio"||c["kind"]=="control")c["kind"]="signal";for(auto key:{"fromNode","fromPort","toNode","toPort"})c.remove(key);cables.append(c);}
 auto outputs=o["outputs"].toArray();int id=view["outputNode"].toInt();if(outputs.isEmpty()||outputs[0].toObject()["module"]!=id){outputs={};for(auto n:view["modules"].toArray())if(n.toObject()["id"]==id)outputs.append(QJsonObject{{"module",id},{"port",catalog.value(n.toObject()["type"].toString()).default_output},{"bus_channel",0}});}
 o["version"]=2;o["modules"]=nodes;o["connections"]=cables;o["outputs"]=outputs;if(!source.object.contains("outputNode"))o.remove("outputNode");
 PatchDocument p=source;p.object=o;validate_patch(o,&p.error);return p;
}
AppEngine* build_app_patch(AppEngine*catalog,const PatchDocument&p,const ws_engine_config&config,QString&error){
 if(!p.valid()){error=p.error;return nullptr;}
 std::string detail;
 auto*engine=load_runtime_patch(QJsonDocument(p.object).toJson(QJsonDocument::Compact).toStdString(),catalog,config,detail);
 error=QString::fromStdString(detail);return engine;
}
}
