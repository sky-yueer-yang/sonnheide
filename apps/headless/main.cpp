#include "sonnheide/simulation.hpp"
#include <iostream>
#include <sstream>
#include <stdexcept>

int main(){
  using namespace sonnheide;
  try {
    std::vector<Surface> natural(36,Surface::Water);for(int z=0;z<6;++z) natural[static_cast<std::size_t>(z)*6]=Surface::Land;
    Simulation simulation(FrozenWorld(6,6,std::move(natural)),{{7,{100,0}}});
    auto frozen=simulation.world().natural_fingerprint();
    auto issue=[&](std::string id,Payload payload,Principal principal=Principal::Player){
      auto result=simulation.submit({std::move(id),simulation.revision(),std::move(payload)},{principal,7});
      if(result.status!=Status::Accepted) throw std::runtime_error(result.reason);return result.created;
    };
    issue("road",PlaceRoad{{0,2}});
    for(int z=2;z<=3;++z){auto job=issue("plan-"+std::to_string(z),PlanReclamation{{1,z},7});issue("work-"+std::to_string(z),ContributeWork{job,5},Principal::Scheduler);}
    simulation.rebuild_navigation();
    auto port=issue("port",KernelPlacePort{{1,2},1,2,0,true,simulation.world().navigation_epoch()});
    std::stringstream checkpoint;simulation.save(checkpoint);auto restored=Simulation::load(checkpoint);
    if(frozen!=simulation.world().natural_fingerprint() || restored.state_fingerprint()!=simulation.state_fingerprint()) throw std::runtime_error("roundtrip failed");
    std::cout<<"Sonnheide kernel proof slice\nreclaimed_cells=2 port_id="<<port<<" available_material="<<simulation.stock().at(7).available<<" reserved_material="<<simulation.stock().at(7).reserved<<"\nrevision="<<simulation.revision()<<" events="<<simulation.events().size()<<" frozen_natural_mask=unchanged checkpoint_roundtrip=identical\n";
    return 0;
  }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
