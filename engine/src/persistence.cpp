#include "sonnheide/simulation.hpp"
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <stdexcept>

namespace sonnheide {
namespace {
void require(bool ok){ if(!ok) throw std::runtime_error("invalid or incompatible kernel checkpoint"); }
std::size_t count(std::istream& in){std::size_t n{}; require(static_cast<bool>(in>>n) && n<=1000000); return n;}
}
void Simulation::save(std::ostream& out) const {
  validate();
  out<<"SONNHEIDE_KERNEL 1\n"<<world_.width_<<' '<<world_.depth_<<' '<<world_.natural_fingerprint()<<'\n';
  for(auto v:world_.natural_) out<<static_cast<int>(v);
  out<<'\n'<<revision_<<' '<<nextId_<<' '<<world_.navigationEpoch_<<' '<<world_.navigation_ready()<<'\n';
  out<<stock_.size()<<'\n'; for(auto& [id,s]:stock_) out<<id<<' '<<s.available<<' '<<s.reserved<<'\n';
  out<<jobs_.size()<<'\n'; for(auto& [id,j]:jobs_) out<<id<<' '<<j.cell.x<<' '<<j.cell.z<<' '<<j.owner<<' '<<j.material<<' '<<j.work<<' '<<static_cast<int>(j.phase)<<'\n';
  out<<world_.roads_.size()<<'\n'; for(auto c:world_.roads_) out<<c.x<<' '<<c.z<<'\n';
  out<<ports_.size()<<'\n'; for(auto& [id,p]:ports_){auto& g=p.geometry; out<<id<<' '<<g.anchor.x<<' '<<g.anchor.z<<' '<<g.width<<' '<<g.depth<<' '<<g.rotation<<' '<<g.ocean<<' '<<g.navigationEpoch<<'\n';}
  out<<receipts_.size()<<'\n'; for(auto& [id,r]:receipts_) out<<std::quoted(id)<<' '<<std::quoted(r.canonical)<<' '<<static_cast<int>(r.principal)<<' '<<r.actor<<' '<<static_cast<int>(r.result.status)<<' '<<std::quoted(r.result.reason)<<' '<<r.result.revision<<' '<<r.result.created<<'\n';
  out<<events_.size()<<'\n'; for(auto& e:events_) out<<e.sequence<<' '<<std::quoted(e.kind)<<' '<<e.entity<<'\n';
  if(!out) throw std::runtime_error("checkpoint write failed");
}
Simulation Simulation::load(std::istream& source){
  std::string bytes; char ch{};
  while(source.get(ch)){ require(bytes.size()<16*1024*1024); bytes.push_back(ch); }
  require(source.eof()); std::istringstream in(bytes);
  std::string magic,mask; int version{},w{},d{}; std::uint64_t fingerprint{};
  require(static_cast<bool>(in>>magic>>version>>w>>d>>fingerprint>>mask) && magic=="SONNHEIDE_KERNEL" && version==1 && w>0 && d>0 &&
    w<=10000 && d<=10000 && static_cast<std::uint64_t>(w)*static_cast<std::uint64_t>(d)<=1000000 && mask.size()==static_cast<std::size_t>(w)*static_cast<std::size_t>(d));
  std::vector<Surface> natural; natural.reserve(mask.size());
  for(char c:mask){require(c=='0'||c=='1');natural.push_back(c=='0'?Surface::Water:Surface::Land);}
  Simulation s(FrozenWorld(w,d,std::move(natural)),{}); require(s.world_.natural_fingerprint()==fingerprint);
  int ready{}; require(static_cast<bool>(in>>s.revision_>>s.nextId_>>s.world_.navigationEpoch_>>ready) && (ready==0 || ready==1));
  for(auto n=count(in);n>0;--n){EntityId id{};Stock v;require(static_cast<bool>(in>>id>>v.available>>v.reserved));require(s.stock_.emplace(id,v).second);}
  for(auto n=count(in);n>0;--n){ReclamationJob j{};int phase{};require(static_cast<bool>(in>>j.id>>j.cell.x>>j.cell.z>>j.owner>>j.material>>j.work>>phase) && phase>=0 && phase<=3);j.phase=static_cast<JobPhase>(phase);require(s.jobs_.emplace(j.id,j).second);
    if(j.phase==JobPhase::Complete) require(s.world_.reclaimed_.insert(j.cell).second);
    else if(j.phase!=JobPhase::Cancelled) require(s.world_.construction_.insert(j.cell).second);
  }
  for(auto n=count(in);n>0;--n){Cell c;require(static_cast<bool>(in>>c.x>>c.z));require(s.world_.roads_.insert(c).second);}
  for(auto n=count(in);n>0;--n){KernelPort p{};auto& g=p.geometry;int ocean{};require(static_cast<bool>(in>>p.id>>g.anchor.x>>g.anchor.z>>g.width>>g.depth>>g.rotation>>ocean>>g.navigationEpoch) && (ocean==0 || ocean==1));g.ocean=ocean==1;require(s.ports_.emplace(p.id,p).second);
    require(s.world_.contains(g.anchor));auto geometry=s.world_.port_geometry(g); require(!geometry.core.empty());
    for(auto c:geometry.core) require(s.world_.occupied_.insert(c).second);
    for(auto c:geometry.berth) require(s.world_.berths_.insert(c).second);
  }
  for(auto n=count(in);n>0;--n){std::string id;Receipt r{};int principal{},status{};require(static_cast<bool>(in>>std::quoted(id)>>std::quoted(r.canonical)>>principal>>r.actor>>status>>std::quoted(r.result.reason)>>r.result.revision>>r.result.created) && principal>=0 && principal<=2 && status>=0 && status<=2 && !id.empty() && id.size()<=128 && r.canonical.size()<=512 && r.result.reason.size()<=128 && r.result.revision<=s.revision_);
    r.principal=static_cast<Principal>(principal);r.result.status=static_cast<Status>(status);require(s.receipts_.emplace(id,std::move(r)).second);
  }
  for(auto n=count(in);n>0;--n){Event e{};require(static_cast<bool>(in>>e.sequence>>std::quoted(e.kind)>>e.entity) && e.kind.size()<=128);s.events_.push_back(std::move(e));}
  in>>std::ws;require(in.eof());s.validate(); if(ready) s.rebuild_navigation(); return s;
}
void save_atomic(const Simulation& s,const std::string& path){
  // Same-directory rename. This is atomic replacement, not a crash-durable/fsync production save journal.
  auto temporary=path+".tmp";
  {std::ofstream out(temporary,std::ios::binary|std::ios::trunc);if(!out) throw std::runtime_error("cannot open checkpoint");s.save(out);out.flush();if(!out) throw std::runtime_error("checkpoint flush failed");}
  std::error_code error;std::filesystem::rename(temporary,path,error);
  if(error){std::filesystem::remove(temporary);throw std::runtime_error("atomic checkpoint rename failed: "+error.message());}
}
} // namespace sonnheide
