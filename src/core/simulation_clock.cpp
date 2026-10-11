#include "simulation_clock.hpp"
#include <algorithm>
#include <limits>

namespace sonn {
void SimulationClock::set_speed(double multiplier) {
 const auto found=std::find_if(supported_half_steps.begin(),supported_half_steps.end(),[multiplier](auto half){return multiplier==half/2.0;});
 if(found==supported_half_steps.end())throw Error("BAD_SPEED","Supported simulation rates are 0.5, 1, 1.5, 2, 3, 5 and 10");
 half_steps_=*found;
}
void SimulationClock::restore_preference(const Json& preferences) {
 const auto& fields=preferences.object();
 if(auto entry=fields.find("speed_half_steps");entry!=fields.end()) {
  const auto half=entry->second.integer();
  const auto found=std::find(supported_half_steps.begin(),supported_half_steps.end(),half);
  if(found==supported_half_steps.end())throw Error("BAD_SPEED","Invalid persisted half-step simulation rate");
  half_steps_=*found;
 } else if(auto entry=fields.find("speed");entry!=fields.end()) {
  // Legacy application preferences used integer 1/4/16/64. Preserve 1,
  // choose the nearest lower rate for the 3/5 tie, and cap retired fast rates.
  const auto legacy=entry->second.integer();
  if(legacy==4)set_speed(3);
  else if(legacy==16||legacy==64)set_speed(10);
  else if(legacy==1||legacy==2||legacy==3||legacy==5||legacy==10)set_speed(static_cast<double>(legacy));
  else throw Error("BAD_SPEED","Invalid legacy simulation rate");
 }
}
void SimulationClock::accrue_frame(std::int64_t elapsed_ns,bool running) {
 if(!running)return; // Pause/hidden UI accrue neither elapsed time nor debt.
 const auto elapsed=static_cast<std::uint64_t>(std::clamp<std::int64_t>(elapsed_ns,0,maximum_frame_ns));
 const auto added=elapsed*half_steps_;
 if(added>std::numeric_limits<std::uint64_t>::max()-pending_units_)throw Error("CLOCK_OVERFLOW","Simulation pacing debt exceeds uint64");
 pending_units_+=added;
}
void SimulationClock::complete_tick() {
 if(!tick_due())throw Error("CLOCK_TICK_NOT_DUE","Cannot consume an unearned simulation tick");
 pending_units_-=units_per_tick;
}
}
