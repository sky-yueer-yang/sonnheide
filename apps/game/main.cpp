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
static fs::path utf8_file(std::string_view s){return fs::path(std::u8string(reinterpret_cast<const char8_t*>(s.data()),s.size()));}
static sonn::Uuid new_id(){std::random_device r;sonn::Uuid id{};for(auto& x:id)x=static_cast<std::uint8_t>(r());return id;}
static sonn::Hash new_seed(){std::random_device r;sonn::Hash s{};for(auto& x:s)x=static_cast<std::uint8_t>(r());return s;}
static std::string utf8_path(const fs::path& p){auto u=p.u8string();return {reinterpret_cast<const char*>(u.data()),u.size()};}
static std::string bytes_text(const sonn::Bytes& b){return {reinterpret_cast<const char*>(b.data()),b.size()};}
struct CandidateJob {sonn::CreationDraft draft; std::shared_ptr<std::atomic_bool> canceled; std::future<sonn::World> result;};
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
 std::vector<CandidateJob> jobs_;
 std::optional<sonn::World> preview_;
 std::optional<sonn::CreationDraft> preview_draft_;
 std::optional<sonn::PreparedCommand> rename_;
 std::uint64_t preview_token_=0,command_sequence_=0;
 bool quit_=false,paused_=false,inspector_was_paused_=false,inspector_open_=false;
 std::vector<ui::Action> platform_actions_;
 ui::Screen load_origin_=ui::Screen::MainMenu;
 int speed_=1;double tick_accumulator_=0,presentation_seconds_=0;
 ui::View view_;
 sonn::Bytes arcade_font_;
 client::Client client_;
 std::unique_ptr<ui::Ui> ui_;
 bool smoke_=false,smoke_creation_captured_=false,smoke_sea_captured_=false,smoke_land_options_open_=false,smoke_load_captured_=false; int smoke_stage_=0,smoke_camera_stage_=0,smoke_menu_phase_=0,smoke_material_phase_=0;double smoke_menu_live_seconds_=0,smoke_menu_frozen_seconds_=0; Clock::time_point smoke_start_=Clock::now();
 client::Camera smoke_camera_before_{};
 sonn::Hash smoke_camera_world_{},smoke_large_surface_{};
 fs::path evidence_;
 std::vector<std::string> completed_actions_;
 sonn::Uuid counted_world_{};std::uint64_t counted_geometry_revision_=0;
public:
 App(fs::path resources,fs::path saves,bool smoke,fs::path evidence):resources_(std::move(resources)),save_root_(std::move(saves)),definitions_(admit_runtime(resources_)),saves_(save_root_),client_({resources_,1280,800,false}),smoke_(smoke),evidence_(std::move(evidence)) {
  Rml::SetSystemInterface(client_.system_interface());Rml::SetRenderInterface(client_.render_interface());Rml::SetFileInterface(client_.file_interface());
  if(!Rml::Initialise())throw sonn::Error("UI_INIT","RmlUi initialization failed");
  arcade_font_=sonn::read_file(resources_/"fonts/fusion-pixel-12px-proportional-zh_hans.otf",8388608);
  if(!Rml::LoadFontFace(arcade_font_.data(),static_cast<int>(arcade_font_.size()),"Sonn Arcade",Rml::Style::FontStyle::Normal,Rml::Style::FontWeight::Normal,true))throw sonn::Error("FONT_LOAD","Locked arcade pixel font could not be loaded");
  ui_=std::make_unique<ui::Ui>(resources_);auto d=client_.dimensions();std::string err;
  if(!ui_->initialize(d.pixel_w,d.pixel_h,d.density,err))throw sonn::Error("UI_DOCUMENT",err);
  view_.creation.can_select_theme=true;view_.creation.cell_mm=sonn::terrain_cell_size_mm;client_.audio_volume(view_.audio_volume/100.f);read_settings();refresh_saves();
  if(smoke_){fs::create_directories(evidence_);std::error_code ec;fs::remove(evidence_/"native-flow.json",ec);std::ofstream(evidence_/"gpu.json")<<client_.gpu_parameters_json();}
 }
 ~App(){for(auto& j:jobs_)j.canceled->store(true);for(auto& j:jobs_)if(j.result.valid())j.result.wait();ui_.reset();Rml::Shutdown();}
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
   auto v=ui_->viewport();
   client_.set_scene_viewport(static_cast<int>(v.x),static_cast<int>(v.y),static_cast<int>(v.width),static_cast<int>(v.height));
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
  if(smoke_){std::ofstream material_gpu(evidence_/"physical-ui-gpu.json");material_gpu<<client_.gpu_parameters_json();material_gpu.flush();if(!material_gpu)throw sonn::Error("SMOKE_REPORT","Physical UI GPU report could not be written");}
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
 sonn::CreationDraft draft() const{
  sonn::CreationDraft d;
  d.kind=view_.creation.base==ui::BlankBase::Soil?sonn::CreationKind::BlankLand:sonn::CreationKind::BlankSea;
  d.name=view_.creation.name;
  constexpr std::array<std::string_view,8> themes{"snowfield","flower_meadow","maple_field","cherry_field","wetland","savanna","sonnheide_sacred","volcanic"};
  auto ti=std::find(themes.begin(),themes.end(),view_.creation.theme);
  if(ti==themes.end())throw sonn::Error("INVALID_THEME",view_.creation.theme);
  d.soil_theme=static_cast<std::uint8_t>(ti-themes.begin());d.core_width=view_.creation.width;d.core_height=view_.creation.height;
  d.world_id=new_id();d.seed=new_seed();return d;
 }
 void invalidate_preview(){for(auto& j:jobs_)j.canceled->store(true);session_.cancel();preview_.reset();preview_draft_.reset();view_.creation.preview_ready=false;view_.creation.busy=false;view_.creation.generation=++preview_token_;}
 void request_preview(){invalidate_preview();auto d=session_.begin(draft());view_.creation.generation=d.draft_generation;view_.creation.busy=true;
  auto canceled=std::make_shared<std::atomic_bool>(false);auto def=definitions_;
  jobs_.push_back({d,canceled,std::async(std::launch::async,[d,def,canceled]{return sonn::create_candidate(d,def,nullptr,[canceled]{return canceled->load();});})});
 }
 void poll_jobs(){
  for(auto it=jobs_.begin();it!=jobs_.end();){if(it->result.wait_for(std::chrono::milliseconds(0))!=std::future_status::ready){++it;continue;}
   try{auto w=it->result.get();if(!it->canceled->load()&&session_.accepts(it->draft)){
    preview_=std::move(w);preview_draft_=it->draft;view_.creation.preview_ready=true;view_.creation.busy=false;view_.creation.preview_token=++preview_token_;client_.frame_surface(client::surface_view(*preview_));
   }}catch(const sonn::Error& e){if(!it->canceled->load()&&session_.accepts(it->draft)){view_.creation.busy=false;error(e.code,e.what());}}
   it=jobs_.erase(it);
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
  case A::SetBlankBase:invalidate_preview();view_.creation.base=a.text=="ocean"?ui::BlankBase::Ocean:ui::BlankBase::Soil;break;
  case A::SetCreationSize:invalidate_preview();view_.creation.width=static_cast<int>(a.a);view_.creation.height=static_cast<int>(a.b);break;
  case A::SetCreationTheme:invalidate_preview();view_.creation.theme=a.text;break;
  case A::SetCreationName:invalidate_preview();view_.creation.name=a.text;break;
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
 void sync_view(){if(auto* a=session_.active()){const auto& w=a->world();view_.world.session=session_.generation();view_.world.name=w.name;view_.world.revision=w.revision;view_.world.day=w.tick/12000;if(counted_world_!=w.id||counted_geometry_revision_!=w.terrain.surface_revision()){counted_world_=w.id;counted_geometry_revision_=w.terrain.surface_revision();const auto d=static_cast<std::uint64_t>(w.terrain.micro_divisions());view_.world.cells=static_cast<std::uint64_t>(w.terrain.width-2*w.terrain.guard)*(w.terrain.height-2*w.terrain.guard)*d*d;view_.world.dry_cells=w.terrain.dry_micro_count();}view_.world.darkness=w.age.current==sonn::Age::Darkness;view_.world.automatic_age=w.age.automatic;view_.world.age_ticks_remaining=w.age.remaining_tick;view_.world.paused=paused_;view_.world.speed=speed_;}}
 void event(const SDL_Event& e){if(e.type==SDL_EVENT_QUIT){auto prior=ui_->take_actions();platform_actions_.insert(platform_actions_.end(),prior.begin(),prior.end());platform_actions_.push_back({ui::ActionKind::Exit});return;}
  if(e.type==SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED||e.type==SDL_EVENT_WINDOW_DISPLAY_SCALE_CHANGED){auto d=client_.dimensions();ui_->resize(d.pixel_w,d.pixel_h,d.density);}
  bool consumed=client_.process_ui_event(e,ui_->context());
  if(e.type==SDL_EVENT_MOUSE_BUTTON_UP)client_.process_camera_event(e);
  auto dim=client_.dimensions();float mx=0,my=0;
  const bool pointer=e.type==SDL_EVENT_MOUSE_MOTION||e.type==SDL_EVENT_MOUSE_BUTTON_DOWN||e.type==SDL_EVENT_MOUSE_BUTTON_UP||e.type==SDL_EVENT_MOUSE_WHEEL;
  if(e.type==SDL_EVENT_MOUSE_MOTION){mx=e.motion.x;my=e.motion.y;}else if(e.type==SDL_EVENT_MOUSE_BUTTON_DOWN||e.type==SDL_EVENT_MOUSE_BUTTON_UP){mx=e.button.x;my=e.button.y;}else if(e.type==SDL_EVENT_MOUSE_WHEEL){mx=e.wheel.mouse_x;my=e.wheel.mouse_y;}else SDL_GetMouseState(&mx,&my);
  const bool over_ui=pointer&&ui_->captures_pointer(static_cast<int>(mx*dim.density),static_cast<int>(my*dim.density));
  if(ui_->text_input_focused()||ui_->blocks_world_input()||over_ui)return;
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
  case 5:{if(!same(c.yaw_deg,smoke_camera_before_.yaw_deg))throw sonn::Error("SMOKE_KEY","Native E rotation failed");auto surface=client::surface_view(session_.active()->world());auto hit=client_.pick(surface,cx,cy);if(!hit||!same(hit->point.y,session_.active()->world().terrain.height_for(sonn::TerrainKind::Soil)/1000.)||!same(hit->normal.y,1)||client_.pick(surface,v.x-1,v.y-1))throw sonn::Error("SMOKE_PICK","Screen ray did not hit the same soil top or rejected viewport incorrectly");auto support=session_.active()->world().terrain.support(static_cast<std::int64_t>(std::llround(hit->point.x*1000)),static_cast<std::int64_t>(std::llround(hit->point.z*1000)));if(!support||support->height_mm!=session_.active()->world().terrain.height_for(sonn::TerrainKind::Soil)||support->wet)throw sonn::Error("SMOKE_PICK_SUPPORT","Pick and core support differ");auto* tab=ui_->context().GetDocument(0)->GetElementById("tab-observe");auto p=tab->GetAbsoluteOffset(Rml::Box::BORDER),s=tab->GetBox().GetSize(Rml::Box::BORDER);auto density=client_.dimensions().density;smoke_camera_before_=c;wheel((p.x+s.x/2)/density,(p.y+s.y/2)/density);return false;}
  case 6:if(!unchanged())throw sonn::Error("SMOKE_UI_CAPTURE","Wheel on toolbar changed the camera");button(SDL_EVENT_MOUSE_BUTTON_DOWN,SDL_BUTTON_MIDDLE);{SDL_Event e{};e.type=SDL_EVENT_WINDOW_FOCUS_LOST;push(e);}motion(100,0,SDL_BUTTON_MMASK);return false;
  case 7:if(!unchanged())throw sonn::Error("SMOKE_FOCUS","Lost focus retained camera drag");ui_->activate("world-info");return false;
  case 8:{auto* input=ui_->context().GetDocument(0)->GetElementById("world-name-draft");input->Focus();key(SDLK_W);return false;}
  case 9:if(!unchanged())throw sonn::Error("SMOKE_TEXT_CAPTURE","Text/modal input changed the camera");ui_->activate("world-close");return false;
  case 10:key(SDLK_W);return false;
  case 11:{if(unchanged())throw sonn::Error("SMOKE_KEY_RELEASE","Closing the modal did not release world keyboard input");if(session_.active()->world().deterministic_hash()!=smoke_camera_world_)throw sonn::Error("SMOKE_CAMERA_WORLD","Camera/UI input changed authoritative World");std::ofstream report(evidence_/"camera-input.json");report<<sonn::canonical_json(sonn::Json(sonn::Json::Object{{"status","pass"},{"path","SDL_PushEvent -> App input routing -> native camera -> screen ray -> core terrain support"},{"orbit",true},{"wheel",true},{"pan",true},{"qe",true},{"toolbar_wheel_isolated",true},{"focus_loss_releases_drag",true},{"text_modal_isolated",true},{"modal_close_releases_keyboard",true},{"pick_top_mm",session_.active()->world().terrain.height_for(sonn::TerrainKind::Soil)},{"outside_viewport_rejected",true},{"world_hash_unchanged",sonn::hex(smoke_camera_world_)}}));report.flush();if(!report)throw sonn::Error("SMOKE_REPORT","Camera input report could not be written");return true;}
  default:return true;
  }
 }
 void record_creation_dialog(const std::string& screenshot){
  auto* doc=ui_->context().GetDocument(0);auto* dialog=doc->GetElementById("creation-controls");
  if(!dialog||doc->GetElementById("earth-mode")||doc->GetElementById("earth-controls")||doc->GetElementById("map-controls"))throw sonn::Error("SMOKE_CREATION_UI","Blank-only centered creation UI required");
  const auto p=dialog->GetAbsoluteOffset(Rml::Box::BORDER),size=dialog->GetBox().GetSize(Rml::Box::BORDER);const auto dim=client_.dimensions();
  if(size.x<=0||size.y<=0||std::abs(p.x+size.x/2-dim.pixel_w/2.f)>3||std::abs(p.y+size.y/2-dim.pixel_h/2.f)>3||p.x<0||p.y<0||p.x+size.x>dim.pixel_w+1||p.y+size.y>dim.pixel_h+1)
   throw sonn::Error("SMOKE_CREATION_CENTER","Actual creation dialog is not centered and bounded");
  client_.request_screenshot(evidence_/screenshot);
  std::ofstream report(evidence_/"creation-ui.json");
  report<<sonn::canonical_json(sonn::Json(sonn::Json::Object{{"status","pass"},{"creation_entry","blank_only"},{"world_map_controls_present",false},{"geography_loaded_on_startup",false},{"centered",true},{"viewport_width_px",dim.pixel_w},{"viewport_height_px",dim.pixel_h},{"dialog_left_px",static_cast<std::int64_t>(std::llround(p.x))},{"dialog_top_px",static_cast<std::int64_t>(std::llround(p.y))},{"dialog_width_px",static_cast<std::int64_t>(std::llround(size.x))},{"dialog_height_px",static_cast<std::int64_t>(std::llround(size.y))}}));
  report.flush();if(!report)throw sonn::Error("SMOKE_REPORT","Creation UI report write failed");
 }
 void smoke_native_click(const char* id){
  auto* target=ui_->context().GetDocument(0)->GetElementById(id);
  if(!target)throw sonn::Error("SMOKE_INPUT","Native pointer target missing");
  target->ScrollIntoView(false);ui_->update();
  const auto p=target->GetAbsoluteOffset(Rml::Box::BORDER),size=target->GetBox().GetSize(Rml::Box::BORDER);
  const Rml::Vector2f centre{p.x+size.x/2,p.y+size.y/2};
  auto* hit=ui_->context().GetElementAtPoint(centre);
  while(hit && hit!=target)hit=hit->GetParentNode();
  if(hit!=target)throw sonn::Error("SMOKE_MODAL_LAYER","Error action is obscured by another native control");
  const auto dim=client_.dimensions();const float x=centre.x/dim.density,y=centre.y/dim.density;
  auto push=[](SDL_Event e){if(!SDL_PushEvent(&e))throw sonn::Error("SMOKE_INPUT","Native pointer event could not be queued");};
  SDL_Event motion{};motion.type=SDL_EVENT_MOUSE_MOTION;motion.motion.x=x;motion.motion.y=y;push(motion);
  for(const auto type:{SDL_EVENT_MOUSE_BUTTON_DOWN,SDL_EVENT_MOUSE_BUTTON_UP}){SDL_Event button{};button.type=type;button.button.button=SDL_BUTTON_LEFT;button.button.x=x;button.button.y=y;push(button);}
 }
 void smoke_step(){
  static int frame_count=0;static auto stage_time=Clock::now();
  if(std::chrono::duration<double>(Clock::now()-smoke_start_).count()>180)throw sonn::Error("SMOKE_TIMEOUT","Native flow did not complete");
  if(!view_.error_key.empty() && !(smoke_stage_>=43 && smoke_stage_<=46 && view_.error_key=="CREATION_DIMENSIONS"))throw sonn::Error("SMOKE_ERROR",view_.error_key+": "+view_.error_detail);
  if(++frame_count<8||std::chrono::duration<double>(Clock::now()-stage_time).count()<.3)return;
  auto step=[&](std::string name){completed_actions_.push_back(std::move(name));++smoke_stage_;frame_count=0;stage_time=Clock::now();};
  switch(smoke_stage_){
  case 0:
   if(smoke_menu_phase_==0){client_.request_screenshot(evidence_/"main-menu");smoke_menu_live_seconds_=presentation_seconds_;smoke_menu_phase_=1;break;}
   if(smoke_menu_phase_==1){if(presentation_seconds_-smoke_menu_live_seconds_<1.5)break;client_.request_screenshot(evidence_/"main-menu-motion");act({ui::ActionKind::SetReducedMotion,0,0,"",1});smoke_menu_frozen_seconds_=presentation_seconds_;smoke_menu_phase_=2;break;}
   if(smoke_menu_phase_==2){client_.request_screenshot(evidence_/"main-menu-frozen-a");smoke_menu_phase_=3;frame_count=0;break;}
   if(smoke_menu_phase_==3){if(presentation_seconds_!=smoke_menu_frozen_seconds_||session_.active()||preview_)throw sonn::Error("SMOKE_PRESENTATION","Reduced motion changed clock or menu created authority");client_.request_screenshot(evidence_/"main-menu-frozen-b");std::ofstream report(evidence_/"presentation-motion.json");report<<sonn::canonical_json(sonn::Json(sonn::Json::Object{{"status","pass"},{"local_clock_frozen",true},{"menu_creates_world",false},{"live_advance_ms",static_cast<std::int64_t>((smoke_menu_frozen_seconds_-smoke_menu_live_seconds_)*1000)},{"image_comparison_required",true}}));report.flush();if(!report)throw sonn::Error("SMOKE_REPORT","Presentation report write failed");completed_actions_.push_back("menu-live-motion-and-reduced-clock");smoke_menu_phase_=4;break;}
   // The GPU screenshot request is serviced on a later frame. Keep the
   // presentation clock frozen until its actual readback has reached disk.
   if(smoke_menu_phase_==4){if(presentation_seconds_!=smoke_menu_frozen_seconds_)throw sonn::Error("SMOKE_PRESENTATION","Clock resumed before frozen GPU readback");if(!fs::is_regular_file(evidence_/"main-menu-frozen-b.tga"))break;act({ui::ActionKind::SetReducedMotion,0,0,"",0});smoke_menu_phase_=5;}
   ui_->activate("new-world");step("menu-new");break;
  case 1:if(view_.screen==ui::Screen::Creation){
   if(!smoke_creation_captured_){record_creation_dialog("creation-centered");smoke_creation_captured_=true;frame_count=0;stage_time=Clock::now();break;}
   act({ui::ActionKind::SetCreationSize,0,0,"",sonn::maximum_core_cells,sonn::maximum_core_cells});ui_->activate("preview");step("largest-blank-preview-request");}break;
  case 2:if(preview_){client_.request_screenshot(evidence_/"blank-preview");step("blank-preview-gpu");}break;
  case 3:ui_->activate("create");step("blank-create");break;
  case 4:if(session_.active()&&exercise_native_camera()){
   const auto& t=session_.active()->world().terrain;
   if(t.profile!=2||t.width-2*t.guard!=sonn::maximum_core_cells||t.height-2*t.guard!=sonn::maximum_core_cells||t.cell_mm!=2000||t.micro_divisions()!=64||t.dry_micro_count()!=std::uint64_t(4096)*4096*64*64)throw sonn::Error("SMOKE_LARGE_WORLD","Largest Blank not admitted at the actual requested scale");
   smoke_large_surface_=t.surface_hash();
   std::ofstream report(evidence_/"large-world.json");report<<sonn::canonical_json(sonn::Json(sonn::Json::Object{{"status","created"},{"core_width_cells",4096},{"core_height_cells",4096},{"physical_width_mm",8192000},{"physical_height_mm",8192000},{"management_cell_mm",t.cell_mm},{"micro_side",t.micro_divisions()},{"micro_size_um",31250},{"actual_geometry_bytes",static_cast<std::int64_t>(t.storage_bytes())},{"geometry_budget_bytes",static_cast<std::int64_t>(t.storage_budget_bytes)},{"surface_hash",sonn::hex(smoke_large_surface_)},{"zero_plane_land_pick_mm",t.height_for(sonn::TerrainKind::Soil)}}));report.flush();if(!report)throw sonn::Error("SMOKE_REPORT","Largest world report write failed");
   client_.camera().yaw_deg=155;client_.camera().pitch_deg=18;client_.camera().distance_m=80;ui_->activate("tab-world");step("native-camera-pick-and-near-light");}break;
  case 5:{
   static constexpr const char* sections[]={"observe","terrain","life","civilization","construction","economy","world","settings"};
   if(smoke_material_phase_<16){const auto section=sections[smoke_material_phase_/2];
    if(smoke_material_phase_%2==0)ui_->activate(std::string("tab-")+section);
    else {client_.request_screenshot(evidence_/(std::string("toolbar-")+section));completed_actions_.push_back(std::string("physical-toolbar-")+section);}
    ++smoke_material_phase_;frame_count=0;stage_time=Clock::now();break;}
   if(smoke_material_phase_==16){ui_->activate("tab-observe");ui_->activate("camera-help");++smoke_material_phase_;frame_count=0;break;}
   if(smoke_material_phase_==17){client_.request_screenshot(evidence_/"physical-help");completed_actions_.push_back("physical-help-gpu");++smoke_material_phase_;frame_count=0;break;}
   if(smoke_material_phase_==18){ui_->activate("help-close");ui_->activate("tab-world");++smoke_material_phase_;frame_count=0;break;}
   client_.request_screenshot(evidence_/"blank-oblique");step("oblique-gpu");break;
  }
  case 6:ui_->activate("age-darkness");step("set-darkness");break;
  case 7:client_.request_screenshot(evidence_/"darkness");step("dark-gpu");break;
  case 8:ui_->activate("save");ui_->activate("return-menu");step("save-return");break;
  case 9:if(view_.screen==ui::Screen::MainMenu){ui_->activate("continue-world");step("continue");}break;
  case 10:if(session_.active()){
   const auto& t=session_.active()->world().terrain;
   if(t.width-2*t.guard!=4096||t.height-2*t.guard!=4096||t.surface_hash()!=smoke_large_surface_)throw sonn::Error("SMOKE_LARGE_READBACK","Largest world Continue changed fine geometry");
   // GPU diagnostics contain presentation floats. Keep their exact JSON in
   // a separate evidence file instead of passing it through authority JSON.
   std::ofstream gpu(evidence_/"large-world-gpu.json");gpu<<client_.gpu_parameters_json();gpu.flush();if(!gpu)throw sonn::Error("SMOKE_REPORT","Largest world GPU report write failed");
   auto report=sonn::parse_json(bytes_text(sonn::read_file(evidence_/"large-world.json",65536))).object();report["status"]="pass";report["continue_roundtrip_surface_identical"]=true;report["native_gpu_parameters_file"]="large-world-gpu.json";std::ofstream out(evidence_/"large-world.json");out<<sonn::canonical_json(report);out.flush();if(!out)throw sonn::Error("SMOKE_REPORT","Largest world readback report write failed");
   ui_->activate("tab-settings");ui_->activate("settings-open");step("settings-open");}break;
  case 11:client_.request_screenshot(evidence_/"settings-zh");step("settings-zh-gpu");break;
  case 12:ui_->activate("locale-en");step("english");break;
  case 13:client_.request_screenshot(evidence_/"settings-en");step("settings-en-gpu");break;
  case 14:ui_->activate("locale-de");step("german");break;
  case 15:client_.request_screenshot(evidence_/"settings-de");step("settings-de-gpu");break;
  case 16:ui_->activate("settings-close");ui_->activate("return-menu");step("settings-return");break;
  case 17:if(view_.screen==ui::Screen::MainMenu){ui_->activate("new-world");step("blank-sea-new");}break;
  case 18:if(view_.screen==ui::Screen::Creation){
   if(!smoke_sea_captured_){record_creation_dialog("creation-options-centered");smoke_sea_captured_=true;frame_count=0;stage_time=Clock::now();break;}
   act({ui::ActionKind::SetCreationSize,0,0,"",512,256});ui_->activate("blank-ocean");ui_->activate("preview");step("blank-sea-preview-request");}break;
  case 19:if(preview_){if(preview_->terrain.dry_micro_count()!=0||preview_->terrain.width-2*preview_->terrain.guard!=512||preview_->terrain.height-2*preview_->terrain.guard!=256)throw sonn::Error("SMOKE_SEA","Blank sea preview did not use exact selected dimensions and wet ground");client_.request_screenshot(evidence_/"sea-preview");step("blank-sea-preview-gpu");}break;
  case 20:if(!smoke_land_options_open_){ui_->activate("creation-options");smoke_land_options_open_=true;frame_count=0;stage_time=Clock::now();break;}ui_->activate("blank-soil");act({ui::ActionKind::SetCreationTheme,0,0,"cherry_field"});ui_->activate("preview");step("blank-land-preview-request");break;
  case 21:if(preview_){client_.request_screenshot(evidence_/"land-preview");step("blank-land-preview-gpu");}break;
  case 22:ui_->activate("create");step("blank-land-create");break;
  case 23:if(session_.active()&&view_.screen==ui::Screen::World){if(session_.active()->world().terrain.dry_micro_count()==0)throw sonn::Error("SMOKE_LAND","Created Blank soil has no dry surface");ui_->activate("tab-world");step("blank-land-world");}break;
  case 24:client_.request_screenshot(evidence_/"blank-world");step("blank-world-gpu");break;
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
  case 39:act({ui::ActionKind::NewWorld});ui_->activate("preview");step("new-candidate-while-world-retained");break;
  case 40:if(preview_){ui_->activate("cancel-create");step("cancel-candidate");}break;
  case 41:if(view_.screen==ui::Screen::World){if(session_.active()->world().name!="新世界 · Ähren"||saves_.continue_world(definitions_)->name!="新世界 · Ähren")throw sonn::Error("SMOKE_CANCEL","Cancel replaced the retained World or Continue");act({ui::ActionKind::NewWorld});act({ui::ActionKind::SetCreationSize,0,0,"",1,1});step("invalid-dimensions");}break;
  case 42:ui_->activate("preview");step("invalid-preview-request");break;
  case 43:if(view_.error_key=="CREATION_DIMENSIONS")step("real-error-ready");break;
  case 44:client_.request_screenshot(evidence_/"creation-error");step("real-error-gpu");break;
  case 45:smoke_native_click("error-dismiss");step("dismiss-error-native-pointer");break;
  case 46:{if(!view_.error_key.empty())throw sonn::Error("SMOKE_MODAL_LAYER","Native pointer did not dismiss the visible error");
   std::ofstream report(evidence_/"error-layer.json");report<<sonn::canonical_json(sonn::Json(sonn::Json::Object{{"status","pass"},{"screen","creation"},{"topmost_action_hit",true},{"native_pointer_acknowledged",true},{"path","SDL_PushEvent -> App routing -> RmlUi pointer hit -> error dismiss"}}));report.flush();if(!report)throw sonn::Error("SMOKE_REPORT","Modal layer report write failed");
   ui_->activate("cancel-create");step("cancel-invalid-draft");break;}
  case 47:if(view_.screen==ui::Screen::World){act({ui::ActionKind::OpenLoad});act({ui::ActionKind::OpenLoad});if(load_origin_!=ui::Screen::World)throw sonn::Error("SMOKE_LOAD_ORIGIN","Duplicate load lost its return destination");step("selected-load-open-twice");}break;
  case 48:if(view_.screen==ui::Screen::Load&&!view_.saves.empty()){
   if(!smoke_load_captured_){client_.request_screenshot(evidence_/"physical-save-ledger");smoke_load_captured_=true;completed_actions_.push_back("physical-save-ledger-gpu");frame_count=0;break;}
   ui_->activate("save-entry-0");step("selected-checkpoint");}break;
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
 resources=fs::absolute(resources).lexically_normal();
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
