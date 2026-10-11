#pragma once
#include "sonnheide.hpp"

namespace sonn {
// Local pacing only: the authoritative world still advances in indivisible
// 20 Hz ticks. Half-step rates and integer nanoseconds avoid rounding 0.5/1.5.
class SimulationClock {
 std::uint8_t half_steps_=2;
 std::uint64_t pending_units_=0;
public:
 static constexpr std::array<std::uint8_t,7> supported_half_steps{1,2,3,4,6,10,20};
 static constexpr std::uint64_t units_per_tick=100000000;
 static constexpr std::int64_t maximum_frame_ns=250000000;
 static constexpr unsigned work_budget=256;
 double speed() const { return half_steps_/2.0; }
 int half_steps() const { return half_steps_; }
 std::uint64_t pending_units() const { return pending_units_; }
 void set_speed(double multiplier); // Validate before changing any state.
 void restore_preference(const Json& preferences);
 void accrue_frame(std::int64_t elapsed_ns,bool running);
 bool tick_due() const { return pending_units_>=units_per_tick; }
 void complete_tick(); // Call only after the authoritative tick succeeded.
 void reset_elapsed() { pending_units_=0; } // Only a different world/session.
};
}
