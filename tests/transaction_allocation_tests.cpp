#include "sonnheide/simulation.hpp"
#include <cstdlib>
#include <iostream>
#include <new>

namespace {thread_local long failAfter=-1;thread_local bool injected=false;}
void* operator new(std::size_t size){
  if(failAfter==0){failAfter=-1;injected=true;throw std::bad_alloc();}
  if(failAfter>0) --failAfter;
  if(auto pointer=std::malloc(size==0?1:size)) return pointer;
  throw std::bad_alloc();
}
void* operator new[](std::size_t size){return ::operator new(size);}
void operator delete(void* pointer) noexcept {std::free(pointer);}
void operator delete[](void* pointer) noexcept {std::free(pointer);}
void operator delete(void* pointer,std::size_t) noexcept {std::free(pointer);}
void operator delete[](void* pointer,std::size_t) noexcept {std::free(pointer);}

int main(){
  using namespace sonnheide;
  long failures=0;
  for(long allocation=0;allocation<256;++allocation){
    Simulation simulation(FrozenWorld(2,2,{Surface::Land,Surface::Water,Surface::Land,Surface::Water}),{{7,{100,0}}});
    Command command{"atomic",0,PlanReclamation{{1,0},7}};
    // MSVC locale/iostream initialization can allocate inside noexcept library startup.
    // Warm a separate transaction before testing per-command allocations in an initialized runtime.
    auto warmup=simulation;warmup.submit(command,{Principal::Player,7});
    bool failed=false;injected=false;failAfter=allocation;
    try{simulation.submit(command,{Principal::Player,7});}
    catch(const std::exception&){failAfter=-1;if(!injected) throw;failed=true;}
    failAfter=-1;
    if(failed){
      if(simulation.revision()!=0 || !simulation.jobs().empty() || simulation.stock().at(7)!=Stock{100,0}){std::cerr<<"allocation failure left partial commit\n";return 1;}
      auto retry=simulation.submit(command,{Principal::Player,7});
      if(retry.status!=Status::Accepted || simulation.stock().at(7)!=Stock{90,10}){std::cerr<<"original command retry failed\n";return 1;}
      simulation.validate();
    }else{
      auto retry=simulation.submit(command,{Principal::Player,7});
      if(retry.status!=Status::Accepted || simulation.revision()!=1 || simulation.jobs().size()!=1){std::cerr<<"successful commit missing receipt\n";return 1;}
    }
    if(injected) ++failures;
    else {std::cout<<"PASS atomic commit across "<<failures<<" injected allocation failures\n";return 0;}
  }
  std::cerr<<"allocation injection bound too small\n";return 1;
}
