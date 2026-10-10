#include "sonnheide.hpp"
#include <algorithm>
#include <cmath>
#include <limits>
namespace sonn {
namespace {
void require_query_geometry(const Terrain& t){
 if((t.cell_mm!=terrain_cell_size_mm&&t.cell_mm!=legacy_terrain_cell_size_mm)||t.width<=0||t.height<=0||t.width>maximum_core_cells+2*terrain_guard_cells||t.height>maximum_core_cells+2*terrain_guard_cells||t.cells.size()!=size_t(t.width)*size_t(t.height))throw Error("TERRAIN_PROFILE","query requires admitted bounded geometry");
}
}
std::string_view terrain_name(TerrainKind k){constexpr std::array<std::string_view,8>n{"deep_ocean","close_ocean","shallow_water","sand","soil","hill","mountain","high_peak"};auto i=static_cast<size_t>(k);if(i>=n.size())throw Error("TERRAIN_KIND","index");return n[i];}
TerrainKind parse_terrain(std::string_view s){for(unsigned i=0;i<8;i++)if(terrain_name(TerrainKind(i))==s)return TerrainKind(i);throw Error("TERRAIN_KIND",std::string(s));}
const Cell& Terrain::at(int32_t x,int32_t z)const{if(x<0||z<0||x>=width||z>=height)throw Error("OUTSIDE_WORLD","cell");return cells.at(size_t(z)*size_t(width)+size_t(x));}
bool Terrain::editable(int32_t x,int32_t z)const{return x>=guard&&z>=guard&&x<width-guard&&z<height-guard;}
std::optional<SurfacePoint> Terrain::support(int64_t x,int64_t z)const{require_query_geometry(*this);if(x<0||z<0||x>=int64_t(width)*cell_mm||z>=int64_t(height)*cell_mm)return {};auto cx=int32_t(x/cell_mm),cz=int32_t(z/cell_mm);auto k=at(cx,cz).kind;auto h=terrain_height_mm[size_t(k)];return SurfacePoint{cx,cz,h,k,editable(cx,cz),h<0};}
void Terrain::validate()const{if(geometry_revision==0||geometry_revision>uint64_t(INT64_MAX)||guard!=terrain_guard_cells||(cell_mm!=terrain_cell_size_mm&&cell_mm!=legacy_terrain_cell_size_mm)||width<minimum_core_cells+2*guard||height<minimum_core_cells+2*guard||width>maximum_core_cells+2*guard||height>maximum_core_cells+2*guard||cells.size()!=size_t(width)*size_t(height))throw Error("TERRAIN_PROFILE","dimensions or scale");for(int32_t z=0;z<height;z++)for(int32_t x=0;x<width;x++){auto& c=at(x,z);if(size_t(c.kind)>=8||c.theme>7||c.revision==0||c.revision>uint64_t(INT64_MAX))throw Error("TERRAIN_PROFILE","cell");if(!editable(x,z)&&c.kind!=TerrainKind::DeepOcean)throw Error("TERRAIN_GUARD","guard must remain deep ocean");}}
Hash Terrain::surface_hash()const{Bytes b;auto put=[&](uint64_t v,unsigned n){for(unsigned i=0;i<n;i++)b.push_back(uint8_t(v>>(8*i)));};put(geometry_revision,8);put(uint32_t(width),4);put(uint32_t(height),4);put(uint32_t(cell_mm),4);put(uint32_t(guard),4);for(auto& c:cells){put(uint8_t(c.kind),1);put(c.theme,1);put(c.revision,8);}return sha256(b);}
Mesh Terrain::mesh(int32_t sx,int32_t sz,int32_t nx,int32_t nz)const{require_query_geometry(*this);if(sx<0||sz<0||sx>width||sz>height)throw Error("OUTSIDE_WORLD","mesh range");if(nx<0)nx=width-sx;if(nz<0)nz=height-sz;if(sx<0||sz<0||nx<0||nz<0||int64_t(sx)+nx>width||int64_t(sz)+nz>height)throw Error("OUTSIDE_WORLD","mesh range");const double cell_m=cell_mm/1000.0;Mesh m;auto quad=[&](std::array<Vec3,4> p,Vec3 n,uint8_t mat){if(m.vertices.size()>UINT32_MAX-4)throw Error("MESH_LIMIT","vertices");auto b=uint32_t(m.vertices.size());constexpr float uv[4][2]{{0,0},{0,1},{1,1},{1,0}};for(size_t i=0;i<4;i++)m.vertices.push_back({float(p[i].x),float(p[i].y),float(p[i].z),float(n.x),float(n.y),float(n.z),uv[i][0],uv[i][1],mat});m.indices.insert(m.indices.end(),{b,b+1,b+2,b,b+2,b+3});};
 auto h=[&](int32_t x,int32_t z){return x<0||z<0||x>=width||z>=height?-20.0:terrain_height_mm[size_t(at(x,z).kind)]/1000.0;};
 for(int32_t z=sz;z<sz+nz;z++)for(int32_t x=sx;x<sx+nx;x++){double x0=x*cell_m,x1=x0+cell_m,z0=z*cell_m,z1=z0+cell_m,y=h(x,z);auto mat=uint8_t(at(x,z).kind);quad({Vec3{x0,y,z0},Vec3{x0,y,z1},Vec3{x1,y,z1},Vec3{x1,y,z0}},{0,1,0},mat);double lo=h(x-1,z);if(y>lo)quad({Vec3{x0,y,z1},Vec3{x0,y,z0},Vec3{x0,lo,z0},Vec3{x0,lo,z1}},{-1,0,0},mat);lo=h(x+1,z);if(y>lo)quad({Vec3{x1,y,z0},Vec3{x1,y,z1},Vec3{x1,lo,z1},Vec3{x1,lo,z0}},{1,0,0},mat);lo=h(x,z-1);if(y>lo)quad({Vec3{x0,y,z0},Vec3{x1,y,z0},Vec3{x1,lo,z0},Vec3{x0,lo,z0}},{0,0,-1},mat);lo=h(x,z+1);if(y>lo)quad({Vec3{x1,y,z1},Vec3{x0,y,z1},Vec3{x0,lo,z1},Vec3{x1,lo,z1}},{0,0,1},mat);}
 return m;}
std::optional<RayHit> Terrain::raycast(Vec3 o,Vec3 d,double max)const{require_query_geometry(*this);if(!std::isfinite(o.x)||!std::isfinite(o.y)||!std::isfinite(o.z)||!std::isfinite(d.x)||!std::isfinite(d.y)||!std::isfinite(d.z)||!std::isfinite(max)||max<=0)throw Error("INVALID_RAY","finite coordinates");double scale=std::max({std::abs(d.x),std::abs(d.y),std::abs(d.z)});if(scale==0)throw Error("INVALID_RAY","zero direction");d.x/=scale;d.y/=scale;d.z/=scale;double len=std::hypot(d.x,d.y,d.z);d.x/=len;d.y/=len;d.z/=len;const double cell_m=cell_mm/1000.0;double begin=0,end=max;auto slab=[&](double p,double v,double size){if(std::abs(v)<1e-15)return p>=0&&p<size;double a=-p/v,b=(size-p)/v;if(a>b)std::swap(a,b);begin=std::max(begin,a);end=std::min(end,b);return begin<=end;};if(!slab(o.x,d.x,width*cell_m)||!slab(o.z,d.z,height*cell_m)||end<0)return {};begin=std::max(begin,0.0);double t=begin;int32_t x=std::clamp(int32_t(std::floor((o.x+d.x*(t+1e-8))/cell_m)),0,width-1),z=std::clamp(int32_t(std::floor((o.z+d.z*(t+1e-8))/cell_m)),0,height-1);
 auto h=[&](int32_t a,int32_t b){return a<0||b<0||a>=width||b>=height?-20.0:terrain_height_mm[size_t(at(a,b).kind)]/1000.0;};
 for(int count=0;count<width+height+4&&x>=0&&z>=0&&x<width&&z<height&&t<=end+1e-8;count++){
  double tx=std::abs(d.x)<1e-15?std::numeric_limits<double>::infinity():((d.x>0?(x+1)*cell_m:x*cell_m)-o.x)/d.x;
  double tz=std::abs(d.z)<1e-15?std::numeric_limits<double>::infinity():((d.z>0?(z+1)*cell_m:z*cell_m)-o.z)/d.z;double exit=std::min({tx,tz,end});double y=h(x,z),best=std::numeric_limits<double>::infinity();Vec3 normal;
  auto test=[&](double hit,Vec3 n,double lo,double hi,bool side){if(hit<t-1e-8||hit>exit+1e-8||hit<0||hit>max)return;Vec3 p{o.x+d.x*hit,o.y+d.y*hit,o.z+d.z*hit};if(side&&(p.y<lo-1e-8||p.y>hi+1e-8))return;if(p.x<x*cell_m-1e-8||p.x>(x+1)*cell_m+1e-8||p.z<z*cell_m-1e-8||p.z>(z+1)*cell_m+1e-8)return;if(hit<best){best=hit;normal=n;}};
  if(std::abs(d.y)>1e-15)test((y-o.y)/d.y,{0,1,0},0,0,false);
  if(std::abs(d.x)>1e-15){auto lo=h(x-1,z);if(y>lo)test((x*cell_m-o.x)/d.x,{-1,0,0},lo,y,true);lo=h(x+1,z);if(y>lo)test(((x+1)*cell_m-o.x)/d.x,{1,0,0},lo,y,true);}
  if(std::abs(d.z)>1e-15){auto lo=h(x,z-1);if(y>lo)test((z*cell_m-o.z)/d.z,{0,0,-1},lo,y,true);lo=h(x,z+1);if(y>lo)test(((z+1)*cell_m-o.z)/d.z,{0,0,1},lo,y,true);}
  if(std::isfinite(best))return RayHit{best,{o.x+d.x*best,o.y+d.y*best,o.z+d.z*best},normal,x,z,at(x,z).kind};if(exit>=end)break;t=exit;if(tx<=tz+1e-10)x+=d.x>0?1:-1;if(tz<=tx+1e-10)z+=d.z>0?1:-1;
 }
 return {};
}
} // sonn
