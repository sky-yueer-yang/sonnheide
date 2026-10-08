#include "sonnheide/earth_world.hpp"
#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <limits>
#include <map>
#include <numbers>
#include <sstream>
#include <stdexcept>

namespace sonnheide::earth {
namespace {
constexpr double pi=std::numbers::pi, rad=pi/180., radius=6371008.8;
constexpr const char* archive_hash="28600e8f7a08645aab43079326df6504212ec5ccb2b4bcf3b5f4f12ed60e82bc";
void require(bool ok,const char* message) { if(!ok) throw std::runtime_error(message); }
void cancel(const std::function<bool()>& cancelled) { if(cancelled && cancelled()) throw std::runtime_error("World creation cancelled"); }
double longitude(double x) { return std::remainder(x,360.); }
bool finite(double x) { return std::isfinite(x); }
bool inside(Bounds b,Point p) { return p.x>=b.min_x&&p.x<=b.max_x&&p.y>=b.min_y&&p.y<=b.max_y; }
double smooth(double t) { t=std::clamp(t,0.,1.); return t*t*(3.-2.*t); }
// Independent SHA-256 implementation; admits actual files, not a trusted receipt.
class Sha256 {
 std::array<std::uint32_t,8> state_{0x6a09e667,0xbb67ae85,0x3c6ef372,0xa54ff53a,0x510e527f,0x9b05688c,0x1f83d9ab,0x5be0cd19};
 std::array<std::uint8_t,64> block_{}; std::size_t used_{}; std::uint64_t length_{};
 void compress() {
  constexpr std::array<std::uint32_t,64> k{0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2};
  std::array<std::uint32_t,64> w{};
  for(std::size_t i=0;i<16;++i) w[i]=(std::uint32_t(block_[4*i])<<24)|(std::uint32_t(block_[4*i+1])<<16)|(std::uint32_t(block_[4*i+2])<<8)|block_[4*i+3];
  for(std::size_t i=16;i<64;++i) { const auto a=w[i-15],b=w[i-2]; w[i]=w[i-16]+(std::rotr(a,7)^std::rotr(a,18)^(a>>3))+w[i-7]+(std::rotr(b,17)^std::rotr(b,19)^(b>>10)); }
  auto a=state_[0],b=state_[1],c=state_[2],d=state_[3],e=state_[4],f=state_[5],g=state_[6],h=state_[7];
  for(std::size_t i=0;i<64;++i) { auto t1=h+(std::rotr(e,6)^std::rotr(e,11)^std::rotr(e,25))+((e&f)^((~e)&g))+k[i]+w[i]; auto t2=(std::rotr(a,2)^std::rotr(a,13)^std::rotr(a,22))+((a&b)^(a&c)^(b&c)); h=g;g=f;f=e;e=d+t1;d=c;c=b;b=a;a=t1+t2; }
  state_[0]+=a;state_[1]+=b;state_[2]+=c;state_[3]+=d;state_[4]+=e;state_[5]+=f;state_[6]+=g;state_[7]+=h;
 }
public:
 void add(const void* ptr,std::size_t count) { const auto* data=static_cast<const std::uint8_t*>(ptr); length_+=count; while(count) { auto take=std::min(count,64-used_);std::memcpy(block_.data()+used_,data,take);used_+=take;data+=take;count-=take;if(used_==64){compress();used_=0;} } }
 std::string finish() { auto bits=length_*8;std::uint8_t one=0x80,zero=0;add(&one,1);while(used_!=56)add(&zero,1);std::array<std::uint8_t,8>tail{};for(int i=0;i<8;++i)tail[std::size_t(7-i)]=std::uint8_t(bits>>(8*i));add(tail.data(),8);std::ostringstream s;s.exceptions(std::ios::badbit|std::ios::failbit);s<<std::hex<<std::setfill('0');for(auto v:state_)s<<std::setw(8)<<v;return s.str(); }
};
std::string file_hash(const std::filesystem::path& p) {
 std::ifstream f(p,std::ios::binary); require(bool(f),"Missing admitted GSHHG file");Sha256 hash;std::array<char,65536>buf{};while(f){f.read(buf.data(),buf.size());hash.add(buf.data(),std::size_t(f.gcount()));}require(f.eof(),"Failed GSHHG read");return hash.finish();
}
std::int32_t read_int(std::istream& f) { std::array<unsigned char,4>b{};f.read(reinterpret_cast<char*>(b.data()),4);require(bool(f),"Truncated GSHHG binary");return std::bit_cast<std::int32_t>((std::uint32_t(b[0])<<24)|(std::uint32_t(b[1])<<16)|(std::uint32_t(b[2])<<8)|b[3]); }
std::vector<Ring> read_rings(const std::filesystem::path& path,std::uintmax_t bytes,const char* hash) {
 require(std::filesystem::is_regular_file(path)&&!std::filesystem::is_symlink(path)&&std::filesystem::file_size(path)==bytes,"GSHHG source size/type mismatch");
 require(file_hash(path)==hash,"GSHHG source SHA-256 mismatch");
 std::ifstream f(path,std::ios::binary);std::vector<Ring>out;
 while(f.peek()!=std::char_traits<char>::eof()) {
  std::array<std::int32_t,11>h{};for(auto&v:h)v=read_int(f);require(h[1]>=4&&h[1]<=8000000,"Invalid GSHHG ring length");auto level=std::uint8_t(h[2]&255);require(level>=1&&level<=6,"Invalid GSHHG topology level");
  Ring ring;ring.id=h[0];ring.container=h[9];ring.level=level;ring.west=h[3]*1e-6;ring.east=h[4]*1e-6;ring.south=h[5]*1e-6;ring.north=h[6]*1e-6;
  ring.vertices.reserve(std::size_t(h[1])+2);const bool greenwich=(h[2]>>16)&1;
  for(int i=0;i<h[1];++i){double x=read_int(f)*1e-6,y=read_int(f)*1e-6;require(x>=-180&&x<=360&&y>=-90&&y<=90,"Invalid GSHHG coordinate");if(greenwich&&x>270)x-=360;ring.vertices.push_back({x,y});}
  if(level==6)continue; // One Antarctic convention; never double-count both alternatives.
  if(level==5){ring.level=1;
   const auto first=ring.vertices.front(),last=ring.vertices.back();
   const bool polar_cap=ring.south<=-89.999&&ring.west<=-179.999&&ring.east>=179.999
       &&std::abs(first.longitude-180.)<1e-6&&std::abs(last.longitude+180.)<1e-6
       &&std::abs(first.latitude-last.latitude)<1e-6;
   if(polar_cap){ring.vertices.push_back({-180,-90});ring.vertices.push_back({180,-90});}
  }
  out.push_back(std::move(ring));
 }
 require(!out.empty(),"Empty GSHHG source");return out;
}
double segment_distance(Point p,Point a,Point b) { double x=b.x-a.x,y=b.y-a.y,len=x*x+y*y,t=len>0?std::clamp(((p.x-a.x)*x+(p.y-a.y)*y)/len,0.,1.):0.;return std::hypot(p.x-a.x-t*x,p.y-a.y-t*y); }
std::uint64_t mix(std::uint64_t x) { x^=x>>30;x*=0xbf58476d1ce4e5b9ULL;x^=x>>27;x*=0x94d049bb133111ebULL;return x^(x>>31); }
double closure_signed(Point p, const WorldConfig& c) {
 const double b=c.closure_band_m;double phase=double(mix(c.seed)&0xffffff)* (2*pi/16777216.);
 // Smooth, bounded physical wavelengths. It only removes original land inside
 // the admitted edge band and is independent of camera, jobs and UI RNG.
 auto n=[&](double t,double offset){return b*(.36+.16*std::sin(t/(b*1.7)+phase+offset)+.09*std::sin(t/(b*.61)+phase*1.3+offset));};
 return std::min({p.x+c.width_m*.5-n(p.y,0),c.width_m*.5-p.x-n(p.y,1.7),p.y+c.height_m*.5-n(p.x,3.4),c.height_m*.5-p.y-n(p.x,5.1)});
}
bool segment_inside_closure(Point a,Point b,const WorldConfig& c,int depth=0) {
 double sa=closure_signed(a,c),sb=closure_signed(b,c),length=std::hypot(b.x-a.x,b.y-a.y);
 if(sa<=0||sb<=0)return false;
 // The closure function has gradient norm <= 1.04 for its fixed frequencies.
 // This interval bound proves positivity between checked points, rather than
 // guessing connectivity from coarse terrain cells or vertex-only samples.
 if(std::min(sa,sb)>1.04*length*.5)return true;
 require(depth<40,"Ambiguous tangent lake/closure topology");
 Point m{(a.x+b.x)*.5,(a.y+b.y)*.5};
 return segment_inside_closure(a,m,c,depth+1)&&segment_inside_closure(m,b,c,depth+1);
}
}
std::string sha256_file(const std::filesystem::path& path) {
 require(std::filesystem::is_regular_file(path)&&!std::filesystem::is_symlink(path),"Invalid SHA-256 admission file");
 return file_hash(path);
}
Projection::Projection(LonLat o):origin_{longitude(o.longitude),o.latitude},phi_(o.latitude*rad),lambda_(longitude(o.longitude)*rad) {require(finite(o.longitude)&&finite(o.latitude)&&o.latitude>=-90&&o.latitude<=90,"Invalid geographic origin");}
Point Projection::forward(LonLat ll) const {
 require(finite(ll.longitude)&&finite(ll.latitude)&&ll.latitude>=-90&&ll.latitude<=90,"Invalid geographic coordinate");
 double p=ll.latitude*rad,d=std::remainder(ll.longitude*rad-lambda_,2*pi),cc=std::clamp(std::sin(phi_)*std::sin(p)+std::cos(phi_)*std::cos(p)*std::cos(d),-1.,1.),c=std::acos(cc);
 require(c<pi-1e-7,"AEQD antipodal singularity");double k=c<1e-9?1.:c/std::sin(c);
 return {radius*k*std::cos(p)*std::sin(d),radius*k*(std::cos(phi_)*std::sin(p)-std::sin(phi_)*std::cos(p)*std::cos(d))};
}
LonLat Projection::inverse(Point p) const {
 require(finite(p.x)&&finite(p.y),"Invalid projected coordinate");double rho=std::hypot(p.x,p.y),c=rho/radius;require(c<pi-1e-7,"AEQD inverse outside valid disk");if(rho<1e-8)return origin_;
 double lat=std::asin(std::clamp(std::cos(c)*std::sin(phi_)+p.y*std::sin(c)*std::cos(phi_)/rho,-1.,1.));
 double lon=lambda_+std::atan2(p.x*std::sin(c),rho*std::cos(phi_)*std::cos(c)-p.y*std::sin(phi_)*std::sin(c));return {longitude(lon/rad),lat/rad};
}
std::shared_ptr<GeoAtlas> GeoAtlas::load(const std::filesystem::path& dir) {
 auto atlas=std::make_shared<GeoAtlas>();
 atlas->full_=read_rings(dir/"gshhs_f.b",95809336,"af9215d58ebc525b2d09654a89959829f09e6edc457f3666759cded37be4ecf6");
 atlas->high_=read_rings(dir/"gshhs_h.b",22540096,"380f585415006f2b648a1a99383f50bf06518728d62991d98d48e7c2de48ec8f");
 atlas->intermediate_=read_rings(dir/"gshhs_i.b",5516184,"7d44bf4e16efe6056ff9d147ac0af189bbf0c16a31608426ef63ff8a91c418db");
 atlas->low_=read_rings(dir/"gshhs_l.b",1212340,"7aaa356af4e8d06438319a5c756545325b40fd9080871216d52d377d8a7f6b15");
 atlas->coarse_=read_rings(dir/"gshhs_c.b",182724,"dd0e9e2f8f121b6ae6a421afd1259ab369c2bac7f741a430db5438820c35aee9");
 atlas->source_hash_=archive_hash;atlas->latitude_edges_.resize(180);
 for(std::size_t id=0;id<atlas->full_.size();++id){const auto&r=atlas->full_[id];for(std::size_t j=0;j<r.vertices.size();++j){auto a=r.vertices[j],b=r.vertices[(j+1)%r.vertices.size()];if(a.latitude==b.latitude)continue;int lo=std::clamp(int(std::floor(std::min(a.latitude,b.latitude)+90)),0,179),hi=std::clamp(int(std::floor(std::max(a.latitude,b.latitude)+90)),0,179);for(int k=lo;k<=hi;++k)atlas->latitude_edges_[std::size_t(k)].push_back({a.longitude,a.latitude,b.longitude,b.latitude,std::int32_t(id),r.level});}}
 return atlas;
}
const std::vector<Ring>& GeoAtlas::rings(Detail detail) const {switch(detail){case Detail::Full:return full_;case Detail::High:return high_;case Detail::Intermediate:return intermediate_;case Detail::Low:return low_;case Detail::Coarse:return coarse_;}return full_;}
bool GeoAtlas::land(LonLat p) const {
 require(finite(p.longitude)&&finite(p.latitude)&&p.latitude>=-90&&p.latitude<=90,"Invalid land query");p.longitude=longitude(p.longitude);if(p.latitude<=-89.999999)return true;if(p.latitude>=89.999999)return false;
 std::map<std::int32_t,bool>parity;const auto& edges=latitude_edges_[std::size_t(std::clamp(int(std::floor(p.latitude+90)),0,179))];
 for(const auto&e:edges){if((e.y1>p.latitude)==(e.y2>p.latitude))continue;const auto&r=full_[std::size_t(e.ring)];double x=p.longitude+360.*std::round(((r.west+r.east)*.5-p.longitude)/360.);if(x<r.west||x>r.east)continue;double cross=e.x1+(p.latitude-e.y1)*(e.x2-e.x1)/(e.y2-e.y1);if(x<cross)parity[e.ring]=!parity[e.ring];}
 int level=0;for(const auto&[i,on]:parity)if(on)level=std::max(level,int(full_[std::size_t(i)].level));return level%2!=0;
}
std::shared_ptr<const EarthWorldDefinition> EarthWorldDefinition::create(std::shared_ptr<const GeoAtlas> atlas,const WorldConfig& config,const std::function<bool()>& cancelled) {require(bool(atlas),"Missing geographic atlas");return std::shared_ptr<const EarthWorldDefinition>(new EarthWorldDefinition(std::move(atlas),config,cancelled));}
EarthWorldDefinition::EarthWorldDefinition(std::shared_ptr<const GeoAtlas> atlas,WorldConfig c,const std::function<bool()>& cancelled):atlas_(std::move(atlas)),config_(c),projection_(c.centre) {
 for(double v:{c.width_m,c.height_m,c.world_scale,c.closure_band_m,c.sea_collar_m,c.sea_guard_m,c.coast_width_m,c.land_height_m,c.water_depth_m})require(finite(v)&&v>0,"Invalid world dimensions/profile");
 require(c.world_scale>=.001&&c.world_scale<=100&&c.width_m>=64&&c.height_m>=64&&std::max(c.width_m,c.height_m)/c.world_scale<=4000000,"World scale/projection budget exceeded");
 require(c.closure_band_m>=c.coast_width_m*2&&c.closure_band_m<=std::min(c.width_m,c.height_m)*.2,"Closure band erases too much interior");
 require(c.sea_guard_m>=c.coast_width_m&&c.sea_collar_m>c.sea_guard_m*2,"Invalid sea collar/guard");
 require(c.grid_cells_x>=8&&c.grid_cells_y>=8&&c.grid_cells_x<=512&&c.grid_cells_y<=512,"World grid budget exceeded");
 selection_={-c.width_m*.5,-c.height_m*.5,c.width_m*.5,c.height_m*.5};domain_={selection_.min_x-c.sea_collar_m,selection_.min_y-c.sea_collar_m,selection_.max_x+c.sea_collar_m,selection_.max_y+c.sea_collar_m};
 dx_=c.width_m/c.grid_cells_x;dy_=c.height_m/c.grid_cells_y;
 columns_=std::uint32_t(std::ceil((domain_.max_x-domain_.min_x)/dx_));rows_=std::uint32_t(std::ceil((domain_.max_y-domain_.min_y)/dy_));require(std::uint64_t(columns_)*rows_<=1048576,"World sea collar cell budget exceeded");
 dx_=(domain_.max_x-domain_.min_x)/columns_;dy_=(domain_.max_y-domain_.min_y)/rows_;
 const double pad=c.coast_width_m*2+2;Bounds expanded{domain_.min_x-pad,domain_.min_y-pad,domain_.max_x+pad,domain_.max_y+pad};
 source_reference_={expanded.min_x+.017,expanded.min_y+.031};source_reference_land_=atlas_->land(geographic(source_reference_));
 // Only local full-source boundary segments enter the sampler. Subdivide long
 // source edges before projection: at most 8 game metres per projected segment.
 // No camera/LOD-driven changes to this canonical geometry.
 const double geographic_reach=std::hypot(expanded.max_x,expanded.max_y)/c.world_scale/radius/rad+2.;
 const double geographic_longitude_reach=std::abs(c.centre.latitude)+geographic_reach>=90.?180.:std::asin(std::clamp(std::sin(geographic_reach*rad)/std::cos(c.centre.latitude*rad),0.,1.))/rad;
 std::size_t counter=0;
 for(const auto&r:atlas_->rings(Detail::Full)) {
  if((++counter&255)==0)cancel(cancelled);
  if(r.north<c.centre.latitude-geographic_reach||r.south>c.centre.latitude+geographic_reach)continue;
  double centre_x=c.centre.longitude+360.*std::round(((r.west+r.east)*.5-c.centre.longitude)/360.);
  double lon_reach=geographic_longitude_reach;
  if(centre_x+lon_reach<r.west||centre_x-lon_reach>r.east)continue;
  for(std::size_t j=0;j<r.vertices.size();++j) {
   if((j&4095)==0)cancel(cancelled);auto a=r.vertices[j],b=r.vertices[(j+1)%r.vertices.size()];
   if(std::max(a.latitude,b.latitude)<c.centre.latitude-geographic_reach||std::min(a.latitude,b.latitude)>c.centre.latitude+geographic_reach)continue;
   b.longitude=a.longitude+std::remainder(b.longitude-a.longitude,360.);
   double midpoint=(a.longitude+b.longitude)*.5,origin=c.centre.longitude+360.*std::round((midpoint-c.centre.longitude)/360.);
   if(std::max(a.longitude,b.longitude)<origin-lon_reach||std::min(a.longitude,b.longitude)>origin+lon_reach)continue;
   // Polar closing edges are representation-only, not physical coast.
   if(a.latitude<=-89.999||b.latitude<=-89.999)continue;
   auto project=[&](LonLat ll){auto p=projection_.forward(ll);return Point{p.x*c.world_scale,p.y*c.world_scale};};
   Point p=project(a),q=project(b);if(std::max(p.x,q.x)<expanded.min_x||std::min(p.x,q.x)>expanded.max_x||std::max(p.y,q.y)<expanded.min_y||std::min(p.y,q.y)>expanded.max_y)continue;
   double len=std::hypot(q.x-p.x,q.y-p.y);int count=std::max(1,int(std::ceil(len/8.)));require(count<=1000000,"Geographic edge tessellation exceeded");
   for(int k=1;k<=count;++k){if((k&4095)==0)cancel(cancelled);double t=double(k)/count;Point end=project({a.longitude+(b.longitude-a.longitude)*t,a.latitude+(b.latitude-a.latitude)*t});if(std::max(p.x,end.x)>=expanded.min_x&&std::min(p.x,end.x)<=expanded.max_x&&std::max(p.y,end.y)>=expanded.min_y&&std::min(p.y,end.y)<=expanded.max_y)shore_segments_.push_back({p,end});require(shore_segments_.size()<=500000,"Canonical shoreline segment budget exceeded");p=end;}
  }
 }
 cancel(cancelled);
 // Closed source lake polygons remain separate only when the entire canonical
 // boundary is proven inside the derived selection coast. Any cut opens them
 // into the exterior ocean. Level-4 ponds get their own component even inside
 // an already-open level-2 lake; no stale source lake IDs are carried through.
 water_regions_=1;
 for(const auto&r:atlas_->rings(Detail::Full)) {
  if(r.level!=2&&r.level!=4)continue;
  if(r.north<c.centre.latitude-geographic_reach||r.south>c.centre.latitude+geographic_reach)continue;
  double cx=c.centre.longitude+360.*std::round(((r.west+r.east)*.5-c.centre.longitude)/360.);
  double lr=geographic_longitude_reach;
  if(cx+lr<r.west||cx-lr>r.east)continue;
  WaterRegion water;water.level=r.level;bool closed=true;
  auto project=[&](LonLat ll){auto p=projection_.forward(ll);return Point{p.x*c.world_scale,p.y*c.world_scale};};
  for(std::size_t j=0;j<r.vertices.size()&&closed;++j) {
   if((j&4095)==0)cancel(cancelled);
   auto a=r.vertices[j],b=r.vertices[(j+1)%r.vertices.size()];b.longitude=a.longitude+std::remainder(b.longitude-a.longitude,360.);
   // Quickly reject far vertices before a projection can encounter antipodes.
   if(std::abs(a.latitude-c.centre.latitude)>geographic_reach||std::abs(std::remainder(a.longitude-c.centre.longitude,360.))>lr){closed=false;break;}
   Point p=project(a),q=project(b);
   if(closure_signed(p,c)<=0||closure_signed(q,c)<=0){closed=false;break;}
   int count=std::max(1,int(std::ceil(std::hypot(q.x-p.x,q.y-p.y)/8.)));
   for(int k=1;k<=count&&closed;++k){if((k&4095)==0)cancel(cancelled);double t=double(k)/count;Point end=project({a.longitude+(b.longitude-a.longitude)*t,a.latitude+(b.latitude-a.latitude)*t});if(!segment_inside_closure(p,end,c))closed=false;else water.edges.push_back({p,end});require(water.edges.size()<=500000,"Canonical lake segment budget exceeded");p=end;}
  }
  if(closed&&!water.edges.empty()){water.id=++water_regions_;closed_water_.push_back(std::move(water));}
 }
 cancel(cancelled);
 // The reference-to-query parity includes clipped boundaries inside this convex
 // expanded rectangle. Every selected query stays inside it, so discarded far
 // edges cannot cross that segment.
 segment_bins_.resize(64*64);
 for(std::uint32_t i=0;i<shore_segments_.size();++i){const auto&s=shore_segments_[i];int x0=std::clamp(int((std::min(s.a.x,s.b.x)-c.coast_width_m-domain_.min_x)/(domain_.max_x-domain_.min_x)*64),0,63),x1=std::clamp(int((std::max(s.a.x,s.b.x)+c.coast_width_m-domain_.min_x)/(domain_.max_x-domain_.min_x)*64),0,63),y0=std::clamp(int((std::min(s.a.y,s.b.y)-c.coast_width_m-domain_.min_y)/(domain_.max_y-domain_.min_y)*64),0,63),y1=std::clamp(int((std::max(s.a.y,s.b.y)+c.coast_width_m-domain_.min_y)/(domain_.max_y-domain_.min_y)*64),0,63);for(int y=y0;y<=y1;++y)for(int x=x0;x<=x1;++x)segment_bins_[std::size_t(y*64+x)].push_back(i);}
 cells_.resize(std::size_t(columns_)*rows_);
 for(std::uint32_t y=0;y<rows_;++y){cancel(cancelled);for(std::uint32_t x=0;x<columns_;++x){auto s=sample({domain_.min_x+(x+.5)*dx_,domain_.min_y+(y+.5)*dy_});cells_[std::size_t(y)*columns_+x]={s.source_land,s.land,s.guard,s.height_m,s.water_region};}}
 // Component labels come from the admitted continuous topology above. Grid
 // cells are bookkeeping samples; they never decide whether a narrow strait or
 // island exists, nor whether a source lake was opened by the selection cut.
 Sha256 hash;std::ostringstream recipe;recipe.exceptions(std::ios::badbit|std::ios::failbit);recipe.imbue(std::locale::classic());recipe<<"earth-v1|"<<archive_hash<<std::setprecision(17)<<'|'<<c.centre.longitude<<'|'<<c.centre.latitude<<'|'<<c.width_m<<'|'<<c.height_m<<'|'<<c.world_scale<<'|'<<c.closure_band_m<<'|'<<c.sea_collar_m<<'|'<<c.sea_guard_m<<'|'<<c.coast_width_m<<'|'<<c.land_height_m<<'|'<<c.water_depth_m<<'|'<<c.grid_cells_x<<'|'<<c.grid_cells_y<<'|'<<c.seed;
 auto text=recipe.str();hash.add(text.data(),text.size());recipe_hash_=hash.finish();
}
LonLat EarthWorldDefinition::geographic(Point p) const {return projection_.inverse({p.x/config_.world_scale,p.y/config_.world_scale});}
const std::string& EarthWorldDefinition::source_hash() const {return atlas_->source_hash();}
bool EarthWorldDefinition::source_land_at(Point p) const {
 bool land=source_reference_land_;Point d{p.x-source_reference_.x,p.y-source_reference_.y};
 // Rotate the query ray to +x. Half-open crossing handles shared segment vertices.
 for(const auto&s:shore_segments_){double ax=s.a.x-source_reference_.x,ay=s.a.y-source_reference_.y,bx=s.b.x-source_reference_.x,by=s.b.y-source_reference_.y;double a_cross=ax*d.y-ay*d.x,b_cross=bx*d.y-by*d.x;if((a_cross>0)==(b_cross>0))continue;double t=a_cross/(a_cross-b_cross),x=ax+t*(bx-ax),y=ay+t*(by-ay);double dot=x*d.x+y*d.y;if(dot>0&&dot<d.x*d.x+d.y*d.y)land=!land;}
 return land;
}
bool EarthWorldDefinition::closed_land(Point p,bool source) const {if(!source||!inside(selection_,p))return false;return closure_signed(p,config_)>0;}
double EarthWorldDefinition::coast_distance(Point p) const {
 double distance=config_.coast_width_m;int x=std::clamp(int((p.x-domain_.min_x)/(domain_.max_x-domain_.min_x)*64),0,63),y=std::clamp(int((p.y-domain_.min_y)/(domain_.max_y-domain_.min_y)*64),0,63);
 for(auto i:segment_bins_[std::size_t(y*64+x)]){const auto&s=shore_segments_[i];distance=std::min(distance,segment_distance(p,s.a,s.b));}
 distance=std::min(distance,std::abs(closure_signed(p,config_)));
 return distance;
}
double EarthWorldDefinition::height_at(Point p,bool land) const {
 return (land?config_.land_height_m:-config_.water_depth_m)*smooth(coast_distance(p)/config_.coast_width_m);
}
SurfaceSample EarthWorldDefinition::sample(Point p) const {
 require(finite(p.x)&&finite(p.y),"Invalid world surface query");SurfaceSample out;
 if(!inside(domain_,p)){out.height_m=-config_.water_depth_m;return out;}
 out.authoritative=true;out.guard=std::min({p.x-domain_.min_x,domain_.max_x-p.x,p.y-domain_.min_y,domain_.max_y-p.y})<config_.sea_guard_m;
 out.source_land=inside(selection_,p)&&source_land_at(p);out.land=closed_land(p,out.source_land);out.height_m=height_at(p,out.land);
 out.coast_distance_m=coast_distance(p);
 if(!out.land){out.water_region=1;int deepest=0;for(const auto&water:closed_water_){bool contained=false;for(const auto&e:water.edges){if((e.a.y>p.y)==(e.b.y>p.y))continue;double crossing=e.a.x+(p.y-e.a.y)*(e.b.x-e.a.x)/(e.b.y-e.a.y);if(p.x<crossing)contained=!contained;}if(contained&&water.level>deepest){deepest=water.level;out.water_region=water.id;}}}

 return out;
}
}
