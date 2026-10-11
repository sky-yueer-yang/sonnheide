#include "sonnheide.hpp"
#include <algorithm>
#include <bit>
#include <climits>
#include <cstdlib>
#include <limits>
#include <set>
#include <tuple>
namespace sonn {
namespace {
// Exact portable unsigned 128-bit products and long division; works with MSVC as well as Clang.
struct Wide {uint64_t hi=0,lo=0;};
Wide multiply(uint64_t a,uint64_t b){uint64_t a0=uint32_t(a),a1=a>>32,b0=uint32_t(b),b1=b>>32;uint64_t p00=a0*b0,p01=a0*b1,p10=a1*b0,p11=a1*b1;uint64_t middle=(p00>>32)+uint32_t(p01)+uint32_t(p10);return {p11+(p01>>32)+(p10>>32)+(middle>>32),(middle<<32)|uint32_t(p00)};}
int compare(Wide a,Wide b){if(a.hi!=b.hi)return a.hi<b.hi?-1:1;if(a.lo==b.lo)return 0;return a.lo<b.lo?-1:1;}
std::pair<uint64_t,uint64_t> divide(Wide a,uint64_t d){if(!d||d>uint64_t(INT64_MAX))throw Error("GEO_OVERFLOW","division");uint64_t q=0,r=0;for(int i=127;i>=0;i--){uint64_t bit=i>=64?(a.hi>>(i-64))&1:(a.lo>>i)&1;r=(r<<1)|bit;if(r>=d){r-=d;if(i>=64)throw Error("GEO_OVERFLOW","quotient");q|=uint64_t(1)<<i;}}return {q,r};}
struct Fraction {int64_t whole;uint64_t rem,den;};
bool less(Fraction a,Fraction b){if(a.whole!=b.whole)return a.whole<b.whole;return compare(multiply(a.rem,b.den),multiply(b.rem,a.den))<0;}
bool equal(Fraction a,Fraction b){return !less(a,b)&&!less(b,a);}
Fraction scaled(int64_t n,uint64_t d){int64_t q=n/int64_t(d),r=n%int64_t(d);if(r<0){--q;r+=int64_t(d);}return {q,uint64_t(r),d};}
struct Point {int64_t x,y;};
struct Ring {int32_t id,level,container;int64_t west,east,south,north;std::vector<Point> points;};
Fraction crossing(Point a,Point b,int64_t py,uint64_t yd){if(a.y>b.y)std::swap(a,b);uint64_t den=uint64_t(b.y-a.y)*yd;uint64_t off=uint64_t(py-a.y*int64_t(yd));int64_t dx=b.x-a.x;auto [q,r]=divide(multiply(uint64_t(dx<0?-dx:dx),off),den);if(dx>=0)return {a.x+int64_t(q),r,den};return {a.x-int64_t(q)-(r?1:0),r?den-r:0,den};}
int32_t be32(std::span<const uint8_t>b,size_t p){uint32_t v=(uint32_t(b[p])<<24)|(uint32_t(b[p+1])<<16)|(uint32_t(b[p+2])<<8)|b[p+3];return std::bit_cast<int32_t>(v);}
int64_t unwrap_delta(int64_t d){while(d< -180000000)d+=360000000;while(d>=180000000)d-=360000000;return d;}
}
struct Geography::Impl {
 Hash hash;std::vector<Ring> rings;
 std::vector<uint8_t> raster(const GeoRect&r,int32_t w,int32_t h,int64_t xstep,int64_t ystep,uint64_t xd,uint64_t yd,const std::function<bool()>&cancel)const{
  if(w<1||h<1||w>4096||h>4096||uint64_t(w)*uint64_t(h)>16777216)throw Error("GEO_GRID_LIMIT","navigation grid");if(r.east_unwrapped_udeg<=r.west_udeg||r.north_udeg<=r.south_udeg||int64_t(r.east_unwrapped_udeg)-r.west_udeg>360000000||r.south_udeg< -90000000||r.north_udeg>90000000)throw Error("GEO_RECT","range");
  size_t n=size_t(w)*size_t(h);std::vector<uint8_t>out(n,0),depth(n,0);std::vector<int32_t>ids(n,INT32_MAX);std::vector<Fraction> xs;xs.reserve(size_t(w));for(int32_t x=0;x<w;x++)xs.push_back(scaled(int64_t(r.west_udeg)*int64_t(xd)+(2LL*x+1)*xstep,xd));
  std::vector<int64_t> ys;ys.reserve(size_t(h));for(int32_t z=0;z<h;z++)ys.push_back(int64_t(r.south_udeg)*int64_t(yd)+(2LL*z+1)*ystep);
  auto paint=[&](int32_t row,Fraction left,Fraction right,int32_t level,int32_t id){if(less(right,left))std::swap(left,right);auto first=std::lower_bound(xs.begin(),xs.end(),left,less),last=std::upper_bound(xs.begin(),xs.end(),right,[](auto value,auto element){return less(value,element);});for(auto i=first;i!=last;++i){size_t idx=size_t(row)*size_t(w)+size_t(i-xs.begin());if(level>depth[idx]||(level==depth[idx]&&id<ids[idx])){depth[idx]=uint8_t(level);ids[idx]=id;out[idx]=uint8_t(level%2);}}};
  for(auto& ring:rings){if(cancel&&cancel())throw Error("CREATION_CANCELED","geography");if(ring.north<r.south_udeg||ring.south>r.north_udeg)continue;auto yfirst=std::lower_bound(ys.begin(),ys.end(),ring.south*int64_t(yd)),ylast=std::upper_bound(ys.begin(),ys.end(),ring.north*int64_t(yd));if(yfirst==ylast)continue;int32_t row_start=int32_t(yfirst-ys.begin()),row_end=int32_t(ylast-ys.begin());
   for(int shift=-2;shift<=2;shift++){int64_t offset=int64_t(shift)*360000000;if(ring.east+offset<r.west_udeg||ring.west+offset>r.east_unwrapped_udeg)continue;
    std::vector<std::vector<Fraction>> crossings(static_cast<size_t>(row_end-row_start)),boundaries(static_cast<size_t>(row_end-row_start));
    for(size_t k=1;k<ring.points.size();k++){Point a=ring.points[k-1],b=ring.points[k];a.x+=offset;b.x+=offset;int64_t ymin=std::min(a.y,b.y),ymax=std::max(a.y,b.y); // bounded row range; no traversal of every edge for every cell
     int64_t first=((ymin-r.south_udeg)*int64_t(yd)-ystep)/(2*ystep),last=((ymax-r.south_udeg)*int64_t(yd)-ystep)/(2*ystep);first=std::max<int64_t>(row_start,first-1);last=std::min<int64_t>(row_end-1,last+1);
     for(int64_t j=first;j<=last;j++){int64_t py=int64_t(r.south_udeg)*int64_t(yd)+(2*j+1)*ystep;if(py<ymin*int64_t(yd)||py>ymax*int64_t(yd))continue;if(a.y==b.y){if(py==a.y*int64_t(yd))paint(int32_t(j),{a.x,0,1},{b.x,0,1},ring.level,ring.id);continue;}auto x=crossing(a,b,py,yd);boundaries[size_t(j-row_start)].push_back(x);if(py<ymax*int64_t(yd))crossings[size_t(j-row_start)].push_back(x);}
    }
    for(int32_t j=row_start;j<row_end;j++){auto& v=crossings[size_t(j-row_start)];std::sort(v.begin(),v.end(),less);if(v.size()%2)throw Error("GEO_RING_INVALID","odd row crossings");for(size_t k=0;k<v.size();k+=2)paint(j,v[k],v[k+1],ring.level,ring.id);for(auto b:boundaries[size_t(j-row_start)])paint(j,b,b,ring.level,ring.id);}
   }
  }
  return out;
 }
};
Geography::Geography(const std::filesystem::path&path,const Hash&expected){auto bytes=read_file(path,134217728);if(sha256(bytes)!=expected)throw Error("GEO_SOURCE_HASH",path.string());auto p=std::make_shared<Impl>();p->hash=expected;size_t pos=0;std::map<int32_t,std::pair<int32_t,int32_t>>hierarchy;
 while(pos<bytes.size()){if(bytes.size()-pos<44)throw Error("GEO_SOURCE_FORMAT","truncated header");auto id=be32(bytes,pos),count=be32(bytes,pos+4);uint32_t flag=uint32_t(be32(bytes,pos+8));int32_t level=int32_t(flag&255),container=be32(bytes,pos+36);bool declared_south_pole=be32(bytes,pos+12)==-180000000&&be32(bytes,pos+16)==180000000&&be32(bytes,pos+20)==-90000000;if(id<0||count<4||count>2000000||level<1||level>6||((flag>>8)&255)!=15)throw Error("GEO_SOURCE_FORMAT","GSHHG2.3.7 header");if(!hierarchy.emplace(id,std::pair(level==5?1:level,container)).second)throw Error("GEO_SOURCE_FORMAT","duplicate polygon");pos+=44;if(uint64_t(count)*8>bytes.size()-pos)throw Error("GEO_SOURCE_FORMAT","truncated points");Ring ring{id,level==5?1:level,container,INT64_MAX,INT64_MIN,INT64_MAX,INT64_MIN,{}};ring.points.reserve(size_t(count));
 for(int32_t i=0;i<count;i++){int64_t x=be32(bytes,pos),y=be32(bytes,pos+4);pos+=8;if(x< -180000000||x>360000000||y< -90000000||y>90000000)throw Error("GEO_SOURCE_FORMAT","coordinates");if(!ring.points.empty())x=ring.points.back().x+unwrap_delta(x-ring.points.back().x);ring.points.push_back({x,y});ring.west=std::min(ring.west,x);ring.east=std::max(ring.east,x);ring.south=std::min(ring.south,y);ring.north=std::max(ring.north,y);}
 if(level<=4&&(ring.points.front().x!=ring.points.back().x||ring.points.front().y!=ring.points.back().y))throw Error("GEO_RING_INVALID","not closed");if(level==6)continue; // Official choice: ice front level5 as land; grounding line level6 excluded.
 if(level==5){
  // Level5 denotes the entire ice-front dataset: ordinary closed islands AND the pole-winding mainland.
  // A level tag alone does not imply polar topology. Preserve closed islands exactly as source L1 land.
  auto a=ring.points.front(),b=ring.points.back();bool closed=a.x==b.x&&a.y==b.y;
  if(!closed){
   // Only the source-declared pole-containing loop has equivalent meridian endpoints one full turn apart.
   // Close its planar representation through the declared south pole; never extend ordinary islands to it.
   if(!declared_south_pole||a.y!=b.y||std::abs(a.x-b.x)!=360000000)throw Error("GEO_RING_INVALID","Antarctic winding");
   ring.points.push_back({b.x,-90000000});ring.points.push_back({a.x,-90000000});ring.points.push_back(a);ring.south=-90000000;
  }
 }
 p->rings.push_back(std::move(ring));
 }
 for(auto& ring:p->rings)if(ring.level>1){auto i=hierarchy.find(ring.container);if(i==hierarchy.end()||i->second.first!=ring.level-1)throw Error("GEO_NESTING","missing parent");}
 if(p->rings.empty())throw Error("GEO_SOURCE_FORMAT","empty");impl_=std::move(p);
}
const Hash& Geography::source_hash()const{return impl_->hash;}
size_t Geography::polygon_count()const{return impl_->rings.size();}
void Geography::populate_world(Terrain&t,const GeoRect&r,int32_t w,int32_t h,uint8_t theme,const std::function<bool()>&cancel,CreationMetrics*metrics)const{
 if(t.profile!=2||w<32||h<32||w>4096||h>4096||t.width!=w+16||t.height!=h+16||theme>7||r.west_udeg< -180000000||r.west_udeg>=180000000||r.south_udeg< -90000000||r.north_udeg>90000000||r.north_udeg<=r.south_udeg||r.east_unwrapped_udeg<=r.west_udeg||int64_t(r.east_unwrapped_udeg)-r.west_udeg>360000000)throw Error("GEO_RECT","micro source selection");
 // Compact references keep source points shared. Only edges touching a real
 // sample row survive. The512B binary patches are produced directly, rather
 // than temporarily expanding the entire microgrid or each patch palette.
 struct Edge{uint32_t ring,point;int32_t shift,first,last;};
 struct Cross{uint64_t group;Fraction x;bool parity;};
 struct Interval{int32_t first,last,level,id;};
 constexpr size_t edge_limit=4000000,active_limit=65536;
 const int32_t mw=w*64,mh=h*64;const int64_t span=int64_t(r.east_unwrapped_udeg)-r.west_udeg,den=2LL*mw;
 auto floor_div=[](int64_t n,int64_t d){auto q=n/d,rem=n%d;return q-(rem<0);};
 auto ceil_div=[&](int64_t n,int64_t d){return -floor_div(-n,d);};
 std::vector<Edge>edges;edges.reserve(4096);
 for(uint32_t ri=0;ri<impl_->rings.size();ri++){if(cancel&&cancel())throw Error("CREATION_CANCELED","source edges");auto&ring=impl_->rings[ri];if(ring.north<r.south_udeg||ring.south>r.north_udeg)continue;for(int sh=-2;sh<=2;sh++){int64_t off=int64_t(sh)*360000000;if(ring.east+off<r.west_udeg||ring.west+off>r.east_unwrapped_udeg)continue;for(uint32_t k=1;k<ring.points.size();k++){if((k&4095u)==0&&cancel&&cancel())throw Error("CREATION_CANCELED","source edge batch");auto a=ring.points[k-1],b=ring.points[k];int64_t ymin=std::min(a.y,b.y),ymax=std::max(a.y,b.y);auto first=ceil_div((ymin-r.south_udeg)*den-span,2*span),last=floor_div((ymax-r.south_udeg)*den-span,2*span);first=std::max<int64_t>(first,0);last=std::min<int64_t>(last,mh-1);if(first>last)continue;if(edges.size()==edge_limit)throw Error("GEO_WORK_BUDGET","4M scanline edge references");edges.push_back({ri,k,sh,int32_t(first),int32_t(last)});}}}
 std::sort(edges.begin(),edges.end(),[](const Edge&a,const Edge&b){return std::tie(a.first,a.ring,a.shift,a.point)<std::tie(b.first,b.ring,b.shift,b.point);});
 std::vector<Fraction>xs;xs.reserve(size_t(mw));for(int32_t x=0;x<mw;x++)xs.push_back(scaled(int64_t(r.west_udeg)*den+(2LL*x+1)*span,uint64_t(den)));
 std::vector<uint32_t>active;active.reserve(1024);std::vector<Cross>crosses;std::vector<Interval>intervals;
 std::vector<uint64_t>mask(size_t(w)*64,0);uint64_t scratch_peak=0;auto scratch_check=[&]{uint64_t bytes=uint64_t(edges.capacity())*sizeof(Edge)+uint64_t(xs.capacity())*sizeof(Fraction)+uint64_t(active.capacity())*sizeof(uint32_t)+uint64_t(crosses.capacity())*sizeof(Cross)+uint64_t(intervals.capacity())*sizeof(Interval)+uint64_t(mask.capacity())*sizeof(uint64_t);scratch_peak=std::max(scratch_peak,bytes);if(bytes>134217728)throw Error("GEO_WORK_BUDGET","128MiB scanline scratch");};scratch_check();size_t next_edge=0;uint64_t work=0,geometry_estimate=sizeof(Terrain)+uint64_t(t.height)*sizeof(std::vector<CellRun>);
 for(int32_t row=0;row<mh;row++){
  if(cancel&&cancel())throw Error("CREATION_CANCELED","micro scanline");if(row%64==0)std::fill(mask.begin(),mask.end(),0);
  active.erase(std::remove_if(active.begin(),active.end(),[&](uint32_t i){return edges[i].last<row;}),active.end());while(next_edge<edges.size()&&edges[next_edge].first<=row){active.push_back(uint32_t(next_edge++));if(active.size()>active_limit)throw Error("GEO_WORK_BUDGET","65536 simultaneous edges");}
  if(work>300000000-active.size())throw Error("GEO_WORK_BUDGET","300M exact edge intersections");work+=active.size();crosses.clear();intervals.clear();
  const int64_t py=int64_t(r.south_udeg)*den+(2LL*row+1)*span;
  auto interval=[&](Fraction a,Fraction b,int level,int id){if(less(b,a))std::swap(a,b);auto lo=std::lower_bound(xs.begin(),xs.end(),a,less),hi=std::upper_bound(xs.begin(),xs.end(),b,[](auto value,auto element){return less(value,element);});if(lo!=hi)intervals.push_back({int32_t(lo-xs.begin()),int32_t(hi-xs.begin()),level,id});};
  if(py<=int64_t(r.north_udeg)*den)for(auto idx:active){auto&e=edges[idx];auto&ring=impl_->rings[e.ring];Point a=ring.points[e.point-1],b=ring.points[e.point];a.x+=int64_t(e.shift)*360000000;b.x+=int64_t(e.shift)*360000000;if(a.y==b.y){if(py==a.y*den)interval({a.x,0,1},{b.x,0,1},ring.level,ring.id);continue;}auto x=crossing(a,b,py,uint64_t(den));crosses.push_back({uint64_t(e.ring)*5+uint64_t(e.shift+2),x,py<std::max(a.y,b.y)*den});interval(x,x,ring.level,ring.id);}
  std::sort(crosses.begin(),crosses.end(),[](const Cross&a,const Cross&b){if(a.group!=b.group)return a.group<b.group;return less(a.x,b.x);});
  for(size_t i=0;i<crosses.size();){size_t end=i;while(end<crosses.size()&&crosses[end].group==crosses[i].group)++end;auto&ring=impl_->rings[size_t(crosses[i].group/5)];std::optional<Fraction>left;for(size_t j=i;j<end;j++)if(crosses[j].parity){if(!left)left=crosses[j].x;else{interval(*left,crosses[j].x,ring.level,ring.id);left.reset();}}if(left)throw Error("GEO_RING_INVALID","micro odd row crossings");i=end;}
  scratch_check();std::sort(intervals.begin(),intervals.end(),[](const Interval&a,const Interval&b){return a.level!=b.level?a.level<b.level:a.id>b.id;});
  for(auto&v:intervals){for(int32_t x=v.first;x<v.last;){int32_t block=x/64,bit=x%64,end=std::min(v.last,(block+1)*64),count=end-x;uint64_t bits=count==64?UINT64_MAX:((uint64_t(1)<<count)-1)<<bit;auto&word=mask[size_t(block)*64+size_t(row%64)];if(v.level%2)word|=bits;else word&=~bits;x=end;}}
  if(row%64==63){int32_t cz=row/64+8;auto&rr=t.rows[size_t(cz)];rr.clear();auto append=[&](int32_t end,Cell c){if(!rr.empty()&&rr.back().value==c)rr.back().end_x=end;else rr.push_back({end,c});};append(8,{TerrainKind::DeepOcean,0,1});for(int32_t x=0;x<w;x++){auto begin=mask.begin()+ptrdiff_t(x)*64;bool sea=std::all_of(begin,begin+64,[](uint64_t v){return v==0;}),land=std::all_of(begin,begin+64,[](uint64_t v){return v==UINT64_MAX;});Cell center{((mask[size_t(x)*64+32]>>32)&1)?TerrainKind::Soil:TerrainKind::CloseOcean,0,1};if(center.kind==TerrainKind::Soil)center.theme=theme;append(x+9,center);if(!sea&&!land){FinePatch p;p.palette={{TerrainKind::CloseOcean,0,1},{TerrainKind::Soil,theme,1}};p.index_bits=1;p.indices.reserve(512);for(int z=0;z<64;z++){auto word=mask[size_t(x)*64+z];for(unsigned b=0;b<8;b++)p.indices.push_back(uint8_t(word>>(b*8)));}p.compress_binary();geometry_estimate+=128+2*sizeof(Cell)+p.indices.capacity();t.fine_patches.emplace(uint64_t(cz)*t.width+uint64_t(x+8),std::move(p));}}append(t.width,{TerrainKind::DeepOcean,0,1});geometry_estimate+=uint64_t(rr.capacity())*sizeof(CellRun);if(geometry_estimate>t.storage_budget_bytes)throw Error("CREATION_BUDGET","exact micro coast storage: estimated="+std::to_string(geometry_estimate)+" resident="+std::to_string(t.storage_bytes())+" budget="+std::to_string(t.storage_budget_bytes)+" patches="+std::to_string(t.fine_patches.size())+" completed_coarse_row="+std::to_string(row/64+1)+" source="+std::to_string(r.west_udeg)+","+std::to_string(r.east_unwrapped_udeg)+","+std::to_string(r.south_udeg)+","+std::to_string(r.north_udeg));}
 }
 t.validate();if(metrics){metrics->scratch_peak_bytes=scratch_peak;metrics->geometry_bytes=t.storage_bytes();metrics->source_edge_references=edges.size();metrics->intersection_work=work;metrics->source_storage_bytes=sizeof(Impl)+uint64_t(impl_->rings.capacity())*sizeof(Ring);for(auto&ring:impl_->rings)metrics->source_storage_bytes+=uint64_t(ring.points.capacity())*sizeof(Point);}
}
std::vector<uint8_t> Geography::sample(const GeoRect&r,int32_t w,int32_t h)const{return impl_->raster(r,w,h,int64_t(r.east_unwrapped_udeg)-r.west_udeg,int64_t(r.north_udeg)-r.south_udeg,uint64_t(2)*uint64_t(w),uint64_t(2)*uint64_t(h),{});}
std::vector<uint8_t> Geography::sample_world_selection(const GeoRect&r,int32_t w,int32_t h,const std::function<bool()>&canceled)const{auto out=impl_->raster(r,w,h,int64_t(r.east_unwrapped_udeg)-r.west_udeg,int64_t(r.east_unwrapped_udeg)-r.west_udeg,uint64_t(2)*uint64_t(w),uint64_t(2)*uint64_t(w),canceled);for(int32_t z=0;z<h;z++)if(int64_t(r.south_udeg)*2*w+(2LL*z+1)*(int64_t(r.east_unwrapped_udeg)-r.west_udeg)>int64_t(r.north_udeg)*2*w)std::fill(out.begin()+ptrdiff_t(z)*w,out.begin()+ptrdiff_t(z+1)*w,0);return out;}
bool Geography::land(int64_t lon,int64_t lat)const{if(lat< -90000000||lat>90000000)throw Error("GEO_RECT","latitude");while(lon< -180000000)lon+=360000000;while(lon>=180000000)lon-=360000000;int32_t depth=0,id=INT32_MAX;for(auto& ring:impl_->rings){if(lat<ring.south||lat>ring.north)continue;for(int shift=-2;shift<=2;shift++){int64_t x=lon-int64_t(shift)*360000000;if(x<ring.west||x>ring.east)continue;bool inside=false,boundary=false;for(size_t k=1;k<ring.points.size();k++){auto a=ring.points[k-1],b=ring.points[k];if(lat<std::min(a.y,b.y)||lat>std::max(a.y,b.y))continue;if(a.y==b.y){if(lat==a.y&&x>=std::min(a.x,b.x)&&x<=std::max(a.x,b.x))boundary=true;continue;}auto c=crossing(a,b,lat,1);if(equal(c,{x,0,1}))boundary=true;if(lat<std::max(a.y,b.y)&&less({x,0,1},c))inside=!inside;}if((inside||boundary)&&(ring.level>depth||(ring.level==depth&&ring.id<id))){depth=ring.level;id=ring.id;}}}return depth%2!=0;}
} // sonn
