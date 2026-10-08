#include "sonnheide/world_creation.hpp"
#include <array>
#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <thread>
#include <sstream>
using namespace sonnheide;
using namespace sonnheide::creation;
namespace {
void check(bool ok, const char* why) { if (!ok) throw std::runtime_error(why); }
std::string bytes(const std::filesystem::path& path) { std::ifstream in(path,std::ios::binary); return {std::istreambuf_iterator<char>(in),{}}; }
void write(const std::filesystem::path& path,const std::string& value) { std::ofstream out(path,std::ios::binary|std::ios::trunc); out<<value; }
std::string edit_body(const std::string& checkpoint, std::size_t index, const std::string& replacement) {
 const auto start=checkpoint.find('\n',checkpoint.find('\n')+1)+1;
 std::istringstream input(checkpoint.substr(start));std::ostringstream output;std::string line;std::size_t n{};
 while(std::getline(input,line)){output<<(n++==index?replacement:line)<<'\n';}
 const auto body=output.str();std::uint64_t digest=14695981039346656037ULL;
 for(unsigned char c:body){digest^=c;digest*=1099511628211ULL;}
 return "SONNHEIDE_EARTH_EMPTY 1\n"+std::to_string(digest)+"\n"+body;
}
CreationResult wait(CreateCoordinator& coordinator) {
 const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(30);
 for (;;) {
  if (auto done=coordinator.poll()) return *done;
  check(std::chrono::steady_clock::now()<deadline,"bounded creation did not finish");
  std::this_thread::sleep_for(std::chrono::milliseconds(1));
 }
}
}
int main(int argc,char** argv) {
 int passed{};
 const auto test=[&](const char* name,const std::function<void()>& operation){operation();++passed;std::cout<<"PASS "<<name<<'\n';};
 const auto directory=std::filesystem::temp_directory_path()/ ("sonnheide-creation-test-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
 try {
  check(argc==2,"provide admitted full geographic source directory");
  auto atlas=earth::GeoAtlas::load(argv[1]);
  earth::WorldConfig config; config.grid_cells_x=16; config.grid_cells_y=16;
  auto definition=earth::EarthWorldDefinition::create(atlas,config);
  const std::string presentation_hash(64,'a');
  ResourceReadiness readiness{definition->recipe_hash(),presentation_hash,true,true,true,true};
  CreateCoordinator coordinator(directory,atlas,presentation_hash);
  test("no fabricated continue",[&]{check(!coordinator.can_continue(),"empty directory has continue");});
  test("durable initial real region publishes only after poll",[&]{
   const auto ticket=coordinator.begin(1,"阿尔卑斯 · Sonnheide");
   check(coordinator.start(ticket,definition,readiness).status==CreateStatus::Preparing,"start did not prepare");
   check(!coordinator.active(),"start prematurely published");
   auto result=wait(coordinator);check(static_cast<bool>(result),result.error.c_str());
   check(result.session->definition->source_hash()==atlas->source_hash(),"source replaced by synthetic mask");
   check(result.session->tick==0&&!result.session->age_automatic&&result.session->age==EnvironmentAge::Light,"initial environment incorrect");
   check(result.session->entities.people==0&&result.session->entities.buildings==0&&result.session->entities.countries==0&&result.session->entities.tasks==0,"entities fabricated");
   check(result.session->display_name=="阿尔卑斯 · Sonnheide","world name did not roundtrip");
   check(coordinator.can_continue(),"durable world has no continue");
  });
  test("continue preserves WorldId but allocates fresh session",[&]{
   auto previous=coordinator.active(); auto prepared=coordinator.prepare_continue();
   check(coordinator.active()==previous,"readonly continue published");
   auto result=coordinator.publish_continue(prepared,readiness); check(static_cast<bool>(result),result.error.c_str());
   check(result.session->world_id==previous->world_id,"continue changed WorldId");
   check(result.session->session_generation!=previous->session_generation,"continue reused session generation");
  });
  test("every durable failure preserves old active and old pointer",[&]{
   const std::array steps{PersistenceStep::CheckpointOpen,PersistenceStep::CheckpointWrite,PersistenceStep::CheckpointFlush,
    PersistenceStep::CheckpointFileSync,PersistenceStep::CheckpointRename,PersistenceStep::CheckpointDirectorySync,
    PersistenceStep::CheckpointReadback,PersistenceStep::PointerOpen,PersistenceStep::PointerWrite,PersistenceStep::PointerFlush,
    PersistenceStep::PointerFileSync,PersistenceStep::PointerRename,PersistenceStep::PointerDirectorySync};
   for(auto step:steps){
    const auto old=coordinator.active();const auto pointer=bytes(directory/"continue.pointer");
    coordinator.set_fault_injector([step](PersistenceStep reached){if(reached==step)throw std::runtime_error("injected durable failure");});
    auto result=coordinator.create(coordinator.begin(2),definition,readiness);
    check(!result,"fault unexpectedly published");check(coordinator.active()==old,"failure destroyed old active");
    check(bytes(directory/"continue.pointer")==pointer,"failure replaced old continue pointer");
    check(coordinator.can_continue(),"failure damaged old checkpoint");
   }
   coordinator.set_fault_injector({});
  });
  test("cancel before create makes no world",[&]{auto old=coordinator.active();auto t=coordinator.begin(3);coordinator.cancel(t);
   check(coordinator.create(t,definition,readiness).status==CreateStatus::Cancelled,"cancel not honored");check(coordinator.active()==old,"cancel replaced active");});
  test("cancel during checkpoint worker never publishes",[&]{
   std::atomic<bool> reached{false},release{false};auto old=coordinator.active();auto pointer=bytes(directory/"continue.pointer");
   coordinator.set_fault_injector([&](PersistenceStep step){if(step==PersistenceStep::CheckpointWrite){reached=true;while(!release.load())std::this_thread::yield();}});
   const auto ticket=coordinator.begin(4);check(coordinator.start(ticket,definition,readiness).status==CreateStatus::Preparing,"worker not started");
   const auto until=std::chrono::steady_clock::now()+std::chrono::seconds(5);
   while(!reached.load()&&std::chrono::steady_clock::now()<until)std::this_thread::yield();
   check(reached.load(),"worker hook not reached");coordinator.cancel(ticket);release=true;
   check(wait(coordinator).status==CreateStatus::Cancelled,"running cancel not honored");
   check(coordinator.active()==old&&bytes(directory/"continue.pointer")==pointer,"running cancel damaged old world");coordinator.set_fault_injector({});
  });
  test("old completion and changed draft reject",[&]{
   auto old=coordinator.begin(5);auto fresh=coordinator.begin(6);check(coordinator.create(old,definition,readiness).status==CreateStatus::Stale,"old ticket admitted");
   coordinator.invalidate_draft(7);check(coordinator.create(fresh,definition,readiness).status==CreateStatus::Stale,"changed draft admitted");
  });
  test("late background completion after new request rejected",[&]{
   const auto previous=coordinator.active();auto t=coordinator.begin(8);
   check(coordinator.start(t,definition,readiness).status==CreateStatus::Preparing,"worker not started");
   coordinator.begin(9);check(wait(coordinator).status==CreateStatus::Stale,"late worker published");check(coordinator.active()==previous,"late completion changed active");
  });
  test("cancelled continue preparation cannot publish",[&]{
   auto old=coordinator.active();auto prepared=coordinator.prepare_continue();auto ticket=coordinator.begin(20);coordinator.cancel(ticket);
   check(coordinator.publish_continue(prepared,readiness,ticket).status==CreateStatus::Cancelled,"cancelled continue admitted");
   coordinator.begin(21);check(coordinator.publish_continue(prepared,readiness,ticket).status==CreateStatus::Stale,"late continue admitted");
   check(coordinator.active()==old,"cancelled continue changed active");
  });
  test("first-create pointer failure leaves no false continue",[&]{
   const auto fresh_directory=directory/"first-failure";CreateCoordinator fresh(fresh_directory,atlas,presentation_hash);
   fresh.set_fault_injector([](PersistenceStep step){if(step==PersistenceStep::PointerDirectorySync)throw std::runtime_error("first pointer sync failure");});
   check(!fresh.create(fresh.begin(22),definition,readiness),"first failure published");
   check(!fresh.active()&&!fresh.can_continue(),"first failure fabricated continue");
   check(!std::filesystem::exists(fresh_directory/"continue.pointer"),"first failure left pointer");
  });
  test("candidate requires exact GPU recipe and all resources",[&]{
   auto broken=readiness;broken.terrain_recipe_hash="stale";check(!coordinator.create(coordinator.begin(10),definition,broken),"stale geometry admitted");
   broken=readiness;broken.presentation_recipe_hash=std::string(64,'b');check(!coordinator.create(coordinator.begin(11),definition,broken),"stale resource recipe admitted");
   for(int i=0;i<4;++i){broken=readiness;if(i==0)broken.terrain_ready=false;if(i==1)broken.materials_ready=false;if(i==2)broken.light_sky_ready=false;if(i==3)broken.dark_sky_ready=false;
    check(!coordinator.create(coordinator.begin(12),definition,broken),"missing resource admitted");}
  });
  test("corrupt checkpoint rejects and preserves old active",[&]{
   auto old=coordinator.active();const auto path=directory/(old->world_id+".world");auto saved=bytes(path);auto damaged=saved;damaged.back()='x';write(path,damaged);
   check(!coordinator.load_continue(readiness),"corrupt checkpoint admitted");check(coordinator.active()==old,"bad load destroyed active");write(path,saved);
  });
  test("readback rejects incompatible source and excess cell budgets",[&]{
   auto old=coordinator.active();const auto path=directory/(old->world_id+".world");const auto original=bytes(path);
   write(path,edit_body(original,2,std::string(64,'f')));check(!coordinator.load_continue(readiness),"wrong geographic source admitted");
   write(path,edit_body(original,4,"1"));check(!coordinator.load_continue(readiness),"wrong authoritative geometry admitted");
   write(path,edit_body(original,3,"bad-recipe"));check(!coordinator.load_continue(readiness),"wrong frozen recipe admitted");
   write(path,edit_body(original,10,"4294967295 4294967295 1"));check(!coordinator.load_continue(readiness),"unbounded geometry admitted");
   check(coordinator.active()==old,"invalid readback damaged active");write(path,original);
  });
  test("cancellation at pointer rename rolls back pointer",[&]{
   auto old=coordinator.active();auto pointer=bytes(directory/"continue.pointer");auto ticket=coordinator.begin(17);
   coordinator.set_fault_injector([&](PersistenceStep step){if(step==PersistenceStep::PointerDirectorySync)coordinator.cancel(ticket);});
   auto result=coordinator.create(ticket,definition,readiness);check(result.status==CreateStatus::Cancelled,"final cancellation ignored");
   check(coordinator.active()==old&&bytes(directory/"continue.pointer")==pointer,"final cancellation failed rollback");coordinator.set_fault_injector({});
  });
  test("continue pointer cannot escape save directory",[&]{auto pointer=bytes(directory/"continue.pointer");write(directory/"continue.pointer","SONNHEIDE_CONTINUE 1\n../../outside\n");
   check(!coordinator.can_continue(),"path traversal pointer admitted");check(!coordinator.load_continue(readiness),"path traversal load admitted");write(directory/"continue.pointer",pointer);});
  test("world names UTF-8 bounded and independent of paths",[&]{
   bool rejected=false;try{coordinator.begin(13,std::string(385,'x'));}catch(const std::exception&){rejected=true;}check(rejected,"oversized name admitted");
   rejected=false;try{coordinator.begin(14,std::string("\xc0\xaf",2));}catch(const std::exception&){rejected=true;}check(rejected,"overlong UTF-8 admitted");
   auto result=coordinator.create(coordinator.begin(15,"../世界/.."),definition,readiness);check(static_cast<bool>(result),result.error.c_str());
   check(std::filesystem::is_regular_file(directory/(result.session->world_id+".world")),"name used as path");
  });
  test("writer thread cannot publish from worker",[&]{
   std::atomic<bool> rejected{false};std::thread worker([&]{try{coordinator.begin(16);}catch(const std::exception&){rejected=true;}});worker.join();check(rejected.load(),"second writer admitted");
  });
  test("successful retries leave no failed worlds or temporary files",[&]{
   std::size_t worlds{};for(const auto& entry:std::filesystem::directory_iterator(directory)){check(entry.path().extension()!=".tmp","temporary leaked");if(entry.path().extension()==".world")++worlds;}
   check(worlds==2,"failed or cancelled checkpoint leaked");
  });
  std::filesystem::remove_all(directory);std::cout<<passed<<" world creation scenarios passed\n";return 0;
 }catch(const std::exception& exception){std::filesystem::remove_all(directory);std::cerr<<"FAIL "<<exception.what()<<'\n';return 1;}
}
