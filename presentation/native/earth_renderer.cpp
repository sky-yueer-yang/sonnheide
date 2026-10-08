#include "earth_renderer.hpp"
#include <earth_resource_lock.hpp>
#include <sonnheide/native_platform.hpp>
#include <bgfx/bgfx.h>
#include <bx/math.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <fstream>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>
namespace sonnheide::native {
namespace {
constexpr float pi=3.14159265358979323846f;
struct GroundVertex{float x,y,z,nx,ny,nz,u,v;};
struct ScreenVertex{float x,y,z,u,v;};
struct MapVertex{float x,y,z;std::uint32_t color;};
const std::array<const char*,8> ids={"brown_mud","grass_ground","sparse_grass","forest_leaves_02","dry_ground_01","sandy_gravel_02","sand_03","damp_beach_sand"};
const std::array<float,8> sizes={1.299996f,2.51f,2.f,3.000973f,4.f,2.53f,2.f,2.f};
const std::array<const char*,8> diffuseSuffix={"diff","diff","diff","diffuse","diff","diff","diff","diff"};
std::vector<std::uint8_t> bytes(const std::filesystem::path& path){std::ifstream f(path,std::ios::binary|std::ios::ate);if(!f)throw std::runtime_error("Missing Earth renderer resource: "+path.string());auto length=f.tellg();if(length<=0||length>1024LL*1024*1024)throw std::runtime_error("Invalid Earth resource length");std::vector<std::uint8_t> result(static_cast<std::size_t>(length));f.seekg(0);f.read(reinterpret_cast<char*>(result.data()),length);if(!f)throw std::runtime_error("Truncated Earth resource");return result;}
bgfx::ShaderHandle shader(const std::filesystem::path& path){auto content=bytes(path);auto h=bgfx::createShader(bgfx::copy(content.data(),std::uint32_t(content.size())));if(!bgfx::isValid(h))throw std::runtime_error("Invalid Earth shader "+path.string());return h;}
bgfx::ProgramHandle program(const std::filesystem::path& folder,const char* name){auto v=shader(folder/(std::string(name)+".vs.bin"));bgfx::ShaderHandle f=BGFX_INVALID_HANDLE;try{f=shader(folder/(std::string(name)+".fs.bin"));}catch(...){bgfx::destroy(v);throw;}auto p=bgfx::createProgram(v,f,true);if(!bgfx::isValid(p))throw std::runtime_error("Invalid Earth program");return p;}
// Generated mip chain explicitly respects the source channel transfer functions.
bgfx::TextureHandle groundTexture(const std::filesystem::path& path,int channel){auto image=decode_bimg_image(path);if(image.width!=2048||image.height!=2048)throw std::runtime_error("Ground texture does not match 2K lock");std::vector<std::uint8_t> all=image.rgba,current=std::move(image.rgba);int w=image.width,h=image.height;std::array<float,256> linear{};for(int i=0;i<256;++i){float x=i/255.f;linear[i]=x<=.04045f?x/12.92f:std::pow((x+.055f)/1.055f,2.4f);}while(w>1||h>1){int nw=std::max(1,w/2),nh=std::max(1,h/2);std::vector<std::uint8_t> next(std::size_t(nw)*nh*4);for(int y=0;y<nh;++y)for(int x=0;x<nw;++x){std::array<float,4> sum{};for(int dy=0;dy<2;++dy)for(int dx=0;dx<2;++dx){auto a=std::size_t(std::min(h-1,2*y+dy)*w+std::min(w-1,2*x+dx))*4;for(int c=0;c<4;++c)sum[c]+=channel==0&&c<3?linear[current[a+c]]:current[a+c]/255.f;}for(auto& value:sum)value*=.25f;if(channel==1){float nx=sum[0]*2-1,ny=sum[1]*2-1,nz=sum[2]*2-1;float length=std::sqrt(nx*nx+ny*ny+nz*nz);if(length>.001f){sum[0]=nx/length*.5f+.5f;sum[1]=ny/length*.5f+.5f;sum[2]=nz/length*.5f+.5f;}}for(int c=0;c<4;++c){float v=sum[c];if(channel==0&&c<3)v=v<=.0031308f?v*12.92f:1.055f*std::pow(v,1/2.4f)-.055f;next[(std::size_t(y)*nw+x)*4+c]=std::uint8_t(std::clamp(std::lround(v*255),0L,255L));}}all.insert(all.end(),next.begin(),next.end());current=std::move(next);w=nw;h=nh;}
return bgfx::createTexture2D(2048,2048,true,1,bgfx::TextureFormat::RGBA8,BGFX_SAMPLER_MIN_ANISOTROPIC|BGFX_SAMPLER_MAG_ANISOTROPIC|(channel==0?BGFX_TEXTURE_SRGB:0),bgfx::copy(all.data(),std::uint32_t(all.size())));}
struct Sky{bgfx::TextureHandle texture=BGFX_INVALID_HANDLE,prefilter=BGFX_INVALID_HANDLE;std::array<float,36> sh{};};
Sky skyTexture(const std::filesystem::path& path,bool specular=false){auto data=bytes(path);if(data.size()<128||std::memcmp(data.data(),specular?"SNIBL01\0":"SNHDR01\0",8)!=0)throw std::runtime_error("Invalid cooked HDR signature");std::uint32_t w,h,mips;std::memcpy(&w,data.data()+8,4);std::memcpy(&h,data.data()+12,4);std::memcpy(&mips,data.data()+16,4);if((!specular&&(w!=8192||h!=4096||mips!=14))||(specular&&(w!=512||h!=256||mips!=10)))throw std::runtime_error("Cooked HDR resolution mismatch");std::size_t expected=128;for(unsigned x=w,y=h;;x=std::max(1U,x/2),y=std::max(1U,y/2)){expected+=std::size_t(x)*y*8;if(x==1&&y==1)break;}if(data.size()!=expected)throw std::runtime_error("Incomplete HDR mip chain");Sky s;for(int i=0;i<9;++i)std::memcpy(s.sh.data()+i*4,data.data()+20+i*12,12);s.texture=bgfx::createTexture2D(std::uint16_t(w),std::uint16_t(h),true,1,bgfx::TextureFormat::RGBA16F,BGFX_SAMPLER_V_CLAMP,bgfx::copy(data.data()+128,std::uint32_t(data.size()-128)));if(!bgfx::isValid(s.texture))throw std::runtime_error("Cannot upload HDR sky");return s;}
}
struct EarthRenderer::Impl{
 std::filesystem::path root;std::shared_ptr<const earth::EarthWorldDefinition> world;
 bgfx::ProgramHandle ground=BGFX_INVALID_HANDLE,sky=BGFX_INVALID_HANDLE,mapProgram=BGFX_INVALID_HANDLE,lineProgram=BGFX_INVALID_HANDLE;
 bgfx::VertexLayout groundLayout,screenLayout,mapLayout;
 bgfx::UniformHandle base=BGFX_INVALID_HANDLE,normal=BGFX_INVALID_HANDLE,arm=BGFX_INVALID_HANDLE,skySampler=BGFX_INVALID_HANDLE,specularSampler=BGFX_INVALID_HANDLE,eye=BGFX_INVALID_HANDLE,profile=BGFX_INVALID_HANDLE,sh=BGFX_INVALID_HANDLE,right=BGFX_INVALID_HANDLE,up=BGFX_INVALID_HANDLE,forward=BGFX_INVALID_HANDLE;
 std::array<std::array<bgfx::TextureHandle,3>,8> materials{};std::array<Sky,2> skies;
 bgfx::VertexBufferHandle terrainVB=BGFX_INVALID_HANDLE;bgfx::IndexBufferHandle terrainIB=BGFX_INVALID_HANDLE;
 bgfx::TextureHandle atlasTexture=BGFX_INVALID_HANDLE;bgfx::VertexBufferHandle screenVB=BGFX_INVALID_HANDLE;bgfx::IndexBufferHandle screenIB=BGFX_INVALID_HANDLE;
 double cachedX=std::numeric_limits<double>::infinity(),cachedZ{},cachedSpan{};int cachedMaterial=-1;
 double atlasLon=1e20,atlasLat{},atlasSpan{};int atlasW{},atlasH{};std::string recipe;
 Impl(){for(auto& m:materials)for(auto& t:m)t=BGFX_INVALID_HANDLE;}
 ~Impl(){for(auto& m:materials)for(auto t:m)if(bgfx::isValid(t))bgfx::destroy(t);for(auto& s:skies){if(bgfx::isValid(s.texture))bgfx::destroy(s.texture);if(bgfx::isValid(s.prefilter))bgfx::destroy(s.prefilter);}for(auto h:{ground,sky,mapProgram,lineProgram})if(bgfx::isValid(h))bgfx::destroy(h);for(auto h:{base,normal,arm,skySampler,specularSampler,eye,profile,sh,right,up,forward})if(bgfx::isValid(h))bgfx::destroy(h);if(bgfx::isValid(terrainVB))bgfx::destroy(terrainVB);if(bgfx::isValid(terrainIB))bgfx::destroy(terrainIB);if(bgfx::isValid(screenVB))bgfx::destroy(screenVB);if(bgfx::isValid(screenIB))bgfx::destroy(screenIB);if(bgfx::isValid(atlasTexture))bgfx::destroy(atlasTexture);}
 void mesh(const EarthCamera& camera,double span,int material){if(!world)return;if(cachedX==camera.target_x_m&&cachedZ==camera.target_z_m&&cachedSpan==span&&cachedMaterial==material)return;
  // Nonuniform conforming grid: fine camera patch plus complete finite domain.
  // Shared rows prevent T-junctions and no land patch terminates at the camera edge.
  auto bounds=world->domain_bounds();
  auto axis=[](double centre,double extent,double low,double high){
   double start=std::clamp(centre-extent*.5,low,high),end=std::clamp(centre+extent*.5,low,high);
   if(end-start<.01){start=low;end=high;}
   std::vector<double> out;
   if(start>low)for(int i=0;i<32;++i)out.push_back(low+(start-low)*i/32);
   for(int i=0;i<=192;++i)out.push_back(start+(end-start)*i/192);
   if(end<high)for(int i=1;i<=32;++i)out.push_back(end+(high-end)*i/32);
   return out;
  };
  auto xs=axis(camera.target_x_m,span,bounds.min_x,bounds.max_x);
  auto zs=axis(camera.target_z_m,span,bounds.min_y,bounds.max_y);
  const int nx=int(xs.size()),nz=int(zs.size());
  std::vector<GroundVertex> v;v.reserve(nx*nz);std::vector<float> heights(nx*nz);
  for(int z=0;z<nz;++z)for(int x=0;x<nx;++x)heights[z*nx+x]=float(world->sample({xs[x],zs[z]}).height_m);
  // Local floating origin limits float loss; UV anchors remain World-based.
  for(int z=0;z<nz;++z)for(int x=0;x<nx;++x){int xl=std::max(x-1,0),xr=std::min(x+1,nx-1),zl=std::max(z-1,0),zr=std::min(z+1,nz-1);float dx=(heights[z*nx+xr]-heights[z*nx+xl])/float(xs[xr]-xs[xl]);float dz=(heights[zr*nx+x]-heights[zl*nx+x])/float(zs[zr]-zs[zl]);float len=std::sqrt(dx*dx+1+dz*dz);v.push_back({float(xs[x]-camera.target_x_m),heights[z*nx+x],float(zs[z]-camera.target_z_m),-dx/len,1/len,-dz/len,float(xs[x]/sizes[material]),float(zs[z]/sizes[material])});}
  std::vector<std::uint32_t> idx;idx.reserve((nx-1)*(nz-1)*6);for(int z=0;z<nz-1;++z)for(int x=0;x<nx-1;++x){std::uint32_t a=z*nx+x,b=a+nx;idx.insert(idx.end(),{a,b,a+1,a+1,b,b+1});}
  auto vb=bgfx::createVertexBuffer(bgfx::copy(v.data(),std::uint32_t(v.size()*sizeof(GroundVertex))),groundLayout);auto ib=bgfx::createIndexBuffer(bgfx::copy(idx.data(),std::uint32_t(idx.size()*4)),BGFX_BUFFER_INDEX32);
  if(!bgfx::isValid(vb)||!bgfx::isValid(ib)){if(bgfx::isValid(vb))bgfx::destroy(vb);if(bgfx::isValid(ib))bgfx::destroy(ib);throw std::runtime_error("Cannot upload Earth terrain mesh");}
  if(bgfx::isValid(terrainVB))bgfx::destroy(terrainVB);if(bgfx::isValid(terrainIB))bgfx::destroy(terrainIB);terrainVB=vb;terrainIB=ib;cachedX=camera.target_x_m;cachedZ=camera.target_z_m;cachedSpan=span;cachedMaterial=material;
 }
 void bindSky(bool dark,const std::array<float,4>& cam,float fog){int k=dark?1:0;bgfx::setTexture(3,skySampler,skies[k].texture);bgfx::setTexture(4,specularSampler,skies[k].prefilter);bgfx::setUniform(sh,skies[k].sh.data(),9);float pro[4]={1.f,fog,dark?.06f:1.f,float(world?world->config().seed&65535U:0)};bgfx::setUniform(profile,pro);bgfx::setUniform(eye,cam.data());}
 void screen(bgfx::ProgramHandle p,bgfx::ViewId view){bgfx::setVertexBuffer(0,screenVB);bgfx::setIndexBuffer(screenIB);bgfx::setState(BGFX_STATE_WRITE_RGB|BGFX_STATE_WRITE_A);bgfx::submit(view,p);}
};
EarthRenderer::EarthRenderer(const std::filesystem::path& root):impl_(std::make_unique<Impl>()){
 auto& p=*impl_;p.root=root;auto data=root/"assets/runtime/ground-sky";for(const auto& file:earth_resource_lock::files){auto path=data/file.path;if(!std::filesystem::is_regular_file(path)||std::filesystem::is_symlink(path)||std::filesystem::file_size(path)!=file.bytes||earth::sha256_file(path)!=file.sha256)throw std::runtime_error("Earth resource admission failed: "+path.string());}for(const auto& file:earth_resource_lock::shader_files){auto path=root/"shaders"/file.path;if(!std::filesystem::is_regular_file(path)||std::filesystem::is_symlink(path)||std::filesystem::file_size(path)!=file.bytes||earth::sha256_file(path)!=file.sha256)throw std::runtime_error("Earth shader admission failed: "+path.string());}p.recipe=earth_resource_lock::recipe_hash;
 auto folder=root/"shaders"/(bgfx::getRendererType()==bgfx::RendererType::Metal?"metal":"dx11");p.ground=program(folder,"earth_ground");p.sky=program(folder,"earth_sky");p.mapProgram=program(folder,"earth_map");p.lineProgram=program(folder,"earth_atlas");
 p.groundLayout.begin().add(bgfx::Attrib::Position,3,bgfx::AttribType::Float).add(bgfx::Attrib::Normal,3,bgfx::AttribType::Float).add(bgfx::Attrib::TexCoord0,2,bgfx::AttribType::Float).end();p.screenLayout.begin().add(bgfx::Attrib::Position,3,bgfx::AttribType::Float).add(bgfx::Attrib::TexCoord0,2,bgfx::AttribType::Float).end();p.mapLayout.begin().add(bgfx::Attrib::Position,3,bgfx::AttribType::Float).add(bgfx::Attrib::Color0,4,bgfx::AttribType::Uint8,true).end();
 p.base=bgfx::createUniform("s_base",bgfx::UniformType::Sampler);p.normal=bgfx::createUniform("s_normal",bgfx::UniformType::Sampler);p.arm=bgfx::createUniform("s_arm",bgfx::UniformType::Sampler);p.skySampler=bgfx::createUniform("s_sky",bgfx::UniformType::Sampler);p.specularSampler=bgfx::createUniform("s_specular",bgfx::UniformType::Sampler);p.eye=bgfx::createUniform("u_eye",bgfx::UniformType::Vec4);p.profile=bgfx::createUniform("u_profile",bgfx::UniformType::Vec4);p.sh=bgfx::createUniform("u_sh",bgfx::UniformType::Vec4,9);p.right=bgfx::createUniform("u_cameraRight",bgfx::UniformType::Vec4);p.up=bgfx::createUniform("u_cameraUp",bgfx::UniformType::Vec4);p.forward=bgfx::createUniform("u_cameraForward",bgfx::UniformType::Vec4);
 for(int i=0;i<8;++i){auto folder=data/ids[i];std::string stem=ids[i];p.materials[i][0]=groundTexture(folder/(stem+"_"+diffuseSuffix[i]+"_2k.png"),0);p.materials[i][1]=groundTexture(folder/(stem+"_nor_gl_2k.png"),1);p.materials[i][2]=groundTexture(folder/(stem+"_arm_2k.png"),2);for(auto t:p.materials[i])if(!bgfx::isValid(t))throw std::runtime_error("Cannot upload PBR textures");}
 p.skies[0]=skyTexture(data/"kloppenheim_05_puresky/radiance.bin");p.skies[1]=skyTexture(data/"qwantani_night_puresky/radiance.bin");p.skies[0].prefilter=skyTexture(data/"kloppenheim_05_puresky/specular.bin",true).texture;p.skies[1].prefilter=skyTexture(data/"qwantani_night_puresky/specular.bin",true).texture;
 std::array<ScreenVertex,4> screen={{{-1,-1,0,0,1},{1,-1,0,1,1},{1,1,0,1,0},{-1,1,0,0,0}}};std::array<std::uint16_t,6> ix={0,1,2,0,2,3};p.screenVB=bgfx::createVertexBuffer(bgfx::copy(screen.data(),sizeof(screen)),p.screenLayout);p.screenIB=bgfx::createIndexBuffer(bgfx::copy(ix.data(),sizeof(ix)));
 const bgfx::ViewId order[]={0,2,3,1};bgfx::setViewOrder(0,4,order);bgfx::setViewClear(2,BGFX_CLEAR_NONE);bgfx::setViewClear(3,BGFX_CLEAR_DEPTH,0,1.f,0);bgfx::setViewMode(2,bgfx::ViewMode::Sequential);bgfx::setViewMode(3,bgfx::ViewMode::Sequential);
}
EarthRenderer::~EarthRenderer()=default;
void EarthRenderer::set_world(std::shared_ptr<const earth::EarthWorldDefinition> world){if(!world)throw std::invalid_argument("Null Earth definition");auto previous=impl_->world;impl_->world=std::move(world);impl_->cachedX=1e20;try{EarthCamera initial;initial.height_m=float(std::max(impl_->world->config().width_m,impl_->world->config().height_m)*.45);impl_->mesh(initial,std::max(impl_->world->config().width_m,impl_->world->config().height_m)+2*impl_->world->config().sea_collar_m,1);}catch(...){impl_->world=std::move(previous);impl_->cachedX=1e20;throw;}}
void EarthRenderer::clear_world(){impl_->world.reset();impl_->cachedX=1e20;}
bool EarthRenderer::ready()const{const auto& p=*impl_;if(!bgfx::isValid(p.ground)||!bgfx::isValid(p.sky)||!bgfx::isValid(p.mapProgram)||!bgfx::isValid(p.lineProgram)||!bgfx::isValid(p.screenVB)||!bgfx::isValid(p.screenIB))return false;for(const auto& m:p.materials)for(auto t:m)if(!bgfx::isValid(t))return false;for(const auto& s:p.skies)if(!bgfx::isValid(s.texture)||!bgfx::isValid(s.prefilter))return false;return true;}
std::string EarthRenderer::resource_recipe_hash()const{return impl_->recipe;}
const char* EarthRenderer::material_name(int i)const{return ids[std::clamp(i,0,7)];}
void EarthRenderer::draw(const EarthCamera& camera,bool dark,int w,int h,int overrideMaterial){auto& p=*impl_;if(!p.world||w<=0||h<=0)return;int material=overrideMaterial>=0?std::clamp(overrideMaterial,0,7):1;float height=std::max(camera.height_m,2.f);float pitch=std::clamp(camera.pitch_deg,20.f,89.f)*pi/180,yaw=camera.yaw_deg*pi/180;float horizontal=height/std::tan(pitch);bx::Vec3 eye={horizontal*std::sin(yaw),height,-horizontal*std::cos(yaw)},at={0,0,0};float view[16],proj[16];bx::mtxLookAt(view,eye,at);float span=height*5.0*std::max(1.f,float(w)/h);
 if(camera.orthographic){span=height*2;bx::mtxOrtho(proj,-span*.5f*float(w)/h,span*.5f*float(w)/h,-span*.5f,span*.5f,.1f,height*20,0,bgfx::getCaps()->homogeneousDepth);}else bx::mtxProj(proj,50.f,float(w)/h,.1f,height*50,bgfx::getCaps()->homogeneousDepth);
 p.mesh(camera,span,material);bgfx::setViewRect(2,0,0,std::uint16_t(w),std::uint16_t(h));bgfx::setViewRect(3,0,0,std::uint16_t(w),std::uint16_t(h));bgfx::setViewTransform(3,view,proj);
 auto f=bx::normalize(bx::sub(at,eye)),r=bx::normalize(bx::cross({0,1,0},f)),u=bx::cross(f,r);float cf[4]={f.x,f.y,f.z,camera.orthographic?1.f:0.f},cr[4]={r.x,r.y,r.z,float(w)/h*.46630766f},cu[4]={u.x,u.y,u.z,.46630766f};bgfx::setUniform(p.forward,cf);bgfx::setUniform(p.right,cr);bgfx::setUniform(p.up,cu);std::array<float,4> ce={eye.x,eye.y,eye.z,0};p.bindSky(dark,ce,.000035f);p.screen(p.sky,2);
 bgfx::setUniform(p.forward,cf);p.bindSky(dark,ce,.000035f);for(int c=0;c<3;++c)bgfx::setTexture(std::uint8_t(c),c==0?p.base:c==1?p.normal:p.arm,p.materials[material][c]);bgfx::setVertexBuffer(0,p.terrainVB);bgfx::setIndexBuffer(p.terrainIB);bgfx::setState(BGFX_STATE_WRITE_RGB|BGFX_STATE_WRITE_A|BGFX_STATE_WRITE_Z|BGFX_STATE_DEPTH_TEST_LESS|BGFX_STATE_MSAA);bgfx::submit(3,p.ground);
}
void EarthRenderer::draw_atlas(const earth::GeoAtlas& atlas,double lon,double lat,double span,int w,int h,earth::LonLat selected,double selectionW,double selectionH){auto& p=*impl_;if(w<=0||h<=0)return;span=std::clamp(span,.001,360.0);double latitudeSpan=std::min(180.0,span*double(h)/w);lat=std::clamp(lat,-90.0+latitudeSpan*.5,90.0-latitudeSpan*.5);int mw=std::min(w,2048),mh=std::min(h,1536);double left=lon-span*.5,top=lat+latitudeSpan*.5;
 if(p.atlasLon!=lon||p.atlasLat!=lat||p.atlasSpan!=span||p.atlasW!=mw||p.atlasH!=mh){std::vector<std::uint32_t> pixels(std::size_t(mw)*mh,0xff342416U);auto detail=span>100?earth::Detail::Coarse:span>25?earth::Detail::Low:span>3?earth::Detail::Intermediate:earth::Detail::Full;
  // Scanline polygon filling, level order preserves lake/island/pond holes.
  for(int level=1;level<=4;++level)for(const auto& ring:atlas.rings(detail)){int actual=ring.level==5?1:ring.level;if(actual!=level||ring.vertices.size()<3)continue;bool polar=std::any_of(ring.vertices.begin(),ring.vertices.end(),[](earth::LonLat a){return a.latitude<=-89.999;});if(ring.north<top-latitudeSpan||(!polar&&ring.south>top))continue;std::vector<std::array<double,2>> points;points.reserve(ring.vertices.size());double previous=ring.vertices[0].longitude;for(auto a:ring.vertices){double x=a.longitude;if(!polar){while(x-previous>180)x-=360;while(x-previous<-180)x+=360;}points.push_back({x,a.latitude});previous=x;}
   double lo=points.front()[0],hi=lo;for(auto a:points){lo=std::min(lo,a[0]);hi=std::max(hi,a[0]);}double shift=std::round((lon-(lo+hi)*.5)/360)*360;for(auto& a:points)a[0]+=shift;
   for(int copy=-1;copy<=1;++copy){
    double move=copy*360;if(hi+shift+move<left||lo+shift+move>left+span)continue;
    // Edge buckets cost O(vertices + covered scanlines), not O(vertices*pixels).
    // A continental Full ring can have a million points; narrow zoom must remain usable.
    std::vector<std::vector<double>> rows(mh);
    for(std::size_t i=0,j=points.size()-1;i<points.size();j=i++){
     auto a=points[i],b=points[j];if(a[1]==b[1])continue;
     double ymin=std::min(a[1],b[1]),ymax=std::max(a[1],b[1]);
     int begin=std::max(0,int(std::ceil((top-ymax)/latitudeSpan*mh-.5)));
     int end=std::min(mh-1,int(std::ceil((top-ymin)/latitudeSpan*mh-.5))-1);
     for(int y=begin;y<=end;++y){double yy=top-(y+.5)*latitudeSpan/mh;rows[y].push_back((a[0]+(yy-a[1])*(b[0]-a[0])/(b[1]-a[1])+move-left)/span*mw);}
    }
    for(int y=0;y<mh;++y){auto& intersections=rows[y];std::sort(intersections.begin(),intersections.end());for(std::size_t i=1;i<intersections.size();i+=2){int x0=std::clamp(int(std::ceil(intersections[i-1]-.5)),0,mw),x1=std::clamp(int(std::ceil(intersections[i]-.5)),0,mw);for(int x=x0;x<x1;++x){if(level%2){int grain=0;auto r=std::uint32_t(std::clamp(95+grain,0,255)),g=std::uint32_t(std::clamp(106+grain,0,255)),b=std::uint32_t(std::clamp(86+grain,0,255));pixels[std::size_t(y)*mw+x]=0xff000000U|(b<<16)|(g<<8)|r;}else pixels[std::size_t(y)*mw+x]=0xff342416U;}}}
   }
  }
  auto texture=bgfx::createTexture2D(std::uint16_t(mw),std::uint16_t(mh),false,1,bgfx::TextureFormat::RGBA8,BGFX_SAMPLER_U_CLAMP|BGFX_SAMPLER_V_CLAMP,bgfx::copy(pixels.data(),std::uint32_t(pixels.size()*4)));if(!bgfx::isValid(texture))throw std::runtime_error("Cannot upload geographic atlas");if(bgfx::isValid(p.atlasTexture))bgfx::destroy(p.atlasTexture);p.atlasTexture=texture;p.atlasLon=lon;p.atlasLat=lat;p.atlasSpan=span;p.atlasW=mw;p.atlasH=mh;
 }
 bgfx::setViewRect(2,0,0,std::uint16_t(w),std::uint16_t(h));bgfx::setViewTransform(2,nullptr,nullptr);bgfx::setTexture(0,p.base,p.atlasTexture);p.screen(p.mapProgram,2);
 // Selection is a geographic overlay, never a framed button.
 double selectedLon=selected.longitude;while(selectedLon-lon>180)selectedLon-=360;while(selectedLon-lon<-180)selectedLon+=360;double dx=selectionW/(111195.08*std::max(.01,std::cos(selected.latitude*pi/180)))*.5,dy=selectionH/111195.08*.5;
 float x0=float((selectedLon-dx-left)/span*2-1),x1=float((selectedLon+dx-left)/span*2-1),y0=float(1-(top-selected.latitude-dy)/latitudeSpan*2),y1=float(1-(top-selected.latitude+dy)/latitudeSpan*2);
 std::array<MapVertex,8> verts={{{x0,y0,0,0xffdff6ffU},{x1,y0,0,0xffdff6ffU},{x1,y0,0,0xffdff6ffU},{x1,y1,0,0xffdff6ffU},{x1,y1,0,0xffdff6ffU},{x0,y1,0,0xffdff6ffU},{x0,y1,0,0xffdff6ffU},{x0,y0,0,0xffdff6ffU}}};bgfx::TransientVertexBuffer vb;if(bgfx::getAvailTransientVertexBuffer(8,p.mapLayout)==8){bgfx::allocTransientVertexBuffer(&vb,8,p.mapLayout);std::memcpy(vb.data,verts.data(),sizeof(verts));bgfx::setVertexBuffer(0,&vb);bgfx::setState(BGFX_STATE_WRITE_RGB|BGFX_STATE_WRITE_A|BGFX_STATE_PT_LINES);bgfx::submit(2,p.lineProgram);}
}
}
