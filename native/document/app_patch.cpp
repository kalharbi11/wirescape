// SPDX-License-Identifier: GPL-3.0-only
#include "app_patch.h"
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
 auto fail=[&](const QString&s)->AppEngine*{error=s;return nullptr;};if(!p.valid())return fail(p.error);
 std::unique_ptr<AppEngine,decltype(&app_engine_destroy)>e(app_engine_create(&config),app_engine_destroy);if(!e)return fail("Cannot create engine2");
 if(app_engine_copy_modules(e.get(),catalog)!=WS_OK)return fail("Cannot copy module catalogue");
 auto&engine=app_engine_native(e.get());QHash<int,const ws_module_descriptor*>nodes;
 auto port=[&](int id,const QString&key,bool output){auto*d=nodes.value(id);if(!d)return -1;auto*ports=output?d->outputs:d->inputs;auto count=output?d->output_count:d->input_count;for(uint32_t i=0;i<count;++i)if(key==QString::fromUtf8(ports[i].id))return int(i);return -1;};
 for(auto value:p.object["modules"].toArray()){
  auto m=value.toObject();auto type=m["module"].toString();int id=m["id"].toInt();
  if(m["inactive"].toBool())return fail("Module inactive; saved data preserved");
  if(app_engine_add_node(e.get(),id,type.toUtf8().constData())!=WS_OK)return fail("Module unavailable or prepare failed: "+type);
  for(uint32_t i=0;i<app_engine_module_count(e.get());++i){auto*d=app_engine_module(e.get(),i);if(type==QString::fromUtf8(d->id))nodes[id]=d;}
 }
 for(auto value:p.object["connections"].toArray()){
  auto c=value.toObject(),a=c["from"].toObject(),b=c["to"].toObject();int from=a["module"].toInt(),to=b["module"].toInt();int ap=port(from,a["port"].toString(),true);bool ok=false;
  if(c["kind"]=="modulation"){auto*d=engine.descriptor(to);if(d)for(uint32_t i=0;i<d->param_count;++i)if(b["port"]==QString::fromUtf8(d->params[i].id))ok=ap>=0&&engine.modulate(from,ap,to,i,float(c["amount"].toDouble()));}
  else {int bp=port(to,b["port"].toString(),false);ok=ap>=0&&bp>=0&&engine.connect(from,ap,to,bp);}
  if(!ok)return fail("Cable endpoint unavailable or incompatible");
 }
 for(auto value:p.object["outputs"].toArray()){auto out=value.toObject();int id=out["module"].toInt(),index=port(id,out["port"].toString(),true);if(index<0||!engine.output(id,index,out["bus_channel"].toInt()))return fail("Invalid output bus");}
 if(!engine.prepare())return fail(QString::fromStdString(engine.error()));
 for(auto value:p.object["modules"].toArray()){auto m=value.toObject();int id=m["id"].toInt();auto params=m["parameters"].toObject();auto type=m["module"].toString();auto mapping=app_catalog(e.get()).value(type).legacy_parameters;
  for(auto it=params.begin();it!=params.end();++it){auto legacy=mapping.key(it.key());if(legacy.isEmpty()||app_engine_initialize_parameter(e.get(),id,legacy.toUInt(),float(it.value().toDouble()))!=WS_OK)return fail("Parameter unavailable or outside range");}
  if(m.contains("state")){auto data=QByteArray::fromBase64(m["state"].toObject()["data"].toString().toLatin1());std::vector<uint8_t>bytes(data.begin(),data.end());if(engine.load_state(id,bytes)!=WS2_OK)return fail("Unsupported module state");}
 }
 error.clear();return e.release();
}
}
