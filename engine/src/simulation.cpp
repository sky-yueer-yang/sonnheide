#include "sonnheide/simulation.hpp"
#include <algorithm>
#include <limits>
#include <optional>
#include <queue>
#include <sstream>
#include <stdexcept>
#include <type_traits>

namespace sonnheide {
namespace {
std::optional<Command> decode_canonical(const std::string& text){
  std::istringstream in(text);std::uint64_t revision{};int kind{};char sep{};
  if(!(in>>revision>>sep) || sep!=':' || !(in>>kind>>sep) || sep!=':') return std::nullopt;
  auto comma=[&](){return static_cast<bool>(in>>sep) && sep==',';};
  Payload payload;
  if(kind==0){PlanReclamation p{};if(!(in>>p.cell.x) || !comma() || !(in>>p.cell.z) || !comma() || !(in>>p.materialOwner)) return std::nullopt;payload=p;}
  else if(kind==1){ContributeWork p{};if(!(in>>p.job) || !comma() || !(in>>p.units)) return std::nullopt;payload=p;}
  else if(kind==2){CancelReclamation p{};if(!(in>>p.job)) return std::nullopt;payload=p;}
  else if(kind==3){PlaceRoad p{};if(!(in>>p.cell.x) || !comma() || !(in>>p.cell.z)) return std::nullopt;payload=p;}
  else if(kind==4){KernelPlacePort p{};int ocean{};if(!(in>>p.anchor.x) || !comma() || !(in>>p.anchor.z) || !comma() || !(in>>p.width) || !comma() || !(in>>p.depth) || !comma() || !(in>>p.rotation) || !comma() || !(in>>ocean) || (ocean!=0 && ocean!=1) || !comma() || !(in>>p.navigationEpoch)) return std::nullopt;p.ocean=ocean==1;payload=p;}
  else return std::nullopt;
  in>>std::ws;if(!in.eof()) return std::nullopt;
  Command command{"",revision,payload};if(canonical_command(command)!=text) return std::nullopt;return command;
}
}
std::string canonical_command(const Command& c){
  std::ostringstream s; s << c.expectedRevision << ':' << c.payload.index() << ':';
  std::visit([&](const auto& p){using T=std::decay_t<decltype(p)>;
    if constexpr(std::is_same_v<T,PlanReclamation>) s<<p.cell.x<<','<<p.cell.z<<','<<p.materialOwner;
    else if constexpr(std::is_same_v<T,ContributeWork>) s<<p.job<<','<<p.units;
    else if constexpr(std::is_same_v<T,CancelReclamation>) s<<p.job;
    else if constexpr(std::is_same_v<T,PlaceRoad>) s<<p.cell.x<<','<<p.cell.z;
    else s<<p.anchor.x<<','<<p.anchor.z<<','<<p.width<<','<<p.depth<<','<<p.rotation<<','<<p.ocean<<','<<p.navigationEpoch;
  },c.payload); return s.str();
}
std::string status_name(Status s){switch(s){case Status::Accepted:return "Accepted";case Status::Rejected:return "Rejected";default:return "RequiresRepreview";}}
Simulation::Simulation(FrozenWorld world,std::map<EntityId,Stock> initialStock):world_(std::move(world)),stock_(std::move(initialStock)){validate();}
void Simulation::emit(const std::string& kind,EntityId id){ events_.push_back({events_.size()+1,kind,id}); }
Result Simulation::submit(const Command& c,AuthorityContext a){
  if(c.id.empty() || c.id.size()>128) return {Status::Rejected,"INVALID_COMMAND_ID",revision_,0};
  if(a.principal!=Principal::Player && a.principal!=Principal::Scheduler && a.principal!=Principal::Observer) return {Status::Rejected,"AUTHORITY_DENIED",revision_,0};
  auto canonical=canonical_command(c);
  if(auto it=receipts_.find(c.id);it!=receipts_.end()){
    if(it->second.canonical!=canonical || it->second.principal!=a.principal || it->second.actor!=a.actor)
      return {Status::Rejected,"COMMAND_ID_CONFLICT",revision_,0};
    return it->second.result;
  }
  Result r{Status::RequiresRepreview,"REVISION_STALE",revision_,0};
  if(c.expectedRevision==revision_){
    // Copy-on-commit proof slice. Production will replace whole-state copying with staged write sets.
    auto staged=*this; r=staged.apply(c.payload,a);
    if(r.status==Status::Accepted){
      ++staged.revision_; r.revision=staged.revision_;
      // World writes and deduplication receipt share the same commit even if allocation fails.
      staged.receipts_.emplace(c.id,Receipt{canonical,a.principal,a.actor,r});
      staged.validate();
      static_assert(std::is_nothrow_move_assignable_v<Simulation>);
      *this=std::move(staged); return r;
    }
  }
  receipts_.emplace(c.id,Receipt{canonical,a.principal,a.actor,r}); return r;
}
Result Simulation::apply(const Payload& payload,AuthorityContext a){
  auto reject=[this](std::string why){return Result{Status::Rejected,std::move(why),revision_,0};};
  auto accept=[this](EntityId id=0){return Result{Status::Accepted,{},revision_,id};};
  if(a.principal==Principal::Observer || a.actor==0) return reject("AUTHORITY_DENIED");
  if(revision_>=std::numeric_limits<std::uint64_t>::max()-1) return reject("REVISION_EXHAUSTED");
  if((std::holds_alternative<PlanReclamation>(payload) || std::holds_alternative<KernelPlacePort>(payload)) && nextId_==std::numeric_limits<EntityId>::max()) return reject("ENTITY_ID_EXHAUSTED");
  if((std::holds_alternative<PlanReclamation>(payload) || std::holds_alternative<ContributeWork>(payload) || std::holds_alternative<CancelReclamation>(payload)) && world_.navigation_epoch()>=std::numeric_limits<std::uint64_t>::max()-1) return reject("NAVIGATION_EPOCH_EXHAUSTED");
  return std::visit([&](const auto& p)->Result{ using T=std::decay_t<decltype(p)>;
    if constexpr(std::is_same_v<T,PlanReclamation>){
      if(a.actor!=p.materialOwner) return reject("NOT_OWNER");
      if(!world_.contains(p.cell)) return reject("OUT_OF_BOUNDS");
      if(world_.natural(p.cell)!=Surface::Water || world_.reclaimed(p.cell)) return reject("NOT_WATER");
      if(world_.berth_reserved(p.cell)) return reject("BERTH_PROTECTED");
      if(world_.construction(p.cell) || world_.occupied(p.cell)) return reject("SITE_OCCUPIED");
      if(!world_.adjacent_land(p.cell)) return reject("NO_LAND_ADJACENCY");
      auto owner=stock_.find(p.materialOwner);
      if(owner==stock_.end() || owner->second.available<materialsPerCell) return reject("INSUFFICIENT_MATERIAL");
      auto id=nextId_++; owner->second.available-=materialsPerCell; owner->second.reserved+=materialsPerCell;
      jobs_.emplace(id,ReclamationJob{id,p.cell,p.materialOwner,materialsPerCell,0,JobPhase::Reserved});
      world_.construction_.insert(p.cell); world_.invalidate_navigation(); emit("ReclamationCellJobReserved",id); return accept(id);
    } else if constexpr(std::is_same_v<T,ContributeWork>){
      if(a.principal!=Principal::Scheduler) return reject("AUTHORITY_DENIED");
      auto it=jobs_.find(p.job); if(it==jobs_.end()) return reject("JOB_NOT_FOUND");
      auto& job=it->second;
      if(job.phase==JobPhase::Complete || job.phase==JobPhase::Cancelled) return reject("JOB_TERMINAL");
      if(p.units<=0 || p.units>workPerCell) return reject("INVALID_WORK");
      job.work+=std::min(p.units,workPerCell-job.work); job.phase=JobPhase::UnderConstruction;
      if(job.work==workPerCell){
        stock_.at(job.owner).reserved-=job.material; job.phase=JobPhase::Complete;
        world_.construction_.erase(job.cell); world_.reclaimed_.insert(job.cell); world_.invalidate_navigation();
        emit("ReclaimedLandCompleted",p.job);
      } else emit("ReclamationWorkApplied",p.job);
      return accept();
    } else if constexpr(std::is_same_v<T,CancelReclamation>){
      auto it=jobs_.find(p.job); if(it==jobs_.end()) return reject("JOB_NOT_FOUND");
      auto& job=it->second; if(a.actor!=job.owner) return reject("NOT_OWNER");
      if(job.phase==JobPhase::Complete || job.phase==JobPhase::Cancelled) return reject("JOB_TERMINAL");
      auto& s=stock_.at(job.owner); s.reserved-=job.material; s.available+=job.material; job.phase=JobPhase::Cancelled;
      world_.construction_.erase(job.cell); world_.invalidate_navigation(); emit("ReclamationCancelled",p.job); return accept();
    } else if constexpr(std::is_same_v<T,PlaceRoad>){
      if(!world_.contains(p.cell)) return reject("OUT_OF_BOUNDS");
      if(world_.effective(p.cell)!=Surface::Land) return reject("ROAD_REQUIRES_LAND");
      if(world_.occupied(p.cell) || world_.roads_.contains(p.cell)) return reject("SITE_OCCUPIED");
      world_.roads_.insert(p.cell); emit("KernelRoadPlaced",0); return accept();
    } else {
      auto why=world_.validate_port(p); if(!why.empty()) return reject(why);
      auto id=nextId_++; auto g=world_.port_geometry(p);
      world_.occupied_.insert(g.core.begin(),g.core.end()); world_.berths_.insert(g.berth.begin(),g.berth.end());
      ports_.emplace(id,KernelPort{id,p}); emit("KernelPortGeometryCommitted",id); return accept(id);
    }
  },payload);
}
void Simulation::validate() const {
  if(nextId_==0 || revision_==std::numeric_limits<std::uint64_t>::max()) throw std::runtime_error("ID/revision exhausted");
  std::map<EntityId,Quantity> reserved; std::set<Cell> sites;
  for(auto& [id,j]:jobs_){
    if(id==0 || j.id!=id || id>=nextId_ || !stock_.contains(j.owner) || j.material!=materialsPerCell ||
       j.work<0 || j.work>workPerCell || !world_.contains(j.cell) || world_.natural(j.cell)!=Surface::Water)
      throw std::runtime_error("invalid reclamation job");
    if(j.phase==JobPhase::Reserved || j.phase==JobPhase::UnderConstruction){
      if(!sites.insert(j.cell).second || world_.effective(j.cell)!=Surface::Water || world_.berth_reserved(j.cell) || !world_.adjacent_land(j.cell) ||
         (j.phase==JobPhase::Reserved && j.work!=0) || (j.phase==JobPhase::UnderConstruction && (j.work<=0 || j.work>=workPerCell))) throw std::runtime_error("invalid construction site");
      if(reserved[j.owner]>std::numeric_limits<Quantity>::max()-j.material) throw std::runtime_error("reservation overflow");
      reserved[j.owner]+=j.material;
    } else if(j.phase==JobPhase::Complete){
      if(!world_.reclaimed(j.cell) || j.work!=workPerCell) throw std::runtime_error("completion inconsistent");
    } else if(j.phase!=JobPhase::Cancelled) throw std::runtime_error("unknown job phase");
  }
  if(sites!=world_.construction_) throw std::runtime_error("construction index inconsistent");
  for(auto& [id,s]:stock_) if(id==0 || s.available<0 || s.reserved<0 || s.available>std::numeric_limits<Quantity>::max()-s.reserved || s.reserved!=reserved[id]) throw std::runtime_error("stock/reservation inconsistent");
  std::set<Cell> completed;
  for(auto& [id,j]:jobs_) { (void)id; if(j.phase==JobPhase::Complete && !completed.insert(j.cell).second) throw std::runtime_error("duplicate completed cell"); }
  if(completed!=world_.reclaimed_) throw std::runtime_error("unattributed reclaimed cell");
  // A checkpoint may not introduce an island by forging plausible completed job records.
  std::set<Cell> connected; std::queue<Cell> frontier;
  for(auto cell:completed) {
    for(auto delta:std::vector<Cell>{{1,0},{-1,0},{0,1},{0,-1}}){Cell neighbor{cell.x+delta.x,cell.z+delta.z};
      if(world_.contains(neighbor) && world_.natural(neighbor)==Surface::Land){connected.insert(cell);frontier.push(cell);break;}
    }
  }
  while(!frontier.empty()) {auto cell=frontier.front();frontier.pop();
    for(auto delta:std::vector<Cell>{{1,0},{-1,0},{0,1},{0,-1}}){Cell neighbor{cell.x+delta.x,cell.z+delta.z};
      if(completed.contains(neighbor) && connected.insert(neighbor).second) frontier.push(neighbor);
    }
  }
  if(connected!=completed) throw std::runtime_error("reclaimed island has no natural land support");
  std::set<Cell> ground,berths;
  for(auto& [id,p]:ports_){
    if(id==0 || id!=p.id || id>=nextId_ || jobs_.contains(id) || !world_.contains(p.geometry.anchor)) throw std::runtime_error("invalid port identity");
    auto g=world_.port_geometry(p.geometry); if(g.core.empty()) throw std::runtime_error("invalid port footprint");
    for(auto c:g.core) if(!world_.contains(c) || !world_.reclaimed(c) || !ground.insert(c).second) throw std::runtime_error("invalid port ground");
    for(auto c:g.berth) if(!world_.contains(c) || world_.effective(c)!=Surface::Water || world_.construction(c) || !berths.insert(c).second) throw std::runtime_error("invalid port berth");
    if(std::none_of(g.roadSide.begin(),g.roadSide.end(),[this](Cell c){return world_.roads_.contains(c);})) throw std::runtime_error("port without road");
  }
  if(ground!=world_.occupied_ || berths!=world_.berths_) throw std::runtime_error("port indexes inconsistent");
  for(auto c:world_.roads_) if(!world_.contains(c) || world_.effective(c)!=Surface::Land || world_.occupied(c)) throw std::runtime_error("invalid road");
  if(world_.navigationEpoch_==0 || world_.navigationEpoch_==std::numeric_limits<std::uint64_t>::max() || world_.navigationBuiltEpoch_>world_.navigationEpoch_) throw std::runtime_error("invalid navigation epoch");
  for(std::size_t i=0;i<events_.size();++i) if(events_[i].sequence!=i+1 || events_[i].kind.empty()) throw std::runtime_error("invalid history");
  std::set<std::uint64_t> acceptedRevisions;
  for(auto& [id,receipt]:receipts_){
    auto command=decode_canonical(receipt.canonical);auto& result=receipt.result;
    if(id.empty() || id.size()>128 || !command || result.revision>revision_ ||
       (receipt.principal!=Principal::Player && receipt.principal!=Principal::Scheduler && receipt.principal!=Principal::Observer)) throw std::runtime_error("invalid receipt");
    if(result.status==Status::Accepted){
      if(receipt.principal==Principal::Observer || receipt.actor==0 || !result.reason.empty() || result.revision==0 ||
         command->expectedRevision>=std::numeric_limits<std::uint64_t>::max() || result.revision!=command->expectedRevision+1 ||
         !acceptedRevisions.insert(result.revision).second) throw std::runtime_error("impossible accepted receipt");
      bool consistent=std::visit([&](const auto& p){using T=std::decay_t<decltype(p)>;
        if constexpr(std::is_same_v<T,PlanReclamation>) return jobs_.contains(result.created) && jobs_.at(result.created).cell==p.cell && jobs_.at(result.created).owner==p.materialOwner && receipt.actor==p.materialOwner;
        else if constexpr(std::is_same_v<T,ContributeWork>) return result.created==0 && receipt.principal==Principal::Scheduler && jobs_.contains(p.job) && p.units>0 && p.units<=workPerCell;
        else if constexpr(std::is_same_v<T,CancelReclamation>) return result.created==0 && jobs_.contains(p.job) && jobs_.at(p.job).owner==receipt.actor && jobs_.at(p.job).phase==JobPhase::Cancelled;
        else if constexpr(std::is_same_v<T,PlaceRoad>) return result.created==0 && world_.roads_.contains(p.cell);
        else return ports_.contains(result.created) && canonical_command({"",command->expectedRevision,ports_.at(result.created).geometry})==receipt.canonical;
      },command->payload);
      if(!consistent) throw std::runtime_error("receipt does not describe committed world");
    } else if((result.status!=Status::Rejected && result.status!=Status::RequiresRepreview) || result.created!=0 || result.reason.empty()) throw std::runtime_error("invalid failed receipt");
  }
  if(acceptedRevisions.size()!=revision_ || events_.size()!=revision_) throw std::runtime_error("commit/history/receipt barrier inconsistent");
}
std::uint64_t Simulation::state_fingerprint() const {
  std::ostringstream out; save(out); std::uint64_t h=14695981039346656037ULL;
  for(unsigned char c:out.str()){h^=c;h*=1099511628211ULL;} return h;
}
} // namespace sonnheide
