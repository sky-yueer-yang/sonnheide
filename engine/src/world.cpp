#include "sonnheide/world.hpp"
#include <algorithm>
#include <array>
#include <queue>
#include <stdexcept>

namespace sonnheide {
namespace {
constexpr std::array<Cell, 4> offsets{{{1,0},{-1,0},{0,1},{0,-1}}};
}
FrozenWorld::FrozenWorld(int width, int depth, std::vector<Surface> natural)
    : width_(width), depth_(depth), natural_(std::move(natural)) {
  if(width <= 0 || depth <= 0 || width > 10000 || depth > 10000 ||
     static_cast<std::uint64_t>(width)*static_cast<std::uint64_t>(depth) > 1000000 ||
     natural_.size() != static_cast<std::size_t>(width)*static_cast<std::size_t>(depth))
    throw std::invalid_argument("invalid bounded kernel map");
  for(auto s : natural_) if(s != Surface::Land && s != Surface::Water) throw std::invalid_argument("invalid surface");
}
bool FrozenWorld::contains(Cell c) const { return c.x >= 0 && c.z >= 0 && c.x < width_ && c.z < depth_; }
std::size_t FrozenWorld::index(Cell c) const {
  if(!contains(c)) throw std::out_of_range("cell outside world");
  return static_cast<std::size_t>(c.z)*static_cast<std::size_t>(width_)+static_cast<std::size_t>(c.x);
}
Surface FrozenWorld::natural(Cell c) const { return natural_.at(index(c)); }
Surface FrozenWorld::effective(Cell c) const { return natural(c) == Surface::Land || reclaimed(c) ? Surface::Land : Surface::Water; }
bool FrozenWorld::reclaimed(Cell c) const { return reclaimed_.contains(c); }
bool FrozenWorld::adjacent_land(Cell c) const {
  if(!contains(c)) return false;
  for(auto d : offsets) { Cell n{c.x+d.x,c.z+d.z}; if(contains(n) && effective(n)==Surface::Land) return true; }
  return false;
}
std::uint64_t FrozenWorld::natural_fingerprint() const {
  // Portable FNV regression fingerprint. Production data integrity uses SHA-256 manifests.
  std::uint64_t h=14695981039346656037ULL;
  auto word=[&h](std::uint64_t v){ for(int i=0;i<8;++i){ h ^= v&255U; h*=1099511628211ULL; v>>=8; } };
  word(static_cast<std::uint64_t>(width_)); word(static_cast<std::uint64_t>(depth_));
  for(auto s:natural_) word(static_cast<std::uint64_t>(s));
  return h;
}
void FrozenWorld::invalidate_navigation(){ ++navigationEpoch_; }
void FrozenWorld::rebuild_navigation() {
  // Complete flood fill is deliberately a bounded correctness oracle, not the shipping chunk algorithm.
  regions_.clear(); oceanRegions_.clear(); int region=0;
  for(int z=0;z<depth_;++z) for(int x=0;x<width_;++x){
    Cell seed{x,z};
    if(effective(seed)!=Surface::Water || construction_.contains(seed) || regions_.contains(seed)) continue;
    ++region; std::queue<Cell> q; q.push(seed); regions_[seed]=region;
    while(!q.empty()){
      auto c=q.front(); q.pop();
      if(c.x==0 || c.z==0 || c.x==width_-1 || c.z==depth_-1) oceanRegions_.insert(region);
      for(auto d:offsets){Cell n{c.x+d.x,c.z+d.z};
        if(contains(n) && effective(n)==Surface::Water && !construction_.contains(n) && !regions_.contains(n)){
          regions_[n]=region; q.push(n);
        }
      }
    }
  }
  navigationBuiltEpoch_=navigationEpoch_;
}
bool FrozenWorld::ocean_reachable(Cell c) const {
  if(!navigation_ready()) return false;
  auto it=regions_.find(c); return it!=regions_.end() && oceanRegions_.contains(it->second);
}
bool FrozenWorld::water_reachable(Cell a,Cell b) const {
  if(!navigation_ready()) return false;
  auto ia=regions_.find(a),ib=regions_.find(b); return ia!=regions_.end() && ib!=regions_.end() && ia->second==ib->second;
}
FrozenWorld::PortGeometry FrozenWorld::port_geometry(const KernelPlacePort& p) const {
  if(!contains(p.anchor) || p.width<=0 || p.depth<=0 || p.width>64 || p.depth>64 ||
      (p.rotation!=0 && p.rotation!=90 && p.rotation!=180 && p.rotation!=270)) return {};
  PortGeometry g;
  auto transform=[&p](int x,int z){
    switch(p.rotation){
      case 90: return Cell{p.anchor.x+p.depth-1-z,p.anchor.z+x};
      case 180: return Cell{p.anchor.x+p.width-1-x,p.anchor.z+p.depth-1-z};
      case 270: return Cell{p.anchor.x+z,p.anchor.z+p.width-1-x};
      default: return Cell{p.anchor.x+x,p.anchor.z+z};
    }
  };
  for(int z=0;z<p.depth;++z){
    for(int x=0;x<p.width;++x) g.core.push_back(transform(x,z));
    // Canonical east sea edge, with a two-cell-deep berth. Rotation transforms all zones together.
    for(int x=p.width;x<p.width+2;++x) g.berth.push_back(transform(x,z));
    g.roadSide.push_back(transform(-1,z));
  }
  return g;
}
std::string FrozenWorld::validate_port(const KernelPlacePort& p) const {
  if(!contains(p.anchor)) return "OUT_OF_BOUNDS";
  if(p.rotation!=0 && p.rotation!=90 && p.rotation!=180 && p.rotation!=270) return "ORTHOGONAL_ROTATION_REQUIRED";
  if(p.width<=0 || p.depth<=0 || p.width>64 || p.depth>64) return "INVALID_FOOTPRINT";
  auto g=port_geometry(p);
  for(auto c:g.core){
    if(!contains(c)) return "OUT_OF_BOUNDS";
    if(!reclaimed(c) || effective(c)!=Surface::Land) return "CORE_NOT_RECLAIMED";
    if(occupied(c) || construction(c) || roads_.contains(c)) return "SITE_OCCUPIED";
  }
  for(auto c:g.berth){
    if(!contains(c)) return "OUT_OF_BOUNDS";
    if(effective(c)!=Surface::Water) return "SEA_EDGE_NOT_WATER";
    if(construction(c) || occupied(c) || berth_reserved(c)) return "BERTH_BLOCKED";
  }
  if(!navigation_ready()) return "NAVIGATION_PENDING";
  if(p.navigationEpoch!=navigationEpoch_) return "NAVIGATION_STALE";
  for(auto c:g.berth){
    if(!water_reachable(g.berth.front(),c) || (p.ocean && !ocean_reachable(c))) return "NO_WATER_APPROACH";
  }
  if(std::none_of(g.roadSide.begin(),g.roadSide.end(),[this](Cell c){return roads_.contains(c);})) return "NO_ROAD_ACCESS";
  return {};
}
} // namespace sonnheide
