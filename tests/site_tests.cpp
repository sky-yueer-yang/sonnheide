#include "sonnheide/site.hpp"
#include <algorithm>
#include <iostream>
#include <stdexcept>
#include <string>
#include <utility>

using namespace sonnheide;
using namespace sonnheide::site;
namespace {
void check(bool ok,const char* reason){if(!ok)throw std::runtime_error(reason);}
struct Fixture {
  int size{24};Height cell{4000};
  std::vector<Surface> land=std::vector<Surface>(24*24,Surface::Land);
  std::vector<Height> heights=std::vector<Height>(25*25,0);
  void set(int x,int z,Height h){heights.at(static_cast<std::size_t>(z*(size+1)+x))=h;}
  TerrainGrid terrain() const {return {size,size,cell,land,heights};}
  SiteRegistry registry(std::vector<RoadPortal> portals={{1,{0,0,24,1},0,100}}) const {return {terrain(),std::vector<EntityId>(24*24,7),std::move(portals)};}
};
SiteRequest house(int x=4,EntityId id=10){
  SiteRequest r{};r.id=id;r.actor=7;r.expectedRevision=0;
  r.footprint={x,14,x+2,16};r.constructionEnvelope=r.footprint;
  r.deliveryRoute={x,1,x+1,14};r.deliveryApron={x,13,x+1,14};r.deliveryPortalId=1;r.deliverySocketAcrossMm=x*4000+2000;
  r.entrances.push_back({Mode::Pedestrian,x*4000+2000,14*4000,Facing::North});
  r.approaches.push_back({0,1,r.deliveryRoute,pedestrian_profile()});return r;
}
Rect rotate(Rect r,int n){return {n-r.z1,r.x0,n-r.z0,r.x1};}
void rotate_request(SiteRequest& r,int n,Height cell){
  r.footprint=rotate(r.footprint,n);r.constructionEnvelope=rotate(r.constructionEnvelope,n);
  r.deliveryRoute=rotate(r.deliveryRoute,n);r.deliveryApron=rotate(r.deliveryApron,n);
  if(r.deliverySocketAcrossMm){
    const auto oldAcross=*r.deliverySocketAcrossMm;
    r.deliverySocketAcrossMm=r.deliveryFront==Facing::North || r.deliveryFront==Facing::South?oldAcross:n*cell-oldAcross;
  }
  r.deliveryFront=static_cast<Facing>((static_cast<int>(r.deliveryFront)+1)%4);
  for(auto& e:r.entrances){const auto x=e.xMm;e.xMm=n*cell-e.zMm;e.zMm=x;e.outward=static_cast<Facing>((static_cast<int>(e.outward)+1)%4);}
  for(auto& a:r.approaches)a.area=rotate(a.area,n);
}
Fixture rotate_fixture(const Fixture& old){
  auto f=old;
  for(int z=0;z<=old.size;++z)for(int x=0;x<=old.size;++x)f.set(old.size-z,x,old.heights[static_cast<std::size_t>(z*(old.size+1)+x)]);
  for(int z=0;z<old.size;++z)for(int x=0;x<old.size;++x)f.land[static_cast<std::size_t>(x*old.size+old.size-1-z)]=old.land[static_cast<std::size_t>(z*old.size+x)];
  return f;
}
}
int main(){
  int passed=0;
  const auto test=[&](const char* name,auto body){body();++passed;std::cout<<"PASS "<<name<<'\n';};
  try{
    test("whole footprint catches interior peak and creates one absolute slab",[]{
      Fixture f;f.set(5,15,2000);auto book=f.registry();const auto hash=book.terrain().fingerprint();auto p=book.reserve(house());
      check(static_cast<bool>(p),p.reason.c_str());check(p.plan->floorMm==2150 && p.plan->bottomMm==-300,"foundation ignored interior peak");
      check(book.terrain().fingerprint()==hash && book.terrain().vertex(5,15)==2000,"natural terrain edited");
    });
    test("adjacent independently reachable house preserves prior entrance and height",[]{
      Fixture f;f.set(5,15,2000);f.set(9,15,1000);auto book=f.registry();check(static_cast<bool>(book.reserve(house())),"first house failed");
      const auto first=book.sites().at(10).floorMm;const auto hash=book.terrain().fingerprint();auto b=house(8,11);b.expectedRevision=1;
      auto p=book.reserve(b);check(static_cast<bool>(p),p.reason.c_str());check(book.sites().size()==2 && book.sites().at(10).floorMm==first && book.terrain().fingerprint()==hash,"neighbor changed old site");
    });
    test("later foundation cannot occupy an earlier protected route",[]{
      Fixture f;f.set(5,15,2000);auto book=f.registry();check(static_cast<bool>(book.reserve(house())),"first house failed");
      auto b=house(4,11);b.expectedRevision=1;b.footprint={4,6,6,8};b.constructionEnvelope=b.footprint;b.deliveryRoute={4,1,5,6};b.deliveryApron={4,5,5,6};b.entrances[0].zMm=24000;b.approaches[0].area=b.deliveryRoute;
      check(book.reserve(b).reason=="EXISTING_SITE_ACCESS_PROTECTED","neighbor blocked old ramp");check(book.revision()==1 && book.sites().size()==1,"rejection partially reserved site");
    });
    test("whole vehicle and pedestrian width catches shoulder hill missed by centreline",[]{
      Fixture f;f.set(5,15,2000);f.set(4,6,900);auto book=f.registry();auto r=house();
      check(book.preview(r).reason=="APPROACH_INTERSECTS_TERRAIN","side hill entered ramp surface");
    });
    test("critical points catch terrain intersection between clear grid vertices",[]{
      Fixture f;f.set(4,2,2);f.set(5,2,2);auto book=f.registry();
      check(book.preview(house()).reason=="APPROACH_INTERSECTS_TERRAIN","cubic chord interior was only vertex sampled");
    });
    test("long smooth approach works and short steep approach is rejected",[]{
      Fixture f;f.set(5,15,2000);auto longBook=f.registry();check(static_cast<bool>(longBook.preview(house())),"long ramp failed");
      auto shortBook=f.registry({{1,{0,12,24,13},0,100}});auto r=house();r.deliveryRoute={4,13,5,14};r.deliveryApron=r.deliveryRoute;r.approaches[0].area=r.deliveryRoute;
      check(shortBook.preview(r).reason=="APPROACH_TOO_STEEP","short ramp accepted");
    });
    test("garage requires its own certified vehicle approach",[]{
      Fixture f;f.set(5,15,2000);auto book=f.registry();auto r=house();r.requiresVehicleEntrance=true;
      check(book.preview(r).reason=="GARAGE_APPROACH_REQUIRED","garage used pedestrian-only proof");
      r.entrances.push_back({Mode::Vehicle,18000,56000,Facing::North});r.approaches.push_back({1,1,r.deliveryRoute,vehicle_profile()});
      check(static_cast<bool>(book.preview(r)),"shared straight pedestrian/vehicle approach failed");
    });
    test("accessible pedestrian doorway cannot conceal an inaccessible garage",[]{
      Fixture f;f.set(5,15,2000);auto book=f.registry({{1,{0,0,24,1},0,100},{2,{4,12,5,13},0,100}});auto r=house(5);r.footprint={4,14,6,16};r.constructionEnvelope=r.footprint;r.requiresVehicleEntrance=true;
      r.entrances.push_back({Mode::Vehicle,18000,56000,Facing::North});r.approaches.push_back({1,2,{4,13,5,14},vehicle_profile()});
      check(book.preview(r).reason=="APPROACH_TOO_STEEP","garage accepted using separate good pedestrian path");
    });
    test("car axle clearance rejects a short rounded crest despite acceptable grade",[]{
      Fixture f;f.cell=1000;auto book=f.registry();SiteRequest r{};r.id=10;r.actor=7;r.footprint={4,2,8,4};r.constructionEnvelope=r.footprint;r.deliveryRoute={4,1,8,2};r.deliveryApron=r.deliveryRoute;r.deliveryPortalId=1;r.floorClearanceMm=50;r.requiresVehicleEntrance=true;
      r.entrances={{Mode::Pedestrian,6000,2000,Facing::North},{Mode::Vehicle,6000,2000,Facing::North}};
      r.approaches={{0,1,r.deliveryRoute,pedestrian_profile()},{1,1,r.deliveryRoute,vehicle_profile()}};
      check(book.preview(r).reason=="VEHICLE_BOTTOM_CLEARANCE","car underside intersects rounded crest");
    });
    test("entrances and portals require width rather than point contact",[]{
      Fixture f;auto book=f.registry();auto r=house();r.entrances[0].xMm=16000;
      check(book.preview(r).reason=="ENTRANCE_NOT_CONTINUOUS","corner socket was sufficient");
      auto second=f.registry({{1,{0,0,24,1},0,100},{2,{3,0,4,1},0,100}});r=house();r.approaches[0].portalId=2;
      check(second.preview(r).reason=="PORTAL_NOT_CONTINUOUS","corner road connection was sufficient");
    });
    test("all four orientations use identical absolute floor and access checks",[]{
      Fixture f;f.set(5,15,2000);auto r=house();Rect road{0,0,24,1};
      for(int rotation=0;rotation<4;++rotation){auto book=f.registry({{1,road,0,100}});auto p=book.preview(r);check(static_cast<bool>(p),p.reason.c_str());check(p.plan->floorMm==2150,"rotation changed floor");f=rotate_fixture(f);rotate_request(r,24,4000);road=rotate(road,24);}
    });
    test("negative elevation LAND stays LAND and unknown or WATER sites reject",[]{
      Fixture f;std::fill(f.heights.begin(),f.heights.end(),-100000);auto book=f.registry({{1,{0,0,24,1},-100000,100}});auto p=book.preview(house());check(static_cast<bool>(p) && p.plan->floorMm==-99850,"negative LAND became ocean");
      f.set(5,15,missingHeight);auto unknown=f.registry({{1,{0,0,24,1},-100000,100}});check(unknown.preview(house()).reason=="MISSING_TERRAIN_HEIGHT","NoData silently interpolated");
      Fixture water;water.land[14*24+4]=Surface::Water;auto wet=water.registry();check(wet.preview(house()).reason=="SITE_REQUIRES_LAND","foundation replaced reclamation");
    });
    test("unbuilt foundation cannot supply the material delivery route",[]{
      Fixture f;auto book=f.registry();auto r=house();r.deliveryRoute={4,1,5,15};r.deliveryApron={4,14,5,15};
      check(book.preview(r).reason=="INVALID_SITE_LAYOUT","delivery depended on future raised floor");
    });
    test("material apron must touch the actual front rather than a distant connected road",[]{
      Fixture f;auto book=f.registry();auto r=house();r.deliveryApron={4,2,5,3};
      const auto hash=book.terrain().fingerprint();
      check(book.reserve(r).reason=="DELIVERY_FRONT_NOT_CONTINUOUS","distant apron was a building delivery socket");
      check(book.revision()==0 && book.sites().empty() && book.terrain().fingerprint()==hash,"bad delivery partially reserved site");
      r=house();r.deliveryFront=Facing::South;
      check(book.preview(r).reason=="DELIVERY_FRONT_NOT_CONTINUOUS","apron attached to wrong front");
      r=house();r.deliveryFront=static_cast<Facing>(99);
      check(book.preview(r).reason=="DELIVERY_FRONT_NOT_CONTINUOUS","unknown delivery orientation admitted");
    });
    test("front contact accommodates the transporter width and rejects point or offset sockets",[]{
      Fixture f;auto book=f.registry();auto r=house();r.deliverySocketAcrossMm=16000;
      check(book.preview(r).reason=="DELIVERY_FRONT_NOT_CONTINUOUS","zero-width front socket admitted");
      r.deliverySocketAcrossMm=16499;
      check(book.preview(r).reason=="DELIVERY_FRONT_NOT_CONTINUOUS","one-millimetre half-width deficit admitted");
      r.deliverySocketAcrossMm=16500;
      check(static_cast<bool>(book.preview(r)),"exact transporter width was rejected");
      r.deliverySocketAcrossMm=std::numeric_limits<Height>::min();
      check(book.preview(r).reason=="DELIVERY_FRONT_NOT_CONTINUOUS","hostile delivery coordinate admitted");
      r.deliverySocketAcrossMm=std::numeric_limits<Height>::max();
      check(book.preview(r).reason=="DELIVERY_FRONT_NOT_CONTINUOUS","overflow-sized delivery coordinate admitted");
      r=house();r.deliveryApron={6,13,7,14};r.deliveryRoute={6,1,7,14};
      check(book.preview(r).reason=="DELIVERY_FRONT_NOT_CONTINUOUS","corner-only apron connection admitted");
    });
    test("default North delivery socket uses footprint midpoint on the existing front ground",[]{
      Fixture f;auto book=f.registry();auto r=house();r.deliverySocketAcrossMm.reset();
      r.deliveryRoute={4,1,6,14};r.deliveryApron={4,13,6,14};
      check(r.deliveryFront==Facing::North && static_cast<bool>(book.preview(r)),"default front midpoint unavailable");
      r.constructionEnvelope={4,13,6,16};
      check(book.preview(r).reason=="INVALID_SITE_LAYOUT","apron entered the construction enclosure");
    });
    test("moved-from terrain queries and registry construction safely reject unavailable storage",[]{
      auto original=Fixture{}.terrain();const auto hash=original.fingerprint();
      auto retained=std::move(original);
      check(retained.fingerprint()==hash && retained.valid({0,0,1,1}),"move changed retained terrain");
      check(!original.valid({0,0,1,1}) && !original.all_land({0,0,1,1}) && !original.extrema({0,0,1,1}) && !original.ground_walkable({0,0,1,1},1,3),"moved-from grid remained queryable");
      bool vertexRejected=false, fingerprintRejected=false, registryRejected=false;
      try{(void)original.vertex(0,0);}catch(const std::out_of_range&){vertexRejected=true;}
      try{(void)original.fingerprint();}catch(const std::logic_error&){fingerprintRejected=true;}
      try{SiteRegistry invalid(std::move(original),std::vector<EntityId>(24*24,7),{});}catch(const std::invalid_argument&){registryRejected=true;}
      check(vertexRejected && fingerprintRejected && registryRejected,"moved-from terrain reached unchecked storage");
      auto next=Fixture{}.terrain();retained=std::move(next);
      check(!next.valid({0,0,1,1}) && retained.fingerprint()==hash,"move assignment invalidated the wrong grid");
    });
    test("individually clear North and East ramps cannot reserve incompatible overlapping surfaces",[]{
      constexpr int n=200;std::vector<Height> heights((n+1)*(n+1),0);
      for(int z=80;z<=110;++z)for(int x=170;x<=171;++x)heights[static_cast<std::size_t>(z*(n+1)+x)]=1000;
      TerrainGrid terrain(n,n,1000,std::vector<Surface>(n*n,Surface::Land),std::move(heights));
      SiteRegistry book(std::move(terrain),std::vector<EntityId>(n*n,1),
        {{1,{100,39,130,40},0,1},{2,{170,80,171,110},1000,1},{3,{100,150,105,151},0,1}});
      SiteRequest r{};r.id=1;r.actor=1;r.footprint={100,100,110,110};r.constructionEnvelope=r.footprint;r.requestedFloorMm=3000;
      r.deliveryFront=Facing::South;r.deliverySocketAcrossMm=102500;r.deliveryRoute={100,110,105,150};r.deliveryApron={100,110,105,111};r.deliveryPortalId=3;
      r.entrances={{Mode::Pedestrian,105000,100000,Facing::North},{Mode::Pedestrian,110000,105000,Facing::East}};
      r.approaches={{0,1,{100,40,130,100},pedestrian_profile()},{1,2,{110,80,170,110},pedestrian_profile()}};
      auto north=r;north.entrances.resize(1);north.approaches.resize(1);
      check(static_cast<bool>(book.preview(north)),"North surface alone was not a valid control");
      auto east=r;east.entrances.erase(east.entrances.begin());east.approaches.erase(east.approaches.begin());east.approaches[0].entranceIndex=0;
      check(static_cast<bool>(book.preview(east)),"East surface alone was not a valid control");
      check(book.reserve(r).reason=="APPROACHES_SELF_INTERSECT","individually clear but incompatible ramp surfaces combined");
      check(book.revision()==0 && book.sites().empty(),"self-intersection partially reserved a site");
    });
    test("rights, stale revisions, excessive foundation and hostile coordinates reject",[]{
      Fixture f;auto book=f.registry();auto r=house();r.actor=8;check(book.preview(r).reason=="SITE_RIGHTS_REQUIRED","foreign land reserved");
      r=house();r.requestedFloorMm=7000;check(book.preview(r).reason=="FOUNDATION_TOO_TALL","free tall platform accepted");
      r=house();r.entrances[0].xMm=std::numeric_limits<Height>::min();check(book.preview(r).reason=="ENTRANCE_NOT_CONTINUOUS","hostile coordinate accepted");
      r=house();r.approaches[0].profile.widthMm=1;check(book.preview(r).reason=="UNREGISTERED_MOTION_PROFILE","renderer narrowed motion profile");
      check(static_cast<bool>(book.reserve(house())),"baseline failed");r=house(8,11);check(book.preview(r).reason=="STALE_SITE_PREVIEW","stale footprint accepted");check(book.revision()==1,"failed preview mutated registry");
    });
    std::cout<<passed<<" terrain/site scenarios passed\n";
  }catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}
}
