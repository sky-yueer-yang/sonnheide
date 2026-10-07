#include "sonnheide/site.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <stdexcept>
#include <type_traits>

namespace sonnheide::site {
namespace {
constexpr Height heightLimit=20'000'000;
bool valid_mode(Mode m){return m==Mode::Pedestrian || m==Mode::Vehicle;}
bool valid_facing(Facing f){return f==Facing::North || f==Facing::East || f==Facing::South || f==Facing::West;}
bool valid_profile(const MotionProfile& p){
  return valid_mode(p.mode) && p.widthMm>0 && p.widthMm<=20000 && p.clearanceMm>0 && p.clearanceMm<=20000 &&
    p.maxGradeNumerator>0 && p.maxGradeDenominator>=2 && p.maxGradeDenominator<=1000 && p.maxGradeNumerator<=p.maxGradeDenominator &&
    (p.mode==Mode::Pedestrian || (p.wheelbaseMm>0 && p.wheelbaseMm<=20000 && p.allowableSagMm>0 && p.allowableSagMm<=1000));
}
// Strict positive-width contact; a touching corner never connects a route.
bool edge_contact_width(Rect a,Rect b,Height cell,Height width){
  const auto xOverlap=static_cast<Height>(std::min(a.x1,b.x1))-std::max(a.x0,b.x0);
  const auto zOverlap=static_cast<Height>(std::min(a.z1,b.z1))-std::max(a.z0,b.z0);
  return ((a.x1==b.x0 || b.x1==a.x0) && zOverlap*cell>=width) ||
         ((a.z1==b.z0 || b.z1==a.z0) && xOverlap*cell>=width);
}
std::string validate_delivery_front(const TerrainGrid& terrain,const SiteRequest& request){
  if(!valid_facing(request.deliveryFront))return "DELIVERY_FRONT_NOT_CONTINUOUS";
  const auto& envelope=request.constructionEnvelope;
  const auto& apron=request.deliveryApron;
  bool attached=false;
  switch(request.deliveryFront){
    case Facing::North: attached=apron.z1==envelope.z0;break;
    case Facing::East: attached=apron.x0==envelope.x1;break;
    case Facing::South: attached=apron.z0==envelope.z1;break;
    case Facing::West: attached=apron.x1==envelope.x0;break;
  }
  const bool axisX=request.deliveryFront==Facing::East || request.deliveryFront==Facing::West;
  const auto cell=terrain.cell_size_mm();
  const auto span=[&](Rect rectangle){
    return std::pair{static_cast<Height>(axisX?rectangle.z0:rectangle.x0)*cell,
                     static_cast<Height>(axisX?rectangle.z1:rectangle.x1)*cell};
  };
  const auto footprintSpan=span(request.footprint), envelopeSpan=span(envelope), apronSpan=span(apron);
  const auto socket=request.deliverySocketAcrossMm.value_or(footprintSpan.first+(footprintSpan.second-footprintSpan.first)/2);
  // Check the bounded coordinate before +/- half-width, including hostile int64 input.
  if(!attached || socket<footprintSpan.first || socket>footprintSpan.second)return "DELIVERY_FRONT_NOT_CONTINUOUS";
  const auto halfWidth=(pedestrian_profile().widthMm+1)/2;
  for(const auto contact:std::array{footprintSpan,envelopeSpan,apronSpan})
    if(socket-halfWidth<contact.first || socket+halfWidth>contact.second)return "DELIVERY_FRONT_NOT_CONTINUOUS";
  return {};
}
struct Vertex {long double axis, height;};
long double smooth(long double u){return u*u*(3-2*u);}
// Exact critical points of cubic ramp minus linear terrain along a triangle edge.
// Interior minima reduce to edges because terrain is affine across ramp width.
bool clear_edge(Vertex a,Vertex b,long double origin,long double length,long double y0,long double delta){
  const auto u0=(a.axis-origin)/length, du=(b.axis-a.axis)/length, dh=b.height-a.height;
  const auto gap=[&](long double t){const auto u=u0+du*t;return y0+delta*smooth(u)-(a.height+dh*t);};
  long double minimum=std::min(gap(0),gap(1));
  const auto qa=-6*delta*du*du*du;
  const auto qb=6*delta*du*du*(1-2*u0);
  const auto qc=6*delta*du*u0*(1-u0)-dh;
  const auto inspect=[&](long double t){if(t>0 && t<1) minimum=std::min(minimum,gap(t));};
  if(std::abs(qa)<1e-20L){if(std::abs(qb)>1e-20L) inspect(-qc/qb);}
  else {
    const auto discriminant=qb*qb-4*qa*qc;
    if(discriminant>=0){const auto root=std::sqrt(discriminant);inspect((-qb-root)/(2*qa));inspect((-qb+root)/(2*qa));}
  }
  return minimum>=-1e-6L; // Only numerical noise in mm; production needs interval bounds.
}
bool ramp_clear(const TerrainGrid& terrain,const Ramp& ramp,bool axisX,Height y0,Height y1){
  const auto cell=terrain.cell_size_mm();
  const auto origin=static_cast<long double>(axisX?ramp.area.x0:ramp.area.z0)*cell;
  const auto length=static_cast<long double>(axisX?ramp.area.x1-ramp.area.x0:ramp.area.z1-ramp.area.z0)*cell;
  const auto point=[&](int x,int z){return Vertex{static_cast<long double>(axisX?x:z)*cell,static_cast<long double>(terrain.vertex(x,z))};};
  for(int z=ramp.area.z0;z<ramp.area.z1;++z) for(int x=ramp.area.x0;x<ramp.area.x1;++x){
    const auto a=point(x,z), b=point(x+1,z), c=point(x+1,z+1), d=point(x,z+1);
    for(const auto& triangle:std::array<std::array<Vertex,3>,2>{{{a,b,c},{a,c,d}}})
      for(int i=0;i<3;++i) if(!clear_edge(triangle[static_cast<std::size_t>(i)],triangle[static_cast<std::size_t>((i+1)%3)],origin,length,y0,static_cast<long double>(y1)-y0)) return false;
  }
  return true;
}
std::string validate_ramp(const TerrainGrid& terrain,const SiteRequest& request,Height floor,const Ramp& ramp,const RoadPortal& portal){
  if(ramp.entranceIndex>=request.entrances.size() || !valid_profile(ramp.profile)) return "INVALID_APPROACH_PROFILE";
  const auto& door=request.entrances[ramp.entranceIndex];
  if(!valid_mode(door.mode) || !valid_facing(door.outward) || ramp.profile.mode!=door.mode) return "ENTRANCE_MODE_MISMATCH";
  const auto cell=terrain.cell_size_mm();
  const auto x0=static_cast<Height>(request.footprint.x0)*cell, x1=static_cast<Height>(request.footprint.x1)*cell;
  const auto z0=static_cast<Height>(request.footprint.z0)*cell, z1=static_cast<Height>(request.footprint.z1)*cell;
  if(door.xMm<x0 || door.xMm>x1 || door.zMm<z0 || door.zMm>z1) return "ENTRANCE_NOT_CONTINUOUS";
  if(ramp.profile!=(door.mode==Mode::Pedestrian?pedestrian_profile():vehicle_profile())) return "UNREGISTERED_MOTION_PROFILE";
  const bool axisX=door.outward==Facing::East || door.outward==Facing::West;
  const bool positive=door.outward==Facing::North || door.outward==Facing::West; // Min end is road.
  const auto& r=ramp.area;
  bool attached=false, roadContact=false;
  switch(door.outward){
    case Facing::North: attached=door.zMm==z0 && r.z1==request.footprint.z0;roadContact=portal.area.z1==r.z0 && portal.area.x0<=r.x0 && portal.area.x1>=r.x1;break;
    case Facing::South: attached=door.zMm==z1 && r.z0==request.footprint.z1;roadContact=portal.area.z0==r.z1 && portal.area.x0<=r.x0 && portal.area.x1>=r.x1;break;
    case Facing::East: attached=door.xMm==x1 && r.x0==request.footprint.x1;roadContact=portal.area.x0==r.x1 && portal.area.z0<=r.z0 && portal.area.z1>=r.z1;break;
    case Facing::West: attached=door.xMm==x0 && r.x1==request.footprint.x0;roadContact=portal.area.x1==r.x0 && portal.area.z0<=r.z0 && portal.area.z1>=r.z1;break;
  }
  const auto across=axisX?door.zMm:door.xMm;
  const auto acrossMin=static_cast<Height>(axisX?r.z0:r.x0)*cell;
  const auto acrossMax=static_cast<Height>(axisX?r.z1:r.x1)*cell;
  const auto footprintMin=axisX?z0:x0, footprintMax=axisX?z1:x1;
  const auto halfWidth=(ramp.profile.widthMm+1)/2;
  if(!attached || across-halfWidth<acrossMin || across+halfWidth>acrossMax || across-halfWidth<footprintMin || across+halfWidth>footprintMax) return "ENTRANCE_NOT_CONTINUOUS";
  if(!roadContact) return "PORTAL_NOT_CONTINUOUS";
  const auto length=static_cast<long double>(axisX?r.x1-r.x0:r.z1-r.z0)*cell;
  const auto rise=std::abs(static_cast<long double>(floor)-portal.heightMm);
  if(1.5L*rise*ramp.profile.maxGradeDenominator>length*ramp.profile.maxGradeNumerator) return "APPROACH_TOO_STEEP";
  if(ramp.profile.mode==Mode::Vehicle && 6*rise*ramp.profile.wheelbaseMm*ramp.profile.wheelbaseMm>8*static_cast<long double>(ramp.profile.allowableSagMm)*length*length) return "VEHICLE_BOTTOM_CLEARANCE";
  const auto low=positive?portal.heightMm:floor, high=positive?floor:portal.heightMm;
  if(!ramp_clear(terrain,ramp,axisX,low,high)) return "APPROACH_INTERSECTS_TERRAIN";
  return {};
}
} // namespace
bool overlaps(Rect a,Rect b){return std::max(a.x0,b.x0)<std::min(a.x1,b.x1) && std::max(a.z0,b.z0)<std::min(a.z1,b.z1);}
bool contains(Rect a,Rect b){return a.x0<=b.x0 && a.z0<=b.z0 && a.x1>=b.x1 && a.z1>=b.z1;}
TerrainGrid::TerrainGrid(int w,int d,Height cell,std::vector<Surface> natural,std::vector<Height> vertices):width_(w),depth_(d),cellSize_(cell),natural_(std::move(natural)),vertices_(std::move(vertices)){
  if(w<=0 || d<=0 || w>1024 || d>1024 || static_cast<std::int64_t>(w)*d>1'000'000 || cell<1000 || cell>20000 || natural_.size()!=static_cast<std::size_t>(w)*static_cast<std::size_t>(d) || vertices_.size()!=static_cast<std::size_t>(w+1)*static_cast<std::size_t>(d+1)) throw std::invalid_argument("Invalid terrain grid");
  for(auto s:natural_) if(s!=Surface::Land && s!=Surface::Water) throw std::invalid_argument("Invalid natural surface");
  for(auto h:vertices_) if(h!=missingHeight && (h < -heightLimit || h > heightLimit)) throw std::invalid_argument("Unbounded elevation");
}
bool TerrainGrid::storage_valid() const noexcept{
  return width_>0 && depth_>0 && width_<=1024 && depth_<=1024 &&
    natural_.size()==static_cast<std::size_t>(width_)*static_cast<std::size_t>(depth_) &&
    vertices_.size()==static_cast<std::size_t>(width_+1)*static_cast<std::size_t>(depth_+1);
}
bool TerrainGrid::valid(Rect r) const{return storage_valid() && r.x0>=0 && r.z0>=0 && r.x1<=width_ && r.z1<=depth_ && r.x0<r.x1 && r.z0<r.z1;}
bool TerrainGrid::all_land(Rect r) const{
  if(!valid(r)) return false;
  for(int z=r.z0;z<r.z1;++z) for(int x=r.x0;x<r.x1;++x) if(natural_[static_cast<std::size_t>(z)*static_cast<std::size_t>(width_)+static_cast<std::size_t>(x)]!=Surface::Land) return false;
  return true;
}
Height TerrainGrid::vertex(int x,int z) const{
  if(!storage_valid() || x<0 || z<0 || x>width_ || z>depth_) throw std::out_of_range("Terrain vertex");
  return vertices_[static_cast<std::size_t>(z)*static_cast<std::size_t>(width_+1)+static_cast<std::size_t>(x)];
}
std::optional<std::pair<Height,Height>> TerrainGrid::extrema(Rect r) const{
  if(!valid(r)) return std::nullopt;
  Height minimum=heightLimit, maximum=-heightLimit;
  for(int z=r.z0;z<=r.z1;++z) for(int x=r.x0;x<=r.x1;++x){const auto h=vertex(x,z);if(h==missingHeight)return std::nullopt;minimum=std::min(minimum,h);maximum=std::max(maximum,h);}
  return std::pair{minimum,maximum};
}
bool TerrainGrid::ground_walkable(Rect r,Height rise,Height run) const{
  if(!all_land(r) || !extrema(r) || rise<=0 || run<=0) return false;
  const auto limit=static_cast<long double>(rise)*cellSize_/run;
  for(int z=r.z0;z<r.z1;++z) for(int x=r.x0;x<r.x1;++x){
    const auto a=vertex(x,z),b=vertex(x+1,z),c=vertex(x+1,z+1),d=vertex(x,z+1);
    const auto first=std::hypot(static_cast<long double>(b)-a,static_cast<long double>(c)-b);
    const auto second=std::hypot(static_cast<long double>(c)-d,static_cast<long double>(d)-a);
    if(first>limit || second>limit) return false;
  }
  return true;
}
std::uint64_t TerrainGrid::fingerprint() const{
  if(!storage_valid())throw std::logic_error("Unavailable terrain grid");
  std::uint64_t h=1469598103934665603ULL;
  const auto add=[&](std::uint64_t n){for(unsigned i=0;i<8;++i){h^=(n>>(8*i))&255ULL;h*=1099511628211ULL;}};
  add(static_cast<std::uint64_t>(width_));add(static_cast<std::uint64_t>(depth_));add(static_cast<std::uint64_t>(cellSize_));
  for(auto s:natural_) add(static_cast<std::uint64_t>(s));
  for(auto v:vertices_) add(static_cast<std::uint64_t>(v));
  return h;
}
MotionProfile pedestrian_profile(){return {Mode::Pedestrian,1000,2200,1,12,0,0};}
MotionProfile vehicle_profile(){return {Mode::Vehicle,3000,3500,1,8,2500,140};}
SiteRegistry::SiteRegistry(TerrainGrid terrain,std::vector<EntityId> owners,std::vector<RoadPortal> portals):terrain_(std::move(terrain)),owners_(std::move(owners)){
  if(!terrain_.valid({0,0,terrain_.width(),terrain_.depth()}) || owners_.size()!=static_cast<std::size_t>(terrain_.width())*static_cast<std::size_t>(terrain_.depth())) throw std::invalid_argument("Invalid terrain or ownership grid");
  for(const auto& p:portals){
    const auto bounds=terrain_.extrema(p.area);
    if(p.id==0 || p.connectedNetworkId==0 || !terrain_.all_land(p.area) || !bounds || bounds->first!=p.heightMm || bounds->second!=p.heightMm || !portals_.emplace(p.id,p).second) throw std::invalid_argument("Portal must be a known level LAND area on a certified network");
  }
}
bool SiteRegistry::owned(Rect area,EntityId actor) const{
  if(!terrain_.valid(area))return false;
  for(int z=area.z0;z<area.z1;++z) for(int x=area.x0;x<area.x1;++x) if(owners_[static_cast<std::size_t>(z)*static_cast<std::size_t>(terrain_.width())+static_cast<std::size_t>(x)]!=actor) return false;
  return true;
}
PlanResult SiteRegistry::preview(const SiteRequest& r) const{
  const auto reject=[](std::string reason){return PlanResult{std::move(reason),std::nullopt};};
  if(r.expectedRevision!=revision_)return reject("STALE_SITE_PREVIEW");
  if(revision_==std::numeric_limits<std::uint64_t>::max())return reject("REVISION_EXHAUSTED");
  if(r.id==0 || r.actor==0 || sites_.contains(r.id))return reject("INVALID_OR_DUPLICATE_SITE_ID");
  if(r.entrances.empty() || r.entrances.size()>16 || r.approaches.size()!=r.entrances.size())return reject("MISSING_REQUIRED_APPROACH");
  if(r.floorClearanceMm<=0 || r.floorClearanceMm>1000 || r.embedMm<=0 || r.embedMm>1000 || r.maxFoundationExposureMm<=0 || r.maxFoundationExposureMm>10000)return reject("INVALID_FOUNDATION_BUDGET");
  if(!contains(r.constructionEnvelope,r.footprint) || overlaps(r.constructionEnvelope,r.deliveryRoute) || !contains(r.deliveryRoute,r.deliveryApron))return reject("INVALID_SITE_LAYOUT");
  std::vector<Rect> areas{r.footprint,r.constructionEnvelope,r.deliveryRoute,r.deliveryApron};
  for(const auto& approach:r.approaches){if(overlaps(approach.area,r.footprint))return reject("APPROACH_OVERLAPS_BUILDING");areas.push_back(approach.area);}
  for(std::size_t i=0;i<r.approaches.size();++i) for(std::size_t j=0;j<i;++j){
    const auto& a=r.approaches[i];const auto& b=r.approaches[j];
    if(overlaps(a.area,b.area)){
      if(a.entranceIndex>=r.entrances.size() || b.entranceIndex>=r.entrances.size() || a.area!=b.area || a.portalId!=b.portalId || r.entrances[a.entranceIndex].outward!=r.entrances[b.entranceIndex].outward) return reject("APPROACHES_SELF_INTERSECT");
    }
  }
  for(const auto area:areas){
    if(!terrain_.all_land(area))return reject("SITE_REQUIRES_LAND");
    if(!terrain_.extrema(area))return reject("MISSING_TERRAIN_HEIGHT");
    if(!owned(area,r.actor))return reject("SITE_RIGHTS_REQUIRED");
    for(const auto& entry:portals_) if(overlaps(area,entry.second.area))return reject("PUBLIC_PORTAL_PROTECTED");
    for(const auto& entry:sites_) for(const auto protectedArea:entry.second.protectedAreas) if(overlaps(area,protectedArea))return reject("EXISTING_SITE_ACCESS_PROTECTED");
  }
  const auto elevations=terrain_.extrema(r.footprint).value();
  const auto minimumFloor=elevations.second+r.floorClearanceMm;
  const auto floor=r.requestedFloorMm.value_or(minimumFloor);
  if(floor < minimumFloor || floor>heightLimit)return reject("FLOOR_BELOW_TERRAIN_OR_UNBOUNDED");
  if(floor-elevations.first>r.maxFoundationExposureMm)return reject("FOUNDATION_TOO_TALL");
  const auto bottom=elevations.first-r.embedMm;
  if(const auto reason=validate_delivery_front(terrain_,r);!reason.empty())return reject(reason);
  const auto delivery=portals_.find(r.deliveryPortalId);
  if(delivery==portals_.end() || !edge_contact_width(r.deliveryRoute,delivery->second.area,terrain_.cell_size_mm(),pedestrian_profile().widthMm) || !terrain_.ground_walkable(r.deliveryRoute,1,3))return reject("DELIVERY_NOT_REACHABLE_ON_CURRENT_GROUND");
  bool pedestrian=false, vehicle=false;
  std::vector<bool> covered(r.entrances.size(),false);
  for(const auto& approach:r.approaches){
    if(approach.entranceIndex>=covered.size() || covered[approach.entranceIndex])return reject("MISSING_REQUIRED_APPROACH");
    const auto portal=portals_.find(approach.portalId);
    if(portal==portals_.end())return reject("UNKNOWN_ACCESS_PORTAL");
    if(const auto reason=validate_ramp(terrain_,r,floor,approach,portal->second);!reason.empty())return reject(reason);
    covered[approach.entranceIndex]=true;pedestrian=pedestrian || approach.profile.mode==Mode::Pedestrian;vehicle=vehicle || approach.profile.mode==Mode::Vehicle;
  }
  if(!pedestrian)return reject("PEDESTRIAN_ENTRY_REQUIRED");
  if(r.requiresVehicleEntrance && !vehicle)return reject("GARAGE_APPROACH_REQUIRED");
  return {{},SitePlan{r,bottom,floor,terrain_.fingerprint(),std::move(areas)}};
}
PlanResult SiteRegistry::reserve(const SiteRequest& request){
  auto result=preview(request);if(!result)return result;
  auto staged=sites_;staged.emplace(request.id,*result.plan);
  static_assert(std::is_nothrow_move_assignable_v<decltype(sites_)>);
  static_assert(std::is_nothrow_move_constructible_v<PlanResult>);
  sites_=std::move(staged);++revision_;return result;
}
} // namespace sonnheide::site
