#include "../../src/core/sonnheide.hpp"
#include "../../src/client/client.hpp"
#include "../../src/ui/ui.hpp"
#include <RmlUi/Core.h>
#include <SDL3/SDL.h>
#include <RmlUi/Core/Elements/ElementFormControl.h>
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <fstream>
#include <future>
#include <iostream>
#include <random>
#include <thread>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace ui=sonnheide::ui;
namespace client=sonnheide::client;
namespace fs=std::filesystem;
using Clock=std::chrono::steady_clock;
static constexpr char GEO_HASH[]="af9215d58ebc525b2d09654a89959829f09e6edc457f3666759cded37be4ecf6";
static fs::path utf8_file(std::string_view s){return fs::path(std::u8string(reinterpret_cast<const char8_t*>(s.data()),s.size()));}
static sonn::Uuid new_id(){std::random_device r;sonn::Uuid id{};for(auto& x:id)x=static_cast<std::uint8_t>(r());return id;}
static sonn::Hash new_seed(){std::random_device r;sonn::Hash s{};for(auto& x:s)x=static_cast<std::uint8_t>(r());return s;}
static std::string utf8_path(const fs::path& p){auto u=p.u8string();return {reinterpret_cast<const char*>(u.data()),u.size()};}
static std::string bytes_text(const sonn::Bytes& b){return {reinterpret_cast<const char*>(b.data()),b.size()};}
struct CandidateJob {sonn::CreationDraft draft; std::shared_ptr<std::atomic_bool> canceled; std::future<sonn::World> result;};
struct MapResult {int width{},height{};std::uint64_t revision{};std::vector<std::uint8_t> rgba;};
static sonn::DefinitionPackage admit_runtime(const fs::path& root){
  auto package=sonn::admit_definitions(root/"definitions.json",root);
  const std::vector<std::string> required_definitions={
    "data/content/combat_profiles_v1.json","data/content/economy_content_v1.json","data/content/life_profiles_v1.json","data/content/technology_runtime_v1.json",
    "data/contracts/civic_control_v1.json","data/contracts/decision_core_v1.json","data/contracts/economic_decisions_v1.json","data/contracts/emergence_algorithms_v1.json","data/contracts/game_v0_9.json","data/contracts/god_control_v1.json","data/contracts/interaction_runtime_v1.json","data/contracts/life_genetics_v1.json","data/contracts/natural_action_v1.json","data/contracts/pixel_world.json","data/contracts/political_decisions_v1.json","data/contracts/runtime_foundation_v1.json","data/contracts/settlement_lifecycle_v1.json","data/contracts/social_decisions_v1.json","data/contracts/surface_ecology.json","data/contracts/terrain_access_v1.json","data/contracts/tool_registry_v1.json","data/contracts/vegetation_generator_v1.json","data/contracts/warfare_decisions_v1.json"};
  if(package.files!=required_definitions)throw sonn::Error("DEFINITION_MISSING","Exact v0.9 required definitions must be admitted");
  package.asset_hash=sonn::admit_assets(root/"assets.json",root);
  if(package.hash!=sonn::hash_from_hex(SONN_DEFINITION_SHA))throw sonn::Error("DEFINITION_HASH","Runtime definitions differ from the compiled admitted package");
  if(package.asset_hash!=sonn::hash_from_hex(SONN_ASSET_SHA))throw sonn::Error("ASSET_HASH","Runtime assets differ from the compiled admitted package");
  return package;
}
class App {
 fs::path resources_,save_root_;
 sonn::DefinitionPackage definitions_;
 sonn::SaveStore saves_;
 sonn::WorldSession session_;
 std::shared_ptr<const sonn::Geography> geography_;
 std::future<std::shared_ptr<const sonn::Geography>> geo_job_;
 std::vector<CandidateJob> jobs_;
 std::optional<sonn::World> preview_;
 std::optional<sonn::CreationDraft> preview_draft_;
 std::optional<sonn::PreparedCommand> rename_;
 std::future<MapResult> map_job_;
 std::uint64_t map_generation_=0,preview_token_=0,command_sequence_=0;
 bool map_pending_=false,quit_=false,paused_=false,dragging_selection_=false,inspector_was_paused_=false,inspector_open_=false;
 std::vector<ui::Action> platform_actions_;
 ui::Screen load_origin_=ui::Screen::MainMenu;
 int speed_=1;double tick_accumulator_=0,presentation_seconds_=0;double select_start_x_=0,select_start_y_=0;
 sonn::GeoRect map_rect_{-180000000,180000000,-80000000,80000000};
 ui::View view_;
 sonn::Bytes arcade_font_;
 client::Client client_;
 std::unique_ptr<ui::Ui> ui_;
 bool smoke_=false,smoke_coordinates_checked_=false,smoke_map_focus_sent_=false; int smoke_stage_=0,smoke_camera_stage_=0,smoke_menu_phase_=0;double smoke_menu_live_seconds_=0,smoke_menu_frozen_seconds_=0; Clock::time_point smoke_start_=Clock::now();
 client::Camera smoke_camera_before_{};
 sonn::Hash smoke_camera_world_{};
 fs::path evidence_;
 std::vector<std::string> completed_actions_;
 sonn::Uuid counted_world_{};
public:
 App(fs::path resources,fs::path saves,bool smoke,fs::path evidence):resources_(std::move(resources)),save_root_(std::move(saves)),definitions_(admit_runtime(resources_)),saves_(save_root_),client_({resources_,1280,800,false}),smoke_(smoke),evidence_(std::move(evidence)) {
  Rml::SetSystemInterface(client_.system_interface());Rml::SetRenderInterface(client_.render_interface());Rml::SetFileInterface(client_.file_interface());
  if(!Rml::Initialise())throw sonn::Error("UI_INIT","RmlUi initialization failed");
  arcade_font_=sonn::read_file(resources_/"fonts/fusion-pixel-12px-proportional-zh_hans.otf",8388608);
  if(!Rml::LoadFontFace(arcade_font_.data(),static_cast<int>(arcade_font_.size()),"Sonn Arcade",Rml::Style::FontStyle::Normal,Rml::Style::FontWeight::Normal,true))throw sonn::Error("FONT_LOAD","Locked arcade pixel font could not be loaded");
  ui_=std::make_unique<ui::Ui>(resources_);auto d=client_.dimensions();std::string err;
  if(!ui_->initialize(d.pixel_w,d.pixel_h,d.density,err))throw sonn::Error("UI_DOCUMENT",err);
  view_.creation.can_select_theme=true;view_.creation.cell_mm=sonn::terrain_cell_size_mm;client_.audio_volume(view_.audio_volume/100.f);read_settings();refresh_saves();
  geo_job_=std::async(std::launch::async,[root=resources_]{return std::make_shared<const sonn::Geography>(root/"geo/gshhs_f.b",sonn::hash_from_hex(GEO_HASH));});
  if(smoke_){fs::create_directories(evidence_);std::error_code ec;fs::remove(evidence_/"native-flow.json",ec);std::ofstream(evidence_/"gpu.json")<<client_.gpu_parameters_json();}
 }
 ~App(){for(auto& j:jobs_)j.canceled->store(true);for(auto& j:jobs_)if(j.result.valid())j.result.wait();if(map_job_.valid())map_job_.wait();if(geo_job_.valid())geo_job_.wait();ui_.reset();Rml::Shutdown();}
 int run(){
  auto previous=Clock::now();
  while(!quit_){
   auto now=Clock::now();double dt=std::chrono::duration<double>(now-previous).count();previous=now;
   try{poll_jobs();}catch(const sonn::Error& x){error(x.code,x.what());}catch(const std::exception& x){error("INTERNAL_ERROR",x.what());}
   SDL_Event e;while(client_.poll_event(e))event(e);
   auto pending=ui_->take_actions();platform_actions_.insert(platform_actions_.end(),pending.begin(),pending.end());auto actions=std::move(platform_actions_);platform_actions_.clear();
   for(const auto& a:actions){try{act(a);}catch(const sonn::Error& x){error(x.code,x.what());}catch(const std::exception& x){error("INTERNAL_ERROR",x.what());}if(quit_)break;}
   if(quit_)break; // Exit is the last authority action; never write after its final checkpoint.
   if(session_.active()&&view_.screen==ui::Screen::World&&!paused_&&client_.visible()){
    tick_accumulator_+=std::min(dt,0.25)*20*speed_;
    // Bounded work budget carries remainder; no skipped authoritative ticks.
    unsigned n=0;while(tick_accumulator_>=1&&n++<256){session_.active()->advance_tick();tick_accumulator_-=1;}
   }
   sync_view();
   // Local animation has no simulation RNG/tick owner. Resume without a time jump.
   if(client_.visible()&&!view_.reduced_motion&&!(session_.active()&&view_.screen==ui::Screen::World&&paused_))presentation_seconds_+=std::clamp(dt,0.0,0.1);
   const sonn::World* w=view_.screen==ui::Screen::World&&session_.active()?&session_.active()->world():preview_?&*preview_:nullptr;
   view_.presentation_darkness=w&&w->age.current==sonn::Age::Darkness;
   // A load dialog opened from a world retains that world's presentation Age.
   if(view_.screen==ui::Screen::Load&&load_origin_==ui::Screen::World&&session_.active())view_.presentation_darkness=session_.active()->world().age.current==sonn::Age::Darkness;
   ui_->set_view(view_);ui_->update();client_.sync_text_input(ui_->context());
   std::optional<client::SurfaceView> surface;if(w)surface=client::surface_view(*w);
   auto v=ui_->viewport();bool map=view_.screen==ui::Screen::Creation&&view_.creation.mode==ui::CreationMode::Earth&&!preview_;
   client_.map_overlay(static_cast<int>(v.x),static_cast<int>(v.y),static_cast<int>(v.width),static_cast<int>(v.height),map);
   const auto age=w&&w->age.current==sonn::Age::Darkness?client::Age::Darkness:client::Age::Light;
   client_.begin_frame(surface?&*surface:nullptr,age,presentation_seconds_);
   client_.begin_ui();ui_->render();client_.end_frame();
   if(smoke_)smoke_step();
   std::this_thread::sleep_for(std::chrono::milliseconds(5));
  }
  if(smoke_){
   if(smoke_stage_!=54||!session_.active())throw sonn::Error("SMOKE_INTERRUPTED","The native flow was closed before all acceptance actions completed");
   auto durable=saves_.continue_world(definitions_);
   if(!durable||durable->name!="退出顺序 · Ähren"||durable->deterministic_hash()!=session_.active()->world().deterministic_hash())throw sonn::Error("SMOKE_EXIT_SAVE","Latest queued rename was not in the final exit checkpoint");
   sonn::Json::Array steps;for(auto& x:completed_actions_)steps.emplace_back(x);steps.emplace_back("exit-durable-readback");
   std::ofstream report(evidence_/"native-flow.json");report<<sonn::canonical_json(sonn::Json(sonn::Json::Object{{"actions",steps},{"renderer",client_.renderer_name()},{"world_hash",sonn::hex(durable->deterministic_hash())},{"world",durable->query()},{"status","pass"}}));report.flush();if(!report)throw sonn::Error("SMOKE_REPORT","Acceptance report could not be written");
  }
  persist_settings();return 0;
 }
private:
 void error(const std::string& key,const std::string& detail){view_.error_key=key;view_.error_detail=detail;view_.busy=false;std::cerr<<key<<": "<<detail<<'\n';}
 void refresh_saves(){view_.saves.clear();for(const auto& f:saves_.recovery_candidates()){
  try{auto w=saves_.recover(f,definitions_);view_.saves.push_back({f,w.name,""});}catch(const std::exception&){}
 }view_.can_continue=false;try{view_.can_continue=saves_.continue_world(definitions_).has_value();}catch(const std::exception& x){error("SAVE_RECOVERY",x.what());}}
 void read_settings(){auto p=save_root_/"preferences.json";if(!fs::exists(p))return;try{
  auto x=sonn::parse_json(bytes_text(sonn::read_file(p,65536)));auto l=x.at("locale").string();view_.locale=l=="de"?ui::Locale::German:l=="en"?ui::Locale::English:ui::Locale::Chinese;
  view_.reduced_motion=x.at("reduced_motion").boolean();view_.audio_volume=static_cast<int>(std::clamp<std::int64_t>(x.at("audio_volume").integer(),0,100));view_.fullscreen=x.at("fullscreen").boolean();if(x.object().contains("paused"))paused_=x.at("paused").boolean();if(x.object().contains("speed")){int v=static_cast<int>(x.at("speed").integer());if(v==1||v==4||v==16||v==64)speed_=v;}client_.audio_volume(view_.audio_volume/100.f);client_.fullscreen(view_.fullscreen);
 }catch(const std::exception& x){error("SETTINGS_INVALID",x.what());}}
 void persist_settings(){fs::create_directories(save_root_);auto p=save_root_/"preferences.json";auto tmp=p;tmp+=".tmp";
  sonn::Json j(sonn::Json::Object{{"locale",ui::locale_code(view_.locale)},{"reduced_motion",view_.reduced_motion},{"fullscreen",view_.fullscreen},{"audio_volume",view_.audio_volume},{"paused",inspector_open_?inspector_was_paused_:paused_},{"speed",speed_}});
  {std::ofstream f(tmp,std::ios::binary);f<<sonn::canonical_json(j);if(!f)throw sonn::Error("SETTINGS_WRITE","Preferences could not be written");}
#ifdef _WIN32
  if(!MoveFileExW(tmp.c_str(),p.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH))throw sonn::Error("SETTINGS_WRITE",std::to_string(GetLastError()));
#else
  std::error_code ec;fs::rename(tmp,p,ec);if(ec)throw sonn::Error("SETTINGS_WRITE",ec.message());
#endif
 }
 sonn::CreationDraft draft() const{sonn::CreationDraft d;d.kind=view_.creation.mode==ui::CreationMode::Earth?sonn::CreationKind::Earth:view_.creation.base==ui::BlankBase::Soil?sonn::CreationKind::BlankLand:sonn::CreationKind::BlankSea;d.name=view_.creation.name;constexpr std::array<std::string_view,8> themes{"snowfield","flower_meadow","maple_field","cherry_field","wetland","savanna","sonnheide_sacred","volcanic"};auto ti=std::find(themes.begin(),themes.end(),view_.creation.theme);if(ti==themes.end())throw sonn::Error("INVALID_THEME",view_.creation.theme);d.soil_theme=static_cast<std::uint8_t>(ti-themes.begin());d.core_width=view_.creation.width;d.core_height=view_.creation.height;
  auto u=[](double x){if(!std::isfinite(x)||x<-360||x>540)throw sonn::Error("EARTH_BOUNDS","Geographic coordinate out of range");return static_cast<std::int32_t>(std::llround(x*1000000));};
  auto west=u(view_.creation.west),east=u(view_.creation.east);std::int64_t shift=(static_cast<std::int64_t>(west)+180000000)/360000000;if(west< -180000000)shift=(static_cast<std::int64_t>(west)+180000000-359999999)/360000000;west=static_cast<std::int32_t>(west-shift*360000000);east=static_cast<std::int32_t>(east-shift*360000000);d.selection={west,east,u(view_.creation.south),u(view_.creation.north)};d.world_id=new_id();d.seed=new_seed();return d;
 }
 void invalidate_preview(){for(auto& j:jobs_)j.canceled->store(true);session_.cancel();preview_.reset();preview_draft_.reset();view_.creation.preview_ready=false;view_.creation.busy=false;view_.creation.generation=++preview_token_;map_pending_=true;}
 void request_preview(){invalidate_preview();auto d=session_.begin(draft());view_.creation.generation=d.draft_generation;view_.creation.busy=true;
  auto canceled=std::make_shared<std::atomic_bool>(false);auto def=definitions_;auto geo=geography_;
  if(d.kind==sonn::CreationKind::Earth&&!geo)throw sonn::Error("GEOGRAPHY_LOADING","Geographic source is still being admitted");
  jobs_.push_back({d,canceled,std::async(std::launch::async,[d,def,geo,canceled]{return sonn::create_candidate(d,def,geo.get(),[canceled]{return canceled->load();});})});
 }
 void poll_jobs(){
  if(geo_job_.valid()&&geo_job_.wait_for(std::chrono::milliseconds(0))==std::future_status::ready){try{geography_=geo_job_.get();view_.creation.earth_available=true;map_pending_=true;}catch(const sonn::Error& e){error(e.code,e.what());}}
  for(auto it=jobs_.begin();it!=jobs_.end();){if(it->result.wait_for(std::chrono::milliseconds(0))!=std::future_status::ready){++it;continue;}
   try{auto w=it->result.get();if(!it->canceled->load()&&session_.accepts(it->draft)){
    preview_=std::move(w);preview_draft_=it->draft;view_.creation.preview_ready=true;view_.creation.busy=false;view_.creation.preview_token=++preview_token_;client_.frame_surface(client::surface_view(*preview_));
   }}catch(const sonn::Error& e){if(!it->canceled->load()&&session_.accepts(it->draft)){view_.creation.busy=false;error(e.code,e.what());}}
   it=jobs_.erase(it);
  }
  if(map_job_.valid()&&map_job_.wait_for(std::chrono::milliseconds(0))==std::future_status::ready){try{auto m=map_job_.get();if(m.revision==map_generation_)client_.set_map_rgba(m.width,m.height,m.rgba,m.revision);}catch(const std::exception& e){error("MAP_SAMPLE",e.what());}}
  if(map_pending_&&geography_&&!map_job_.valid()&&view_.screen==ui::Screen::Creation&&view_.creation.mode==ui::CreationMode::Earth){
   map_pending_=false;auto geo=geography_;auto rect=map_rect_;auto selection=draft().selection;auto gen=++map_generation_;
   auto v=ui_->viewport();int w=std::clamp(static_cast<int>(v.width),512,1600);int h=std::clamp(static_cast<int>(v.height),256,1000);
   map_job_=std::async(std::launch::async,[geo,rect,selection,gen,w,h]{auto values=geo->sample(rect,w,h);MapResult result{w,h,gen,{}};result.rgba.resize(static_cast<std::size_t>(w)*h*4);
    for(int y=0;y<h;y++)for(int x=0;x<w;x++){double lon=rect.west_udeg+(x+.5)*(static_cast<double>(rect.east_unwrapped_udeg)-rect.west_udeg)/w;double lat=rect.north_udeg-(y+.5)*(static_cast<double>(rect.north_udeg)-rect.south_udeg)/h;
     // Sample source rows run south-to-north; display map north-up. Fill-only selected region, no outline.
     bool land=values[(h-1-y)*w+x]!=0;bool selected=lon>=selection.west_udeg&&lon<=selection.east_unwrapped_udeg&&lat>=selection.south_udeg&&lat<=selection.north_udeg;
     auto i=static_cast<std::size_t>(y*w+x)*4;std::array<int,3> rgb=land?std::array<int,3>{59,132,106}:std::array<int,3>{35,67,121};
     double shade=selected?1.20:.68;for(int k=0;k<3;k++)result.rgba[i+k]=static_cast<std::uint8_t>(std::clamp(rgb[k]*shade,0.,255.));result.rgba[i+3]=255;
    }return result;});
  }
 }
 void apply_command(const sonn::Command& command){auto* a=session_.active();if(!a)throw sonn::Error("NO_WORLD","No active world");sonn::Envelope e{a->world().id,session_.generation(),a->world().revision,++command_sequence_,"ui-"+std::to_string(command_sequence_)};a->commit(a->prepare(e,command));view_.world.dirty=true;}
 void act(const ui::Action& a){using A=ui::ActionKind;
  bool world_mutation=a.kind==A::PreviewWorldName||a.kind==A::CommitWorldName||a.kind==A::SetAgeLight||a.kind==A::SetAgeDarkness||a.kind==A::SetAgeAutomatic;
  if(world_mutation&&a.generation!=session_.generation())throw sonn::Error("STALE_GENERATION","The interface belongs to an earlier world session");
  switch(a.kind){
  case A::NewWorld:invalidate_preview();view_.screen=ui::Screen::Creation;view_.creation.name="Sonnheide";break;
  case A::ContinueWorld:session_.load_continue(saves_,definitions_);show_world();break;
  case A::OpenLoad:if(view_.screen==ui::Screen::Load)break;load_origin_=view_.screen;refresh_saves();view_.selected_save.clear();view_.screen=ui::Screen::Load;break;
  case A::CancelLoad:view_.screen=load_origin_;break;
  case A::SelectSave:view_.selected_save=a.text;break;
  case A::LoadSelected:{if(view_.selected_save.empty())throw sonn::Error("NO_SAVE_SELECTED","Select a checkpoint");session_.load_checkpoint(view_.selected_save,saves_,definitions_);show_world();break;}
  case A::SaveWorld:if(session_.active()){auto c=saves_.save(session_.active()->world(),definitions_);if(c.status!=sonn::SaveStatus::Committed)throw sonn::Error(c.code,"World checkpoint was not confirmed durable");view_.world.dirty=false;view_.status_key="saved";refresh_saves();}break;
  case A::ReturnMenu:if(session_.active()){auto c=saves_.save(session_.active()->world(),definitions_);if(c.status!=sonn::SaveStatus::Committed)throw sonn::Error(c.code,"Return to menu stopped: save could not be confirmed");}session_.return_to_menu();invalidate_preview();view_.screen=ui::Screen::MainMenu;refresh_saves();break;
  case A::Exit:if(session_.active()){auto c=saves_.save(session_.active()->world(),definitions_);if(c.status!=sonn::SaveStatus::Committed)throw sonn::Error(c.code,"Exit stopped: save could not be confirmed");}quit_=true;break;
  case A::SetCreationMode:invalidate_preview();view_.creation.mode=a.text=="earth"?ui::CreationMode::Earth:ui::CreationMode::Blank;break;
  case A::SetBlankBase:invalidate_preview();view_.creation.base=a.text=="ocean"?ui::BlankBase::Ocean:ui::BlankBase::Soil;break;
  case A::SetCreationSize:invalidate_preview();view_.creation.width=static_cast<int>(a.a);view_.creation.height=static_cast<int>(a.b);break;
  case A::SetCreationTheme:invalidate_preview();view_.creation.theme=a.text;break;
  case A::SetCreationName:invalidate_preview();view_.creation.name=a.text;break;
  case A::SetEarthBounds:invalidate_preview();view_.creation.west=a.a;view_.creation.south=a.b;view_.creation.east=a.c;view_.creation.north=a.d;break;
  case A::MapZoom:map_zoom(a.a);break;
  case A::MapPan:map_pan(a.a*.15,a.b*.15);break;
  case A::RequestPreview:request_preview();break;
  case A::CreateWorld:{if(!preview_||!preview_draft_||a.token!=view_.creation.preview_token||a.generation!=view_.creation.generation)throw sonn::Error("PREVIEW_STALE","Create preview is no longer current");sonn::Checkpoint c;if(!session_.publish(*preview_draft_,*preview_,saves_,definitions_,c))throw sonn::Error(c.code,"Initial checkpoint publication failed");preview_.reset();preview_draft_.reset();show_world();refresh_saves();break;}
  case A::CancelCreation:invalidate_preview();view_.screen=session_.active()?ui::Screen::World:ui::Screen::MainMenu;break;
  case A::SetLocale:view_.locale=a.text=="de"?ui::Locale::German:a.text=="en"?ui::Locale::English:ui::Locale::Chinese;persist_settings();break;
  case A::SetFullscreen:view_.fullscreen=a.a!=0;client_.fullscreen(view_.fullscreen);break;
  case A::SetReducedMotion:view_.reduced_motion=a.a!=0;break;
  case A::SetAudioVolume:view_.audio_volume=std::clamp(static_cast<int>(a.a),0,100);client_.audio_volume(view_.audio_volume/100.f);break;
  case A::CameraHome:if(session_.active())client_.frame_surface(client::surface_view(session_.active()->world()));break;
  case A::SetPaused:paused_=a.a!=0;tick_accumulator_=0;break;
  case A::SetSpeed:speed_=static_cast<int>(a.a);if(speed_!=1&&speed_!=4&&speed_!=16&&speed_!=64)throw sonn::Error("BAD_SPEED","Unsupported simulation speed");break;
  case A::OpenWorldInspector:if(inspector_open_)break;inspector_open_=true;inspector_was_paused_=paused_;paused_=true;tick_accumulator_=0;break;
  case A::CloseWorldInspector:if(inspector_open_)paused_=inspector_was_paused_;inspector_open_=false;rename_.reset();view_.world.rename_preview_ready=false;break;
  case A::SetWorldNameDraft:rename_.reset();view_.world.rename_preview_ready=false;break;
  case A::PreviewWorldName:{if(!session_.active())break;auto* auth=session_.active();sonn::Command c{sonn::Command::Kind::RenameWorld,a.text};sonn::Envelope e{auth->world().id,session_.generation(),auth->world().revision,++command_sequence_,"rename-"+std::to_string(command_sequence_)};rename_=auth->prepare(e,c);view_.world.rename_preview_ready=true;view_.world.rename_preview_token=++preview_token_;view_.world.rename_consequences=sonn::canonical_json(rename_->preview);break;}
  case A::CommitWorldName:if(!rename_||a.token!=view_.world.rename_preview_token||a.generation!=session_.generation())throw sonn::Error("PREVIEW_STALE","Rename preview is no longer current");session_.active()->commit(*rename_);rename_.reset();view_.world.rename_preview_ready=false;view_.world.dirty=true;break;
  case A::CancelWorldName:rename_.reset();view_.world.rename_preview_ready=false;break;
  case A::SetAgeLight:case A::SetAgeDarkness:{sonn::Command c{sonn::Command::Kind::SetAge};c.age=a.kind==A::SetAgeLight?sonn::Age::Light:sonn::Age::Darkness;apply_command(c);break;}
  case A::SetAgeAutomatic:{sonn::Command c{sonn::Command::Kind::SetAgeAutomatic};c.boolean=a.a!=0;apply_command(c);break;}
  case A::DismissError:view_.error_key.clear();view_.error_detail.clear();break;
  default:break; // Local UI open/close/tab intents have no world mutation.
  }
 }
 void show_world(){if(!session_.active())throw sonn::Error("NO_WORLD","Continue has no compatible world");view_.screen=ui::Screen::World;if(inspector_open_)paused_=inspector_was_paused_;inspector_open_=false;command_sequence_=std::max(command_sequence_,session_.active()->world().input_sequence);tick_accumulator_=0;rename_.reset();client_.frame_surface(client::surface_view(session_.active()->world()));}
 void sync_view(){if(view_.creation.mode==ui::CreationMode::Earth){const auto micro=[](double value){return static_cast<std::int64_t>(std::llround(value*1000000));};const auto span=micro(view_.creation.east)-micro(view_.creation.west),lat=micro(view_.creation.north)-micro(view_.creation.south);if(span>0&&lat>0&&view_.creation.width>0&&view_.creation.width<=sonn::maximum_core_cells){const auto h=(static_cast<std::int64_t>(view_.creation.width)*lat+span-1)/span;view_.creation.height=static_cast<int>(std::clamp<std::int64_t>(h,1,100000));}}if(auto* a=session_.active()){const auto& w=a->world();view_.world.session=session_.generation();view_.world.name=w.name;view_.world.revision=w.revision;view_.world.day=w.tick/12000;if(counted_world_!=w.id){counted_world_=w.id;view_.world.cells=static_cast<std::uint64_t>(w.terrain.width-2*w.terrain.guard)*(w.terrain.height-2*w.terrain.guard);view_.world.dry_cells=0;for(int z=w.terrain.guard;z<w.terrain.height-w.terrain.guard;z++)for(int x=w.terrain.guard;x<w.terrain.width-w.terrain.guard;x++)if(sonn::terrain_height_mm[static_cast<unsigned>(w.terrain.at(x,z).kind)]>0)++view_.world.dry_cells;}view_.world.darkness=w.age.current==sonn::Age::Darkness;view_.world.automatic_age=w.age.automatic;view_.world.age_ticks_remaining=w.age.remaining_tick;view_.world.paused=paused_;view_.world.speed=speed_;}}
 void map_zoom(double amount){double factor=amount>0?.7:1.0/.7;double cx=(static_cast<double>(map_rect_.west_udeg)+map_rect_.east_unwrapped_udeg)/2;double cy=(static_cast<double>(map_rect_.south_udeg)+map_rect_.north_udeg)/2;double w=std::clamp((map_rect_.east_unwrapped_udeg-static_cast<double>(map_rect_.west_udeg))*factor,500.,360000000.);double h=std::clamp((map_rect_.north_udeg-static_cast<double>(map_rect_.south_udeg))*factor,500.,160000000.);map_rect_={static_cast<int>(cx-w/2),static_cast<int>(cx+w/2),static_cast<int>(std::max(-80000000.,cy-h/2)),static_cast<int>(std::min(80000000.,cy+h/2))};map_pending_=true;++map_generation_;}
 void map_pan(double dx,double dy){double w=map_rect_.east_unwrapped_udeg-static_cast<double>(map_rect_.west_udeg),h=map_rect_.north_udeg-static_cast<double>(map_rect_.south_udeg);std::int64_t x=static_cast<std::int64_t>(std::clamp(dx,-.5,.5)*w),y=static_cast<std::int64_t>(std::clamp(dy,-.5,.5)*h);std::int64_t west=map_rect_.west_udeg+x;while(west< -180000000)west+=360000000;while(west>=180000000)west-=360000000;map_rect_.west_udeg=static_cast<int>(west);map_rect_.east_unwrapped_udeg=static_cast<int>(west+w);if(map_rect_.south_udeg+y>=-80000000&&map_rect_.north_udeg+y<=80000000){map_rect_.south_udeg+=static_cast<int>(y);map_rect_.north_udeg+=static_cast<int>(y);}map_pending_=true;++map_generation_;}
 void event(const SDL_Event& e){if(e.type==SDL_EVENT_QUIT){auto prior=ui_->take_actions();platform_actions_.insert(platform_actions_.end(),prior.begin(),prior.end());platform_actions_.push_back({ui::ActionKind::Exit});return;}
  if(e.type==SDL_EVENT_WINDOW_FOCUS_LOST)dragging_selection_=false;
  if(e.type==SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED||e.type==SDL_EVENT_WINDOW_DISPLAY_SCALE_CHANGED){auto d=client_.dimensions();ui_->resize(d.pixel_w,d.pixel_h,d.density);map_pending_=true;}
  bool consumed=client_.process_ui_event(e,ui_->context());
  if(e.type==SDL_EVENT_MOUSE_BUTTON_UP)client_.process_camera_event(e);
  auto v=ui_->viewport();auto dim=client_.dimensions();float mx=0,my=0;
  const bool pointer=e.type==SDL_EVENT_MOUSE_MOTION||e.type==SDL_EVENT_MOUSE_BUTTON_DOWN||e.type==SDL_EVENT_MOUSE_BUTTON_UP||e.type==SDL_EVENT_MOUSE_WHEEL;
  if(e.type==SDL_EVENT_MOUSE_MOTION){mx=e.motion.x;my=e.motion.y;}else if(e.type==SDL_EVENT_MOUSE_BUTTON_DOWN||e.type==SDL_EVENT_MOUSE_BUTTON_UP){mx=e.button.x;my=e.button.y;}else if(e.type==SDL_EVENT_MOUSE_WHEEL){mx=e.wheel.mouse_x;my=e.wheel.mouse_y;}else SDL_GetMouseState(&mx,&my);
  double px=mx,py=my;
  bool inside=px>=v.x&&px<v.x+v.width&&py>=v.y&&py<v.y+v.height;
  if(ui_->text_input_focused()||ui_->blocks_world_input()||(pointer&&ui_->captures_pointer(static_cast<int>(mx*dim.density),static_cast<int>(my*dim.density)))){if(e.type==SDL_EVENT_MOUSE_BUTTON_UP)dragging_selection_=false;return;}
  if(view_.screen==ui::Screen::Creation&&view_.creation.mode==ui::CreationMode::Earth&&!preview_){
   if(e.type==SDL_EVENT_MOUSE_WHEEL&&inside){map_zoom(e.wheel.y);return;}
   if(e.type==SDL_EVENT_MOUSE_MOTION&&inside&&(e.motion.state&SDL_BUTTON_RMASK)){map_pan(-e.motion.xrel/v.width,e.motion.yrel/v.height);return;}
   if(e.type==SDL_EVENT_MOUSE_BUTTON_DOWN&&e.button.button==SDL_BUTTON_LEFT&&inside){dragging_selection_=true;select_start_x_=px;select_start_y_=py;}
   if((e.type==SDL_EVENT_MOUSE_MOTION||e.type==SDL_EVENT_MOUSE_BUTTON_UP)&&dragging_selection_){double ax=std::clamp((select_start_x_-v.x)/v.width,0.,1.),ay=std::clamp((select_start_y_-v.y)/v.height,0.,1.);double bx=std::clamp((px-v.x)/v.width,0.,1.),by=std::clamp((py-v.y)/v.height,0.,1.);
    double w=(map_rect_.east_unwrapped_udeg-static_cast<double>(map_rect_.west_udeg))/1000000.,h=(map_rect_.north_udeg-static_cast<double>(map_rect_.south_udeg))/1000000.;view_.creation.west=map_rect_.west_udeg/1000000.+std::min(ax,bx)*w;view_.creation.east=view_.creation.west+std::max(.0001,std::min(60.,std::abs(ax-bx)*w));view_.creation.north=map_rect_.north_udeg/1000000.-std::min(ay,by)*h;view_.creation.south=view_.creation.north-std::max(.0001,std::min(60.,std::abs(ay-by)*h));map_pending_=true;
    if(e.type==SDL_EVENT_MOUSE_BUTTON_UP){dragging_selection_=false;invalidate_preview();}return;
   }return;
  }
  if(!consumed&&(view_.screen==ui::Screen::World||preview_)){if(e.type==SDL_EVENT_KEY_DOWN&&e.key.key==SDLK_SPACE){paused_=!paused_;tick_accumulator_=0;}else client_.process_camera_event(e);}
 }
 bool exercise_native_camera(){
  auto v=ui_->viewport();const float cx=static_cast<float>(v.x+v.width/2),cy=static_cast<float>(v.y+v.height/2);
  auto push=[](SDL_Event e){if(!SDL_PushEvent(&e))throw sonn::Error("SMOKE_INPUT","SDL acceptance event could not be queued");};
  auto button=[&](std::uint32_t type,std::uint8_t which){SDL_Event e{};e.type=type;e.button.button=which;e.button.x=cx;e.button.y=cy;push(e);};
  auto motion=[&](float dx,float dy,SDL_MouseButtonFlags mask){SDL_Event e{};e.type=SDL_EVENT_MOUSE_MOTION;e.motion.x=cx+dx;e.motion.y=cy+dy;e.motion.xrel=dx;e.motion.yrel=dy;e.motion.state=mask;push(e);};
  auto wheel=[&](float x,float y){SDL_Event e{};e.type=SDL_EVENT_MOUSE_WHEEL;e.wheel.y=2;e.wheel.mouse_x=x;e.wheel.mouse_y=y;push(e);};
  auto key=[&](SDL_Keycode code){SDL_Event e{};e.type=SDL_EVENT_KEY_DOWN;e.key.key=code;push(e);e.type=SDL_EVENT_KEY_UP;push(e);};
  auto same=[](double a,double b){return std::abs(a-b)<.00001;};
  auto unchanged=[&]{const auto& a=client_.camera();const auto& b=smoke_camera_before_;return same(a.yaw_deg,b.yaw_deg)&&same(a.pitch_deg,b.pitch_deg)&&same(a.distance_m,b.distance_m)&&same(a.target.x,b.target.x)&&same(a.target.y,b.target.y)&&same(a.target.z,b.target.z);};
  const auto& c=client_.camera();
  switch(smoke_camera_stage_++){
  case 0:act({ui::ActionKind::SetPaused,0,0,"",1});smoke_camera_world_=session_.active()->world().deterministic_hash();smoke_camera_before_=c;button(SDL_EVENT_MOUSE_BUTTON_DOWN,SDL_BUTTON_MIDDLE);motion(100,-40,SDL_BUTTON_MMASK);button(SDL_EVENT_MOUSE_BUTTON_UP,SDL_BUTTON_MIDDLE);return false;
  case 1:if(!same(c.yaw_deg,smoke_camera_before_.yaw_deg+32)||!same(c.pitch_deg,smoke_camera_before_.pitch_deg+9.6))throw sonn::Error("SMOKE_ORBIT","Native middle drag did not reach camera");smoke_camera_before_=c;wheel(cx,cy);return false;
  case 2:if(!same(c.distance_m,smoke_camera_before_.distance_m*std::exp(-.3)))throw sonn::Error("SMOKE_ZOOM","Native wheel did not zoom camera");smoke_camera_before_=c;button(SDL_EVENT_MOUSE_BUTTON_DOWN,SDL_BUTTON_RIGHT);motion(20,0,SDL_BUTTON_RMASK);button(SDL_EVENT_MOUSE_BUTTON_UP,SDL_BUTTON_RIGHT);return false;
  case 3:if((same(c.target.x,smoke_camera_before_.target.x)&&same(c.target.z,smoke_camera_before_.target.z))||!same(c.target.y,smoke_camera_before_.target.y))throw sonn::Error("SMOKE_PAN","Native right drag did not pan on ground");smoke_camera_before_=c;key(SDLK_Q);return false;
  case 4:if(!same(c.yaw_deg,smoke_camera_before_.yaw_deg-8))throw sonn::Error("SMOKE_KEY","Native Q rotation failed");key(SDLK_E);return false;
  case 5:{if(!same(c.yaw_deg,smoke_camera_before_.yaw_deg))throw sonn::Error("SMOKE_KEY","Native E rotation failed");auto surface=client::surface_view(session_.active()->world());auto hit=client_.pick(surface,cx,cy);if(!hit||!same(hit->point.y,2)||!same(hit->normal.y,1)||client_.pick(surface,v.x-1,v.y-1))throw sonn::Error("SMOKE_PICK","Screen ray did not hit the same soil top or rejected viewport incorrectly");auto support=session_.active()->world().terrain.support(static_cast<std::int64_t>(std::llround(hit->point.x*1000)),static_cast<std::int64_t>(std::llround(hit->point.z*1000)));if(!support||support->height_mm!=2000||support->wet)throw sonn::Error("SMOKE_PICK_SUPPORT","Pick and core support differ");auto* tab=ui_->context().GetDocument(0)->GetElementById("tab-observe");auto p=tab->GetAbsoluteOffset(Rml::Box::BORDER),s=tab->GetBox().GetSize(Rml::Box::BORDER);auto density=client_.dimensions().density;smoke_camera_before_=c;wheel((p.x+s.x/2)/density,(p.y+s.y/2)/density);return false;}
  case 6:if(!unchanged())throw sonn::Error("SMOKE_UI_CAPTURE","Wheel on toolbar changed the camera");button(SDL_EVENT_MOUSE_BUTTON_DOWN,SDL_BUTTON_MIDDLE);{SDL_Event e{};e.type=SDL_EVENT_WINDOW_FOCUS_LOST;push(e);}motion(100,0,SDL_BUTTON_MMASK);return false;
  case 7:if(!unchanged())throw sonn::Error("SMOKE_FOCUS","Lost focus retained camera drag");ui_->activate("world-info");return false;
  case 8:{auto* input=ui_->context().GetDocument(0)->GetElementById("world-name-draft");input->Focus();key(SDLK_W);return false;}
  case 9:if(!unchanged())throw sonn::Error("SMOKE_TEXT_CAPTURE","Text/modal input changed the camera");ui_->activate("world-close");return false;
  case 10:key(SDLK_W);return false;
  case 11:{if(unchanged())throw sonn::Error("SMOKE_KEY_RELEASE","Closing the modal did not release world keyboard input");if(session_.active()->world().deterministic_hash()!=smoke_camera_world_)throw sonn::Error("SMOKE_CAMERA_WORLD","Camera/UI input changed authoritative World");std::ofstream report(evidence_/"camera-input.json");report<<sonn::canonical_json(sonn::Json(sonn::Json::Object{{"status","pass"},{"path","SDL_PushEvent -> App input routing -> native camera -> screen ray -> core terrain support"},{"orbit",true},{"wheel",true},{"pan",true},{"qe",true},{"toolbar_wheel_isolated",true},{"focus_loss_releases_drag",true},{"text_modal_isolated",true},{"modal_close_releases_keyboard",true},{"pick_top_mm",2000},{"outside_viewport_rejected",true},{"world_hash_unchanged",sonn::hex(smoke_camera_world_)}}));report.flush();if(!report)throw sonn::Error("SMOKE_REPORT","Camera input report could not be written");return true;}
  default:return true;
  }
 }
 void smoke_step(){
  static int frame_count=0;static auto stage_time=Clock::now();
  if(std::chrono::duration<double>(Clock::now()-smoke_start_).count()>180)throw sonn::Error("SMOKE_TIMEOUT","Native flow did not complete");
  if(!view_.error_key.empty() && !(smoke_stage_>=43 && smoke_stage_<=46 && view_.error_key=="CREATION_DIMENSIONS") && !(smoke_stage_==20&&view_.error_key=="EARTH_BOUNDS"))throw sonn::Error("SMOKE_ERROR",view_.error_key+": "+view_.error_detail);
  if(++frame_count<8||std::chrono::duration<double>(Clock::now()-stage_time).count()<.3)return;
  auto step=[&](std::string name){completed_actions_.push_back(std::move(name));++smoke_stage_;frame_count=0;stage_time=Clock::now();};
  switch(smoke_stage_){
  case 0:
   if(smoke_menu_phase_==0){client_.request_screenshot(evidence_/"main-menu");smoke_menu_live_seconds_=presentation_seconds_;smoke_menu_phase_=1;break;}
   if(smoke_menu_phase_==1){if(presentation_seconds_-smoke_menu_live_seconds_<1.5)break;client_.request_screenshot(evidence_/"main-menu-motion");act({ui::ActionKind::SetReducedMotion,0,0,"",1});smoke_menu_frozen_seconds_=presentation_seconds_;smoke_menu_phase_=2;break;}
   if(smoke_menu_phase_==2){client_.request_screenshot(evidence_/"main-menu-frozen-a");smoke_menu_phase_=3;frame_count=0;break;}
   if(smoke_menu_phase_==3){if(presentation_seconds_!=smoke_menu_frozen_seconds_||session_.active()||preview_)throw sonn::Error("SMOKE_PRESENTATION","Reduced motion changed clock or menu created authority");client_.request_screenshot(evidence_/"main-menu-frozen-b");std::ofstream report(evidence_/"presentation-motion.json");report<<sonn::canonical_json(sonn::Json(sonn::Json::Object{{"status","pass"},{"local_clock_frozen",true},{"menu_creates_world",false},{"live_advance_ms",static_cast<std::int64_t>((smoke_menu_frozen_seconds_-smoke_menu_live_seconds_)*1000)},{"image_comparison_required",true}}));report.flush();if(!report)throw sonn::Error("SMOKE_REPORT","Presentation report write failed");completed_actions_.push_back("menu-live-motion-and-reduced-clock");act({ui::ActionKind::SetReducedMotion,0,0,"",0});smoke_menu_phase_=4;break;}
   ui_->activate("new-world");step("menu-new");break;
  case 1:if(view_.screen==ui::Screen::Creation){ui_->activate("preview");step("blank-preview-request");}break;
  case 2:if(preview_){client_.request_screenshot(evidence_/"blank-preview");step("blank-preview-gpu");}break;
  case 3:ui_->activate("create");step("blank-create");break;
  case 4:if(session_.active()&&exercise_native_camera()){client_.camera().yaw_deg=155;client_.camera().pitch_deg=18;client_.camera().distance_m=80;ui_->activate("tab-world");step("native-camera-pick-and-near-light");}break;
  case 5:client_.request_screenshot(evidence_/"blank-oblique");step("oblique-gpu");break;
  case 6:ui_->activate("age-darkness");step("set-darkness");break;
  case 7:client_.request_screenshot(evidence_/"darkness");step("dark-gpu");break;
  case 8:ui_->activate("save");ui_->activate("return-menu");step("save-return");break;
  case 9:if(view_.screen==ui::Screen::MainMenu){ui_->activate("continue-world");step("continue");}break;
  case 10:if(session_.active()){ui_->activate("tab-settings");step("settings-open");}break;
  case 11:client_.request_screenshot(evidence_/"settings-zh");step("settings-zh-gpu");break;
  case 12:ui_->activate("locale-en");step("english");break;
  case 13:client_.request_screenshot(evidence_/"settings-en");step("settings-en-gpu");break;
  case 14:ui_->activate("locale-de");step("german");break;
  case 15:client_.request_screenshot(evidence_/"settings-de");step("settings-de-gpu");break;
  case 16:ui_->activate("settings-close");ui_->activate("return-menu");step("settings-return");break;
  case 17:if(view_.screen==ui::Screen::MainMenu){ui_->activate("new-world");step("earth-new");}break;
  case 18:if(view_.screen==ui::Screen::Creation&&geography_){ui_->activate("earth-mode");step("earth-mode");}break;
  case 19:act({ui::ActionKind::SetEarthBounds,0,0,"",600,49,2,55});step("invalid-earth-coordinate");break;
  case 20:if(!smoke_coordinates_checked_){if(view_.error_key=="EARTH_BOUNDS"){ui_->activate("error-dismiss");act({ui::ActionKind::SetEarthBounds,0,0,"",-6,49,2,55});smoke_coordinates_checked_=true;completed_actions_.push_back("invalid-coordinate-corrected-without-exit");}}else if(view_.error_key.empty()&&!map_pending_&&!map_job_.valid()){
   if(!smoke_map_focus_sent_){auto v=ui_->viewport();SDL_Event e{};e.type=SDL_EVENT_MOUSE_BUTTON_DOWN;e.button.button=SDL_BUTTON_LEFT;e.button.x=v.x+v.width*.5f;e.button.y=v.y+v.height*.5f;if(!SDL_PushEvent(&e))throw sonn::Error("SMOKE_INPUT","Map mouse event could not be queued");e={};e.type=SDL_EVENT_WINDOW_FOCUS_LOST;if(!SDL_PushEvent(&e))throw sonn::Error("SMOKE_INPUT","Map focus event could not be queued");e={};e.type=SDL_EVENT_MOUSE_MOTION;e.motion.x=v.x+v.width*.6f;e.motion.y=v.y+v.height*.6f;if(!SDL_PushEvent(&e))throw sonn::Error("SMOKE_INPUT","Map motion could not be queued");smoke_map_focus_sent_=true;break;}
   if(dragging_selection_||view_.creation.west!=-6||view_.creation.south!=49||view_.creation.east!=2||view_.creation.north!=55)throw sonn::Error("SMOKE_MAP_FOCUS","Lost focus retained Earth selection drag");completed_actions_.push_back("earth-selection-focus-loss-released");client_.request_screenshot(evidence_/"earth-map");step("earth-map-gpu");}break;
  case 21:ui_->activate("preview");step("earth-preview-request");break;
  case 22:if(preview_){client_.request_screenshot(evidence_/"earth-preview");step("earth-preview-gpu");}break;
  case 23:ui_->activate("create");step("earth-create");break;
  case 24:if(session_.active()&&view_.screen==ui::Screen::World){client_.request_screenshot(evidence_/"earth-world");step("earth-world-gpu");}break;
  case 25:act({ui::ActionKind::SaveWorld});client_.camera().yaw_deg=295;client_.camera().pitch_deg=40;client_.camera().distance_m=12;step("near-material-view");break;
  case 26:client_.request_screenshot(evidence_/"pixel-near-full");step("full-material-gpu");break;
  case 27:client_.material_diagnostic(client::MaterialDiagnostic::MeanBase);step("base-diagnostic");break;
  case 28:client_.request_screenshot(evidence_/"pixel-near-mean-base");step("base-diagnostic-gpu");break;
  case 29:client_.material_diagnostic(client::MaterialDiagnostic::Full);ui_->activate("age-darkness");step("near-dark-pixel");break;
  case 30:client_.request_screenshot(evidence_/"pixel-near-darkness");step("near-dark-gpu");break;
  case 31:ui_->activate("age-light");step("restore-light-pixel");break;
  case 32:client_.request_screenshot(evidence_/"pixel-near-light");step("near-light-gpu");break;
  case 33:client_.material_diagnostic(client::MaterialDiagnostic::Full);ui_->activate("world-info");step("world-draft-open");break;
  case 34:{auto* input=dynamic_cast<Rml::ElementFormControl*>(ui_->context().GetDocument(0)->GetElementById("world-name-draft"));if(!input)throw sonn::Error("SMOKE_INPUT","Native world name input missing");input->SetValue("");input->Focus();ui_->context().ProcessTextInput("新世界 · Ähren");step("native-utf8-name-input");break;}
  case 35:ui_->activate("rename-preview");step("rename-preview");break;
  case 36:if(view_.world.rename_preview_ready){client_.request_screenshot(evidence_/"world-name-consequences");step("rename-consequences-gpu");}break;
  case 37:ui_->activate("rename-commit");step("rename-commit");break;
  case 38:if(session_.active()->world().name!="新世界 · Ähren")throw sonn::Error("SMOKE_RENAME","Displayed name draft was not committed");ui_->activate("world-close");act({ui::ActionKind::SaveWorld});std::ofstream(evidence_/"gpu-world.json")<<client_.gpu_parameters_json();step("rename-durable");break;
  case 39:act({ui::ActionKind::NewWorld});act({ui::ActionKind::SetCreationMode,0,0,"blank"});ui_->activate("preview");step("new-candidate-while-world-retained");break;
  case 40:if(preview_){ui_->activate("cancel-create");step("cancel-candidate");}break;
  case 41:if(view_.screen==ui::Screen::World){if(session_.active()->world().name!="新世界 · Ähren"||saves_.continue_world(definitions_)->name!="新世界 · Ähren")throw sonn::Error("SMOKE_CANCEL","Cancel replaced the retained World or Continue");act({ui::ActionKind::NewWorld});act({ui::ActionKind::SetCreationSize,0,0,"",1,1});step("invalid-dimensions");}break;
  case 42:ui_->activate("preview");step("invalid-preview-request");break;
  case 43:if(view_.error_key=="CREATION_DIMENSIONS")step("real-error-ready");break;
  case 44:client_.request_screenshot(evidence_/"creation-error");step("real-error-gpu");break;
  case 45:ui_->activate("error-dismiss");step("dismiss-error");break;
  case 46:ui_->activate("cancel-create");step("cancel-invalid-draft");break;
  case 47:if(view_.screen==ui::Screen::World){act({ui::ActionKind::OpenLoad});act({ui::ActionKind::OpenLoad});if(load_origin_!=ui::Screen::World)throw sonn::Error("SMOKE_LOAD_ORIGIN","Duplicate load lost its return destination");step("selected-load-open-twice");}break;
  case 48:if(view_.screen==ui::Screen::Load&&!view_.saves.empty()){ui_->activate("save-entry-0");step("selected-checkpoint");}break;
  case 49:ui_->activate("load-selected");step("selected-load");break;
  case 50:if(view_.screen==ui::Screen::World){client_.request_screenshot(evidence_/"loaded-world");act({ui::ActionKind::SaveWorld});step("loaded-world-gpu");}break;
  case 51:ui_->activate("world-info");step("exit-rename-open");break;
  case 52:{auto* input=dynamic_cast<Rml::ElementFormControl*>(ui_->context().GetDocument(0)->GetElementById("world-name-draft"));if(!input)throw sonn::Error("SMOKE_INPUT","Exit name input missing");input->SetValue("");input->Focus();ui_->context().ProcessTextInput("退出顺序 · Ähren");ui_->activate("rename-preview");step("exit-rename-preview");break;}
  case 53:if(view_.world.rename_preview_ready){ui_->activate("rename-commit");SDL_Event closing{};closing.type=SDL_EVENT_QUIT;if(SDL_PushEvent(&closing)==false)throw sonn::Error("SMOKE_INPUT","Native close event could not be queued");step("rename-then-native-close");}break;
  }
 }

};
static int app_main(int argc,char** argv){try{fs::path resources=utf8_file(SONN_RESOURCE_ROOT);if(const char* base=SDL_GetBasePath()){auto packaged=utf8_file(base)/"runtime";if(fs::is_regular_file(packaged/"definitions.json"))resources=std::move(packaged);}fs::path saves;fs::path evidence=".build/evidence/native";bool smoke=false;
 for(int i=1;i<argc;i++){std::string a=argv[i];if(a=="--smoke-test")smoke=true;else if(a=="--resources"&&i+1<argc)resources=utf8_file(argv[++i]);else if(a=="--saves"&&i+1<argc)saves=utf8_file(argv[++i]);else if(a=="--evidence"&&i+1<argc)evidence=utf8_file(argv[++i]);else throw sonn::Error("ARGUMENT","Unknown argument: "+a);}
 if(saves.empty()){char* p=SDL_GetPrefPath("Sonnreich","Sonnheide");if(!p)throw sonn::Error("SAVE_ROOT",SDL_GetError());saves=utf8_file(p);SDL_free(p);}App app(resources,saves,smoke,evidence);return app.run();
}catch(const std::exception& e){std::cerr<<"Sonnheide: "<<e.what()<<'\n';return 1;}}

#ifdef _WIN32
int wmain(int argc,wchar_t** argv){
 std::vector<std::string> utf8_args;utf8_args.reserve(argc);
 for(int i=0;i<argc;++i){int n=WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,argv[i],-1,nullptr,0,nullptr,nullptr);if(n<=0)return 1;std::string arg(static_cast<std::size_t>(n),'\0');if(!WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,argv[i],-1,arg.data(),n,nullptr,nullptr))return 1;arg.pop_back();utf8_args.push_back(std::move(arg));}
 std::vector<char*> pointers;for(auto& arg:utf8_args)pointers.push_back(arg.data());return app_main(argc,pointers.data());
}
#else
int main(int argc,char** argv){return app_main(argc,argv);}
#endif
