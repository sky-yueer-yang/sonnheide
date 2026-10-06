#pragma once
#include "sonnheide/types.hpp"
#include <map>
#include <set>
#include <vector>

namespace sonnheide {
class Simulation;
class FrozenWorld {
public:
  FrozenWorld(int width, int depth, std::vector<Surface> natural);
  int width() const { return width_; }
  int depth() const { return depth_; }
  bool contains(Cell cell) const;
  Surface natural(Cell cell) const;
  Surface effective(Cell cell) const;
  bool reclaimed(Cell cell) const;
  bool adjacent_land(Cell cell) const;
  bool construction(Cell cell) const { return construction_.contains(cell); }
  bool berth_reserved(Cell cell) const { return berths_.contains(cell); }
  bool occupied(Cell cell) const { return occupied_.contains(cell); }
  std::uint64_t natural_fingerprint() const;
  std::uint64_t navigation_epoch() const { return navigationEpoch_; }
  bool navigation_ready() const { return navigationBuiltEpoch_ == navigationEpoch_; }
  bool ocean_reachable(Cell cell) const;
  bool water_reachable(Cell from, Cell to) const;
  void rebuild_navigation();
  struct PortGeometry { std::vector<Cell> core; std::vector<Cell> berth; std::vector<Cell> roadSide; };
  PortGeometry port_geometry(const KernelPlacePort& port) const;
  std::string validate_port(const KernelPlacePort& port) const;
private:
  friend class Simulation;
  int width_, depth_;
  std::vector<Surface> natural_;
  std::set<Cell> reclaimed_, construction_, occupied_, roads_, berths_;
  std::uint64_t navigationEpoch_{1}, navigationBuiltEpoch_{0};
  std::map<Cell, int> regions_;
  std::set<int> oceanRegions_;
  void invalidate_navigation();
  std::size_t index(Cell cell) const;
};
} // namespace sonnheide
