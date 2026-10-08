#include "native_earth_page.hpp"
#include "earth_navigation.hpp"
#include "earth_renderer.hpp"
#include "sonnheide/world_creation.hpp"
#include <RmlUi/Core.h>
#include <RmlUi/Core/Elements/ElementFormControlInput.h>
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <future>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string_view>

namespace sonnheide::client {
namespace {
struct Label {const char* key;const char* zh;const char* en;const char* de;};
constexpr Label labels[]={
 {"title","创建世界","Create World","Welt erschaffen"},
 {"back","返回","Back","Zurück"},{"select","选择范围","Select Area","Gebiet wählen"},
 {"preview","预览地表","Preview Terrain","Gelände ansehen"},
 {"create","创建世界","Create World","Welt erschaffen"},
 {"cancel","取消","Cancel","Abbrechen"},{"atlas","调整选区","Adjust Area","Gebiet anpassen"},
 {"light","明亮预览","Light Preview","Helle Vorschau"},
 {"dark","黑暗预览","Dark Preview","Dunkle Vorschau"},
 {"surface","地表材质","Ground Material","Bodenmaterial"},
 {"name","世界名称","World Name","Name der Welt"},
 {"loading","正在读取离线地理来源","Reading Offline Geography","Offline-Geodaten werden gelesen"},
 {"renderloading","正在准备地表与天空","Preparing Ground and Sky","Boden und Himmel werden vorbereitet"},
 {"preparing","正在验证地表并保存世界","Validating Terrain and Saving World","Gelände wird geprüft und die Welt gespeichert"},
 {"previewing","正在准备精细地表预览","Preparing Detailed Terrain","Detailliertes Gelände wird vorbereitet"},
 {"loadingworld","正在读取已保存的世界","Reading Saved World","Gespeicherte Welt wird gelesen"},
 {"empty","人口 0 · 建筑 0","Population 0 · Buildings 0","Bevölkerung 0 · Gebäude 0"},
 {"sourceerror","所需的岸线、材质或天空资源缺失、损坏或版本不符。请重新准备资源后重试。","Required coastline, material or sky resources are missing, damaged or incompatible. Prepare the resources again, then retry.","Benötigte Küsten-, Material- oder Himmelsdaten fehlen, sind beschädigt oder inkompatibel. Bitte die Ressourcen erneut vorbereiten und wiederholen."},
 {"saveerror","无法读取或可靠保存世界。请检查存档目录是否可写、磁盘空间以及存档是否完整；原世界保留。","The world could not be read or durably saved. Check directory access, disk space and save integrity. The previous world is preserved.","Die Welt konnte nicht gelesen oder dauerhaft gespeichert werden. Bitte Zugriffsrechte, Speicherplatz und den Spielstand prüfen. Die vorige Welt bleibt erhalten."},
 {"nameerror","名称不能为空，最多 96 个字符，且不能包含控制字符。","Use a nonempty name of at most 96 characters, without control characters.","Der Name muss 1 bis 96 Zeichen lang sein und darf keine Steuerzeichen enthalten."},
 {"genericerror","操作未完成；请调整选区或返回后重试。原世界保留。","The operation did not complete. Adjust the area or return and retry. The previous world is preserved.","Der Vorgang wurde nicht abgeschlossen. Bitte das Gebiet anpassen oder zurückkehren und wiederholen. Die vorige Welt bleibt erhalten."},
 {"failure","无法完成操作","Operation Could Not Complete","Vorgang konnte nicht abgeschlossen werden"},
 {"arealimit","选区每边最多 4,000 km；请放大地图并选择较小区域。","Each side may span at most 4,000 km. Zoom in and select a smaller area.","Jede Seite darf höchstens 4.000 km umfassen. Bitte heranzoomen und ein kleineres Gebiet wählen."},
 {"range","区域","Area","Gebiet"},{"sea","包含外围海域","Includes Surrounding Sea","Mit umliegendem Meer"},
 {"scale","地图比例","Map Scale","Kartenmaßstab"},
 {"ready","预览已就绪","Preview Ready","Vorschau bereit"},
 {"sources","来源","Sources","Quellen"},
 {"sourcefacts","GSHHG 2.3.7 完整岸线；不含真实高程。导航地图逐级加载，精细预览与游戏共用冻结地表。岸线位置精度受原始资料限制，放大不增加测量精度。","GSHHG 2.3.7 full coastlines; no real elevation. Navigation uses detail levels. Detailed preview and game share the frozen terrain. Zooming does not improve the source’s positional accuracy.","GSHHG 2.3.7, vollständige Küsten; keine echten Höhen. Die Übersicht nutzt Detailstufen. Detaillierte Vorschau und Spiel teilen das eingefrorene Gelände. Vergrößern verbessert nicht die Lagegenauigkeit der Quelle."},
 {"longitude","经度","Longitude","Längengrad"},{"latitude","纬度","Latitude","Breitengrad"},
 {"sourceart","地表：Poly Haven · CC0。反重复：mmikk Hex-Tiling · MIT。明暗：两套固定 Pure Sky。水面当前为静态海平面。","Ground: Poly Haven · CC0. Tiling: mmikk Hex-Tiling · MIT. Light and Darkness: two fixed Pure Skies. Water currently uses a static mean sea level.","Boden: Poly Haven · CC0. Kachelung: mmikk Hex-Tiling · MIT. Licht und Dunkelheit: zwei feste Pure Skies. Wasser verwendet derzeit einen statischen mittleren Meeresspiegel."},
 {"coast","真实岸线 · 平坦内陆","Real Coastline · Flat Inland","Echte Küste · Flaches Inland"}
};
std::string text(Locale locale,std::string_view key){
 for(const auto& l:labels)if(key==l.key)return locale==Locale::Chinese?l.zh:locale==Locale::German?l.de:l.en;
 return std::string(key);
}
std::string explain(Locale locale,const std::string& reason){
 const auto has=[&](const char* needle){return reason.find(needle)!=std::string::npos;};
 if(reason==text(locale,"arealimit"))return reason;
 if(has("UTF-8")||has("world name"))return text(locale,"nameerror");
 if(has("source")||has("resource")||has("SHA")||has("Ground")||has("HDR")||has("shader")||has("geography"))return text(locale,"sourceerror");
 if(has("checkpoint")||has("pointer")||has("save")||has("directory")||has("fsync")||has("write")||has("read"))return text(locale,"saveerror");
 if(has("budget")||has("dimensions")||has("Closure")||has("scale"))return text(locale,"arealimit");
 return text(locale,"genericerror");
}
std::string escape(std::string_view s){std::string r;for(char c:s){if(c=='&')r+="&amp;";else if(c=='<')r+="&lt;";else if(c=='>')r+="&gt;";else if(c=='\"')r+="&quot;";else r+=c;}return r;}
bool done(std::future_status s){return s==std::future_status::ready;}
constexpr double pi=3.14159265358979323846;
}

struct NativeEarthPage::Impl {
 enum class Mode {Atlas,Preview,Preparing,World};
 Rml::Context& context;std::filesystem::path root,saves;Rml::ElementDocument* document{};
 Locale locale{Locale::Chinese};Mode mode{Mode::Atlas};bool shown{},returned{},selecting{},drag{},dragged{},darkness{},open_saved{},cancelled_load{},resource_attempted{},orbit{},show_sources{};
 double start_x{},start_y{},last_x{},last_y{};int width{1280},height{800};
 AtlasNavigation view;earth::WorldConfig config;native::EarthCamera camera;
 std::uint64_t revision{1},job_revision{};int material{-1};std::string error,status,world_name{"Sonnheide"};
 std::shared_ptr<std::atomic<bool>> stop;
 std::future<std::shared_ptr<earth::GeoAtlas>> atlas_job;
 std::future<std::shared_ptr<const earth::EarthWorldDefinition>> preview_job;
 std::future<std::shared_ptr<const creation::WorldSession>> load_job;
 std::shared_ptr<earth::GeoAtlas> atlas;
 std::shared_ptr<const earth::EarthWorldDefinition> candidate;
 std::unique_ptr<native::EarthRenderer> renderer;
 std::unique_ptr<creation::CreateCoordinator> coordinator;
 creation::CreationTicket ticket;
 Impl(Rml::Context& c,std::filesystem::path r,std::filesystem::path s):context(c),root(std::move(r)),saves(std::move(s)){
  view.longitude=0;view.latitude=0;config.centre={14.382,40.626};config.world_scale=.04;
 }
 double map_height()const {if(document){auto* bar=document->GetElementById("earth-toolbar");if(bar){double top=bar->GetAbsoluteOffset(Rml::Box::BORDER).y/std::max(.1f,context.GetDensityIndependentPixelRatio());if(top>0)return top;}}return std::max(1,height-106);}
 bool busy()const{return preview_job.valid()||load_job.valid()||(coordinator&&coordinator->busy());}
 void invalidate(){++revision;if(stop)stop->store(true);candidate.reset();error.clear();if(coordinator)coordinator->invalidate_draft(revision);}
 creation::ResourceReadiness readiness(const earth::EarthWorldDefinition& d)const{
  const bool ready=renderer&&renderer->ready();return {d.recipe_hash(),ready?renderer->resource_recipe_hash():"",ready,ready,ready,ready};
 }
 void render_world(std::shared_ptr<const earth::EarthWorldDefinition> definition){
  renderer->set_world(definition);camera={};camera.height_m=float(std::min(definition->config().width_m,definition->config().height_m)*.18);camera.pitch_deg=65;camera.orthographic=true;
  const auto bounds=definition->domain_bounds(),selection=definition->selection_bounds();double nearest=1e100;const auto& cfg=definition->config();
  for(std::uint32_t z=0;z<definition->rows();++z)for(std::uint32_t x=0;x<definition->columns();++x){
   earth::Point p{bounds.min_x+(x+.5)*definition->cell_width_m(),bounds.min_y+(z+.5)*definition->cell_height_m()};
   const double margin=cfg.closure_band_m+cfg.coast_width_m*2;
   if(p.x<selection.min_x+margin||p.x>selection.max_x-margin||p.y<selection.min_y+margin||p.y>selection.max_y-margin)continue;
   const auto surface=definition->sample(p);const double distance=std::hypot(p.x,p.y);
   if(surface.source_land&&surface.land&&surface.coast_distance_m<cfg.coast_width_m*2&&distance<nearest){nearest=distance;camera.target_x_m=p.x;camera.target_z_m=p.y;}
  }
 }
 void prepare_preview(){
  if(!atlas||!renderer||busy())return;
  error.clear();status="previewing";stop=std::make_shared<std::atomic<bool>>(false);job_revision=revision;
  const auto source=atlas;const auto cfg=config;const auto token=stop;
  preview_job=std::async(std::launch::async,[source,cfg,token]{return earth::EarthWorldDefinition::create(source,cfg,[token]{return token->load();});});
 }
 void prepare_load(){
  if(!coordinator||busy())return;
  cancelled_load=false;status="loadingworld";job_revision=revision;stop=std::make_shared<std::atomic<bool>>(false);
  ticket=coordinator->begin(revision);const auto token=stop;
  const auto* writer=coordinator.get();load_job=std::async(std::launch::async,[writer,token]{return writer->prepare_continue([token]{return token->load();});});
 }
 void refresh(){
  if(!document)return;
  document->SetProperty("font-family",locale==Locale::Chinese?"Noto Serif CJK SC":"Cinzel");
  for(const auto& label:labels)if(auto* e=document->GetElementById(std::string("earth-text-")+label.key))e->SetInnerRML(escape(text(locale,label.key)));
  document->GetElementById("earth-sources-panel")->SetProperty("display",show_sources?"block":"none");
  std::ostringstream coordinates;coordinates<<text(locale,"longitude")<<" "<<std::fixed<<std::setprecision(5)<<config.centre.longitude<<" · "<<text(locale,"latitude")<<" "<<config.centre.latitude<<" · "<<text(locale,"scale")<<" 1:"<<std::setprecision(2)<<1/config.world_scale;document->GetElementById("earth-source-position")->SetInnerRML(escape(coordinates.str()));
  const bool atlas_mode=mode==Mode::Atlas,world=mode==Mode::World,waiting=busy();
  for(const auto* action:{"select","preview","scale"})if(auto* e=document->GetElementById(std::string("earth-action-")+action))e->SetProperty("display",atlas_mode?"inline-block":"none");
  for(const auto* action:{"atlas","light","dark","create"})if(auto* e=document->GetElementById(std::string("earth-action-")+action))e->SetProperty("display",mode==Mode::Preview&&!waiting?"inline-block":"none");
  document->GetElementById("earth-action-cancel")->SetProperty("display",waiting?"inline-block":"none");
  document->GetElementById("earth-name-group")->SetProperty("display",mode==Mode::Preview&&!waiting?"block":"none");
  document->GetElementById("earth-heading")->SetInnerRML(escape(world&&coordinator&&coordinator->active()?coordinator->active()->display_name:text(locale,"title")));
  document->GetElementById("earth-facts")->SetInnerRML(escape(world?text(locale,"empty"):text(locale,"coast")));
  document->GetElementById("earth-status")->SetInnerRML(escape(error.empty()?text(locale,status):text(locale,"failure")+": "+explain(locale,error)));
  document->GetElementById("earth-status")->SetProperty("display",status.empty()&&error.empty()?"none":"block");
  for(const auto* action:{"preview","select","scale"})document->GetElementById(std::string("earth-action-")+action)->SetPseudoClass("disabled",!atlas||!renderer||waiting);
  document->GetElementById("earth-action-select")->SetClass("selected",selecting);
  if(auto* e=document->GetElementById("earth-action-scale")){std::ostringstream ss;ss<<text(locale,"scale")<<" 1:"<<std::fixed<<std::setprecision(0)<<1/config.world_scale;e->SetInnerRML(escape(ss.str()));}
  if(candidate||atlas){std::ostringstream ss;ss<<text(locale,"range")<<" "<<std::fixed<<std::setprecision(1)<<config.width_m/config.world_scale/1000<<" × "<<config.height_m/config.world_scale/1000<<" km · "<<text(locale,"sea");document->GetElementById("earth-area")->SetInnerRML(escape(ss.str()));}
  document->GetElementById("earth-title-row")->SetProperty("max-width",std::to_string(std::max(260,width-80))+"dp");
 }
};

NativeEarthPage::NativeEarthPage(Rml::Context& c,std::filesystem::path r,std::filesystem::path s):impl_(std::make_unique<Impl>(c,std::move(r),std::move(s))){}
NativeEarthPage::~NativeEarthPage(){auto& p=*impl_;if(p.stop)p.stop->store(true);if(p.coordinator)p.coordinator->cancel(p.ticket);if(p.load_job.valid())p.load_job.wait();if(p.preview_job.valid())p.preview_job.wait();if(p.atlas_job.valid())p.atlas_job.wait();if(p.document){p.document->RemoveEventListener("click",this);p.document->RemoveEventListener("keydown",this);p.document->RemoveEventListener("change",this);p.document->Close();}}
bool NativeEarthPage::initialize(std::string& error){auto& p=*impl_;auto path=(p.root/"ui/application/create-world.rml").u8string();p.document=p.context.LoadDocument(std::string(path.begin(),path.end()));if(!p.document){error="Cannot load create-world.rml";return false;}for(const auto* event:{"click","keydown","change"})p.document->AddEventListener(event,this);p.refresh();p.document->Hide();const auto source=p.root/"assets/runtime/geography";p.atlas_job=std::async(std::launch::async,[source]{return earth::GeoAtlas::load(source);});error.clear();return true;}
void NativeEarthPage::open(Locale locale,bool saved){auto& p=*impl_;p.locale=locale;p.shown=true;p.returned=false;p.mode=Impl::Mode::Atlas;p.open_saved=saved;p.error.clear();p.status="loading";p.candidate.reset();p.material=-1;p.resource_attempted=false;if(p.renderer)p.renderer->clear_world();p.document->Show();
 if(!p.atlas&&!p.atlas_job.valid()){const auto path=p.root/"assets/runtime/geography";p.atlas_job=std::async(std::launch::async,[path]{return earth::GeoAtlas::load(path);});}
 if(p.atlas&&p.renderer){p.status.clear();if(saved)p.prepare_load();}p.refresh();}
void NativeEarthPage::update(){auto& p=*impl_;const auto size=p.context.GetDimensions();const float density=std::max(.1f,p.context.GetDensityIndependentPixelRatio());p.width=int(size.x/density);p.height=int(size.y/density);
 try{
  if(p.atlas_job.valid()&&done(p.atlas_job.wait_for(std::chrono::seconds(0)))){p.atlas=p.atlas_job.get();p.status="renderloading";p.refresh();}
  else if(p.atlas&&!p.renderer&&!p.resource_attempted){p.resource_attempted=true;p.renderer=std::make_unique<native::EarthRenderer>(p.root);p.coordinator=std::make_unique<creation::CreateCoordinator>(p.saves,p.atlas,p.renderer->resource_recipe_hash());p.status.clear();if(p.open_saved)p.prepare_load();}
  if(p.preview_job.valid()&&done(p.preview_job.wait_for(std::chrono::seconds(0)))){auto candidate=p.preview_job.get();if(p.job_revision==p.revision&&p.stop&&!p.stop->load()&&p.shown){p.render_world(candidate);p.candidate=std::move(candidate);p.mode=Impl::Mode::Preview;p.darkness=false;p.status="ready";}else p.status.clear();}
  if(p.load_job.valid()&&done(p.load_job.wait_for(std::chrono::seconds(0)))){auto session=p.load_job.get();if(!p.cancelled_load&&p.job_revision==p.revision&&p.shown){p.render_world(session->definition);auto result=p.coordinator->publish_continue(session,p.readiness(*session->definition),p.ticket);if(!result)throw std::runtime_error(result.error);p.candidate=session->definition;p.config=session->definition->config();p.mode=Impl::Mode::World;p.darkness=false;p.status.clear();}}
  if(p.coordinator)if(auto result=p.coordinator->poll()){if(*result){p.mode=Impl::Mode::World;p.darkness=false;p.status.clear();p.candidate=result->session->definition;}else{p.mode=p.candidate?Impl::Mode::Preview:Impl::Mode::Atlas;p.status.clear();if(result->status==creation::CreateStatus::Failed)p.error=result->error;}}
 }catch(const std::exception& e){if(!p.stop||!p.stop->load()){p.error=e.what();std::cerr<<"Earth operation: "<<p.error<<'\n';}p.status.clear();p.mode=p.candidate?Impl::Mode::Preview:Impl::Mode::Atlas;}
 p.refresh();}
bool NativeEarthPage::handle_event(const SDL_Event& event,int w,int h){auto& p=*impl_;if(!p.shown)return false;p.width=w;p.height=h;const double map_h=p.map_height();
 auto interactive=[&](float x,float y){auto* e=p.context.GetElementAtPoint({x*p.context.GetDensityIndependentPixelRatio(),y*p.context.GetDensityIndependentPixelRatio()});while(e&&e!=p.document){if(e->GetId()=="earth-sources-panel"||e->GetTagName()=="input"||e->GetTagName()=="button")return true;e=e->GetParentNode();}return false;};
 if(event.type==SDL_EVENT_MOUSE_WHEEL){float x=0,y=0;SDL_GetMouseState(&x,&y);if(y>=map_h||interactive(x,y))return false;if(p.mode==Impl::Mode::Atlas)p.view.zoom(event.wheel.y,x,y,w,map_h);else if(!p.busy())p.camera.height_m=std::clamp(p.camera.height_m*float(std::exp(-event.wheel.y*.15)),8.f,30000.f);return true;}
 
 if(event.type==SDL_EVENT_MOUSE_BUTTON_DOWN&&interactive(event.button.x,event.button.y))return false;
 if(event.type==SDL_EVENT_MOUSE_BUTTON_DOWN&&event.button.y<map_h&&(event.button.button==SDL_BUTTON_LEFT||event.button.button==SDL_BUTTON_RIGHT)){p.orbit=event.button.button==SDL_BUTTON_RIGHT;p.drag=true;p.dragged=false;p.start_x=p.last_x=event.button.x;p.start_y=p.last_y=event.button.y;return true;}
 if(event.type==SDL_EVENT_MOUSE_MOTION&&p.drag){const double x=event.motion.x,y=event.motion.y;if(std::hypot(x-p.start_x,y-p.start_y)>3)p.dragged=true;
  if(p.mode==Impl::Mode::Atlas){if(!p.selecting)p.view.pan(x-p.last_x,y-p.last_y,w,map_h);}else if(!p.busy()&&p.orbit){p.camera.orthographic=false;p.camera.yaw_deg+=float(x-p.last_x)*.25f;p.camera.pitch_deg=std::clamp(p.camera.pitch_deg+float(y-p.last_y)*.2f,15.f,85.f);}else if(!p.busy()){const double mpp=p.camera.height_m/map_h;p.camera.target_x_m-=(x-p.last_x)*mpp;p.camera.target_z_m+=(y-p.last_y)*mpp;const auto bounds=p.candidate->domain_bounds();p.camera.target_x_m=std::clamp(p.camera.target_x_m,bounds.min_x,bounds.max_x);p.camera.target_z_m=std::clamp(p.camera.target_z_m,bounds.min_y,bounds.max_y);}p.last_x=x;p.last_y=y;return true;}
 if(event.type==SDL_EVENT_MOUSE_BUTTON_UP&&p.drag&&(event.button.button==SDL_BUTTON_LEFT||event.button.button==SDL_BUTTON_RIGHT)){p.drag=false;if(p.mode==Impl::Mode::Atlas&&!p.busy()&&!p.orbit){
   if(p.selecting&&p.dragged){const auto a=p.view.at(p.start_x,p.start_y,w,map_h),b=p.view.at(event.button.x,event.button.y,w,map_h);double dl=b.longitude-a.longitude;dl-=360*std::round(dl/360);p.config.centre={a.longitude+dl*.5,(a.latitude+b.latitude)*.5};const double real_w=std::abs(dl)*pi/180*6371008.8*std::cos(p.config.centre.latitude*pi/180),real_h=std::abs(a.latitude-b.latitude)*pi/180*6371008.8;if(real_w>=100&&real_h>=100){if(real_w>4000000||real_h>4000000){p.error=text(p.locale,"arealimit");}else {p.config.world_scale=p.config.width_m/real_w;p.config.height_m=p.config.width_m*real_h/real_w;p.invalidate();}}}
   else if(!p.dragged){auto c=p.view.at(event.button.x,event.button.y,w,map_h);if(c.latitude>=-90&&c.latitude<=90){p.config.centre=c;p.invalidate();}}
  }p.refresh();return true;}
 if(event.type==SDL_EVENT_KEY_DOWN&&!p.busy()){
  auto* focus=p.context.GetFocusElement();const bool typing=focus&&focus->GetTagName()=="input";
  if(!typing){double step=p.mode==Impl::Mode::Atlas?50:p.camera.height_m*.08;
   if(event.key.key==SDLK_LEFT||event.key.key==SDLK_RIGHT||event.key.key==SDLK_UP||event.key.key==SDLK_DOWN){double dx=event.key.key==SDLK_LEFT?step:event.key.key==SDLK_RIGHT?-step:0,dy=event.key.key==SDLK_UP?step:event.key.key==SDLK_DOWN?-step:0;if(p.mode==Impl::Mode::Atlas)p.view.pan(dx,dy,w,map_h);else {p.camera.target_x_m-=dx;p.camera.target_z_m+=dy;const auto bounds=p.candidate->domain_bounds();p.camera.target_x_m=std::clamp(p.camera.target_x_m,bounds.min_x,bounds.max_x);p.camera.target_z_m=std::clamp(p.camera.target_z_m,bounds.min_y,bounds.max_y);}return true;}
   if(event.key.key==SDLK_EQUALS||event.key.key==SDLK_MINUS){int n=event.key.key==SDLK_EQUALS?1:-1;if(p.mode==Impl::Mode::Atlas)p.view.zoom(n,w*.5,map_h*.5,w,map_h);else p.camera.height_m=std::clamp(p.camera.height_m*float(std::exp(-n*.15)),8.f,30000.f);return true;}
  }
 }
 if(event.type==SDL_EVENT_KEY_DOWN&&event.key.key==SDLK_ESCAPE){activate(p.show_sources?"sources":p.busy()?"cancel":"back");return true;}
 return false;}
void NativeEarthPage::draw(int w,int h){auto& p=*impl_;if(!p.shown||!p.renderer||!p.atlas)return;const int viewport_h=std::clamp(int(p.map_height()*p.context.GetDensityIndependentPixelRatio()),1,h);if(p.mode==Impl::Mode::Atlas)p.renderer->draw_atlas(*p.atlas,p.view.longitude,p.view.latitude,p.view.span_longitude,w,viewport_h,p.config.centre,p.config.width_m/p.config.world_scale,p.config.height_m/p.config.world_scale);else if(p.candidate)p.renderer->draw(p.camera,p.mode==Impl::Mode::World?false:p.darkness,w,viewport_h,p.material);}
void NativeEarthPage::activate(const std::string& action){auto& p=*impl_;
 if(action=="back"){if(p.stop)p.stop->store(true);if(p.coordinator)p.coordinator->cancel(p.ticket);p.cancelled_load=true;++p.revision;p.shown=false;p.returned=true;p.document->Hide();}
 else if(action=="cancel"){if(p.stop)p.stop->store(true);if(p.coordinator)p.coordinator->cancel(p.ticket);p.cancelled_load=true;p.status.clear();}
 else if(action=="sources")p.show_sources=!p.show_sources;
 else if(action=="select"&&!p.busy())p.selecting=!p.selecting;
 else if(action=="preview")p.prepare_preview();
 else if(action=="atlas"&&!p.busy()){p.mode=Impl::Mode::Atlas;p.invalidate();p.status.clear();p.renderer->clear_world();}
 else if(action=="scale"&&!p.busy()){p.config.world_scale=p.config.world_scale<.08?.1:p.config.world_scale<.8?1:.04;p.invalidate();}
 else if(action=="light"&&p.mode==Impl::Mode::Preview)p.darkness=false;
 else if(action=="dark"&&p.mode==Impl::Mode::Preview)p.darkness=true;

 else if(action=="create"&&p.candidate&&!p.busy()){try{p.material=-1;p.darkness=false;if(auto* input=dynamic_cast<Rml::ElementFormControlInput*>(p.document->GetElementById("earth-world-name")))p.world_name=input->GetValue();p.ticket=p.coordinator->begin(p.revision,p.world_name);const auto result=p.coordinator->start(p.ticket,p.candidate,p.readiness(*p.candidate));if(result.status!=creation::CreateStatus::Preparing)throw std::runtime_error(result.error);p.mode=Impl::Mode::Preparing;p.status="preparing";p.error.clear();}catch(const std::exception& e){p.error=e.what();}}
 p.refresh();}
void NativeEarthPage::ProcessEvent(Rml::Event& event){if(!impl_->shown)return;if(event.GetType()=="keydown"){const int key=event.GetParameter<int>("key_identifier",0);if(key==Rml::Input::KI_ESCAPE){activate(impl_->show_sources?"sources":busy()?"cancel":"back");event.StopPropagation();}else if(key==Rml::Input::KI_RETURN||key==Rml::Input::KI_SPACE){auto* e=event.GetTargetElement();if(e&&e->GetId().find("earth-action-")==0){activate(e->GetId().substr(13));event.StopPropagation();}}return;}if(event.GetType()=="click"){auto* e=event.GetTargetElement();while(e&&e!=impl_->document){if(e->GetId().find("earth-action-")==0){activate(e->GetId().substr(13));event.StopPropagation();return;}e=e->GetParentNode();}}}
bool NativeEarthPage::visible()const{return impl_->shown;}
bool NativeEarthPage::consume_return(){const bool result=impl_->returned;impl_->returned=false;return result;}
bool NativeEarthPage::can_continue()const{return impl_->coordinator&&impl_->coordinator->can_continue();}
bool NativeEarthPage::in_world()const{return impl_->mode==Impl::Mode::World&&impl_->shown;}
bool NativeEarthPage::preview_ready()const{return impl_->mode==Impl::Mode::Preview&&bool(impl_->candidate);}
bool NativeEarthPage::busy()const{return impl_->busy();}
bool NativeEarthPage::resources_ready()const{return bool(impl_->renderer)&&impl_->renderer->ready()&&bool(impl_->atlas);}
std::string NativeEarthPage::last_error()const{return impl_->error;}
std::string NativeEarthPage::world_id()const{return impl_->coordinator&&impl_->coordinator->active()?impl_->coordinator->active()->world_id:"";}
}
