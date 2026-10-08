#include "sonnheide/world_creation.hpp"
#include <chrono>
#include <cstdlib>
#include <cstdio>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <new>
#include <thread>
namespace {
thread_local long fail_after=-1;
thread_local bool injected=false;
thread_local const char* sweep="initialization";
thread_local long sweep_index=-1;
void arm(const char* phase,long index){
 sweep=phase;sweep_index=index;
#ifdef _WIN32
 std::fprintf(stderr,"ALLOCATION BEGIN phase=%s index=%ld\n",sweep,sweep_index);
#endif
 injected=false;fail_after=index;
}
void terminated() noexcept {
 fail_after=-1;
 std::fprintf(stderr,"ALLOCATION TERMINATE phase=%s index=%ld injected=%d\n",sweep,sweep_index,int(injected));
 if(auto exception=std::current_exception()){
  try{std::rethrow_exception(exception);}
  catch(const std::exception& error){std::fprintf(stderr,"ACTIVE EXCEPTION: %s\n",error.what());}
  catch(...){std::fprintf(stderr,"ACTIVE EXCEPTION: non-standard\n");}
 }else std::fprintf(stderr,"ACTIVE EXCEPTION: none\n");
 std::fflush(stderr);std::abort();
}
#ifdef _WIN32
void invalid_parameter(const wchar_t* expression,const wchar_t* function,const wchar_t* file,unsigned int line,uintptr_t) noexcept {
 fail_after=-1;
 std::fprintf(stderr,"ALLOCATION CRT INVALID PARAMETER phase=%s index=%ld injected=%d line=%u\n",sweep,sweep_index,int(injected),line);
 if(expression)std::fprintf(stderr,"expression=%ls\n",expression);
 if(function)std::fprintf(stderr,"function=%ls\n",function);
 if(file)std::fprintf(stderr,"file=%ls\n",file);
 std::fflush(stderr);std::abort();
}
#endif
void fail_point(){
 if(fail_after==0){
  fail_after=-1;injected=true;
#ifdef _WIN32
  std::fprintf(stderr,"ALLOCATION INJECT phase=%s index=%ld\n",sweep,sweep_index);
#endif
  throw std::bad_alloc();
 }
 if(fail_after>0)--fail_after;
}
}
void* operator new(std::size_t size){fail_point();if(auto* p=std::malloc(size?size:1))return p;throw std::bad_alloc();}
void* operator new[](std::size_t size){return ::operator new(size);}
void operator delete(void* p)noexcept{std::free(p);}
void operator delete[](void* p)noexcept{std::free(p);}
void operator delete(void* p,std::size_t)noexcept{std::free(p);}
void operator delete[](void* p,std::size_t)noexcept{std::free(p);}
namespace {
using namespace sonnheide;
using namespace sonnheide::creation;
void check(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
std::string bytes(const std::filesystem::path& path){std::ifstream in(path,std::ios::binary);return {std::istreambuf_iterator<char>(in),{}};}
std::size_t world_count(const std::filesystem::path& path){std::size_t n{};for(const auto& e:std::filesystem::directory_iterator(path)){check(e.path().extension()!=".tmp","temporary leaked after allocation failure");if(e.path().extension()==".world")++n;}return n;}
CreationResult wait(CreateCoordinator& writer){for(;;){if(auto result=writer.poll())return *result;std::this_thread::sleep_for(std::chrono::milliseconds(1));}}
void clean_previous(const std::filesystem::path& directory,const std::string& old_id,const std::string& new_id){if(old_id!=new_id)std::filesystem::remove(directory/(old_id+".world"));}
}
int main(int argc,char** argv){
 std::setvbuf(stderr,nullptr,_IONBF,0);
 std::cout<<std::unitbuf;
 std::set_terminate(terminated);
#ifdef _WIN32
 ::_set_invalid_parameter_handler(invalid_parameter);
#endif
 const auto directory=std::filesystem::temp_directory_path()/("sonnheide-real-allocation-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
 try{
  check(argc==2,"provide admitted full geographic source directory");
  auto atlas=earth::GeoAtlas::load(argv[1]);earth::WorldConfig config;config.grid_cells_x=8;config.grid_cells_y=8;
  // Warm initialized standard-library locale/thread/future paths before testing
  // allocations attributable to the actual operations, as with kernel tests.
  auto definition=earth::EarthWorldDefinition::create(atlas,config);
  const std::string presentation_hash(64,'a');ResourceReadiness ready{definition->recipe_hash(),presentation_hash,true,true,true,true};
  CreateCoordinator writer(directory,atlas,presentation_hash);
  check(static_cast<bool>(writer.create(writer.begin(1),definition,ready)),"allocation warmup create failed");
  std::size_t geometry_failures{};
  for(long allocation=0;allocation<20000;++allocation){
   const auto active=writer.active();const auto pointer=bytes(directory/"continue.pointer");const auto count=world_count(directory);
   arm("geometry",allocation);bool failed=false;
   std::shared_ptr<const earth::EarthWorldDefinition> candidate;
   try{candidate=earth::EarthWorldDefinition::create(atlas,config);}
   catch(const std::exception&){fail_after=-1;if(!injected)throw;failed=true;}
   fail_after=-1;
   if(candidate)check(candidate->recipe_hash()==definition->recipe_hash(),"allocation failure changed admitted geometry recipe");
   check(writer.active()==active&&bytes(directory/"continue.pointer")==pointer&&world_count(directory)==count,"geometry allocation touched published world");
   if(injected){++geometry_failures;check(failed,"failed geometry was silently published");}
   else {check(!failed,"unexplained geometry failure");break;}
   check(allocation<19999,"geometry allocation coverage bound exhausted");
  }
  std::cout<<"PASS frozen full geometry across "<<geometry_failures<<" real allocation failures\n";
  std::size_t start_failures{};
  for(long allocation=0;allocation<4096;++allocation){
   const auto active=writer.active();const auto pointer=bytes(directory/"continue.pointer");const auto count=world_count(directory);const auto ticket=writer.begin(2);
   arm("owner-start",allocation);CreationResult started;bool escaped=false;
   try{started=writer.start(ticket,definition,ready);}catch(const std::exception&){fail_after=-1;if(!injected)throw;escaped=true;}
   fail_after=-1;
   if(injected){
    ++start_failures;check(escaped||started.status!=CreateStatus::Preparing,"start published after failed allocation");
    check(!writer.busy(),"failed owner start left an inaccessible worker");
    check(writer.active()==active&&bytes(directory/"continue.pointer")==pointer&&world_count(directory)==count,"start allocation left partial commit");
   }else{
    check(!escaped&&started.status==CreateStatus::Preparing,"start failed without injection");auto result=wait(writer);check(static_cast<bool>(result),result.error.c_str());
    clean_previous(directory,active->world_id,result.session->world_id);break;
   }
   check(allocation<4095,"owner start allocation bound exhausted");
  }
  std::cout<<"PASS creation owner start across "<<start_failures<<" real allocation failures\n";
  // Not-ready polls allocate nothing. Sweep allocations on the owner only;
  // worker allocations remain enabled, avoiding future/thread runtime termination.
  std::size_t poll_failures{};
  for(long allocation=0;allocation<4096;++allocation){
   const auto active=writer.active();const auto pointer=bytes(directory/"continue.pointer");const auto count=world_count(directory);
   const auto started=writer.start(writer.begin(3),definition,ready);check(started.status==CreateStatus::Preparing,"poll setup failed");
   arm("owner-poll",allocation);std::optional<CreationResult> result;bool escaped=false;
   try{
    while(!(result=writer.poll()))std::this_thread::sleep_for(std::chrono::milliseconds(1));
   }catch(const std::exception&){fail_after=-1;if(!injected)throw;escaped=true;}
   fail_after=-1;
   if(injected){
    ++poll_failures;check(escaped||!static_cast<bool>(*result),"poll succeeded after failed owner allocation");
    check(!writer.busy(),"failed poll left inaccessible pending work");
    if(writer.active()!=active||bytes(directory/"continue.pointer")!=pointer||world_count(directory)!=count){
     std::cerr<<"poll allocation index="<<allocation<<" active_changed="<<(writer.active()!=active)<<" pointer_changed="<<(bytes(directory/"continue.pointer")!=pointer)<<" worlds_before="<<count<<" worlds_after="<<world_count(directory)<<'\n';
     throw std::runtime_error("owner poll allocation left partial publication");
    }
   }else{
    check(!escaped&&result&&static_cast<bool>(*result),"poll failed without injected allocation");
    clean_previous(directory,active->world_id,result->session->world_id);break;
   }
   check(allocation<4095,"owner poll allocation bound exhausted");
  }
  std::cout<<"PASS durable owner poll across "<<poll_failures<<" real allocation failures\n";
  auto retry=writer.create(writer.begin(4),definition,ready);check(static_cast<bool>(retry),retry.error.c_str());
  check(writer.can_continue()&&retry.session->entities.people==0&&retry.session->entities.buildings==0,"final retry failed production invariants");
  std::cout<<"PASS successful retry after all real allocation failures\n";
  std::filesystem::remove_all(directory);return 0;
 }catch(const std::exception& exception){fail_after=-1;std::filesystem::remove_all(directory);std::cerr<<"FAIL "<<exception.what()<<'\n';return 1;}
}
