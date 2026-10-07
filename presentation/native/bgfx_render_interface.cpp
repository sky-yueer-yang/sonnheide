#include "bgfx_render_interface.hpp"
#include <sonnheide/native_platform.hpp>
#include <bgfx/embedded_shader.h>
#include <bx/math.h>
#include <vs_debugdraw_fill_texture.bin.h>
#include <fs_ocornut_imgui.bin.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <fstream>
#include <map>
#include <regex>
#include <set>
#include <stdexcept>
#include <vector>
namespace sonnheide::native {
namespace {
std::string path_utf8(const std::filesystem::path& p){auto value=p.u8string();return std::string(value.begin(),value.end());}
const bgfx::EmbeddedShader shaders[]={BGFX_EMBEDDED_SHADER(vs_debugdraw_fill_texture),BGFX_EMBEDDED_SHADER(fs_ocornut_imgui),BGFX_EMBEDDED_SHADER_END()};
struct Vertex {float x,y,z,u,v;std::uint32_t color; Vertex(float px,float py,float pu,float pv,std::uint32_t c):x(px),y(py),z(0),u(pu),v(pv),color(c){} };
struct Geometry {bgfx::VertexBufferHandle vb; bgfx::IndexBufferHandle ib;bgfx::TextureHandle texture;};
std::uint32_t color(float alpha,int r=255,int g=246,int b=223){return std::uint32_t(r)|(std::uint32_t(g)<<8)|(std::uint32_t(b)<<16)|(std::uint32_t(std::clamp(alpha,0.f,1.f)*255)<<24);}
constexpr float pi=3.14159265359f;
}
struct BgfxRenderInterface::Impl {
 std::filesystem::path root; bgfx::VertexLayout layout;
 bgfx::ProgramHandle program=BGFX_INVALID_HANDLE;bgfx::UniformHandle sampler=BGFX_INVALID_HANDLE;bgfx::TextureHandle white=BGFX_INVALID_HANDLE;
 std::map<std::filesystem::path,bgfx::TextureHandle> files;std::set<std::uint16_t> textures;std::set<Geometry*> geometries;
 int lw=1,lh=1,pw=1,ph=1; std::uint16_t view=0;bool scissors=false;int sx=0,sy=0,sw=1,sh=1;
 std::array<float,16> transform{};bool transformed=false;std::vector<Vertex> instrument;std::vector<int> instrument_indices;
 void submit(bgfx::VertexBufferHandle vb,bgfx::IndexBufferHandle ib,bgfx::TextureHandle tex,float tx,float ty){
  if(scissors){int x=std::clamp(int(std::floor(float(sx)*(view==1?1.f:float(pw)/lw))),0,pw),y=std::clamp(int(std::floor(float(sy)*(view==1?1.f:float(ph)/lh))),0,ph);
   int right=std::clamp(int(std::ceil(float(sx+sw)*(view==1?1.f:float(pw)/lw))),0,pw),bottom=std::clamp(int(std::ceil(float(sy+sh)*(view==1?1.f:float(ph)/lh))),0,ph);
   if(right<=x || bottom<=y)return;bgfx::setScissor(std::uint16_t(x),std::uint16_t(y),std::uint16_t(right-x),std::uint16_t(bottom-y));
  }else bgfx::setScissor(UINT16_MAX);
  float model[16];if(transformed)std::copy(transform.begin(),transform.end(),model);else bx::mtxIdentity(model);
  // Translation is in the element's local space before the CSS transform.
  for(int row=0;row<4;++row)model[12+row]+=model[row]*tx+model[4+row]*ty;
  bgfx::setTransform(model);bgfx::setVertexBuffer(0,vb);bgfx::setIndexBuffer(ib);
  bgfx::setTexture(0,sampler,bgfx::isValid(tex)?tex:white);
  bgfx::setState(BGFX_STATE_WRITE_RGB|BGFX_STATE_WRITE_A|BGFX_STATE_MSAA|BGFX_STATE_BLEND_FUNC_SEPARATE(BGFX_STATE_BLEND_SRC_ALPHA,BGFX_STATE_BLEND_INV_SRC_ALPHA,BGFX_STATE_BLEND_ONE,BGFX_STATE_BLEND_INV_SRC_ALPHA));
  bgfx::submit(view,program);
 }
 void draw(const std::vector<Vertex>& v,const std::vector<int>& inds,bgfx::TextureHandle tex){
  if(v.empty()||inds.empty())return;
  auto vb=bgfx::createVertexBuffer(bgfx::copy(v.data(),std::uint32_t(v.size()*sizeof(Vertex))),layout);
  auto ib=bgfx::createIndexBuffer(bgfx::copy(inds.data(),std::uint32_t(inds.size()*sizeof(int))),BGFX_BUFFER_INDEX32);
  submit(vb,ib,tex,0,0);bgfx::destroy(vb);bgfx::destroy(ib);
 }
 void quad(float x,float y,float w,float h,std::uint32_t c,bgfx::TextureHandle tex){draw({{x,y,0,0,c},{x+w,y,1,0,c},{x+w,y+h,1,1,c},{x,y+h,0,1,c}},{0,1,2,0,2,3},tex);}
 void line(float x,float y,float ex,float ey,float width,float opacity,std::vector<Vertex>& v,std::vector<int>& i){float dx=ex-x,dy=ey-y,len=std::hypot(dx,dy);if(len==0)return;float nx=-dy/len*width*.5f,ny=dx/len*width*.5f;int a=int(v.size());auto c=color(opacity);v.insert(v.end(),{{x+nx,y+ny,0,0,c},{ex+nx,ey+ny,0,0,c},{ex-nx,ey-ny,0,0,c},{x-nx,y-ny,0,0,c}});i.insert(i.end(),{a,a+1,a+2,a,a+2,a+3});}
 void load_instrument(){
  std::ifstream f(root/"assets/source/ui/branding/sonnreich-compass.svg");if(!f)throw std::runtime_error("Missing original compass source");
  std::regex ring(R"re(<circle cx="500" cy="500" r="([0-9.]+)" stroke-width="([0-9.]+)" opacity="([0-9.]+)"/>)re");
  std::regex ray(R"re(<line x1="([0-9.]+)" y1="([0-9.]+)" x2="([0-9.]+)" y2="([0-9.]+)" stroke-width="([0-9.]+)" opacity="([0-9.]+)"/>)re");
  std::string s;int rings=0,lines=0;std::smatch m;
  while(std::getline(f,s)){if(std::regex_search(s,m,ring)){++rings;float r=std::stof(m[1]),w=std::stof(m[2]),o=std::stof(m[3]);for(int a=0;a<720;++a){float t=a*pi/360.f,nt=(a+1)*pi/360.f;line(500+r*std::cos(t),500+r*std::sin(t),500+r*std::cos(nt),500+r*std::sin(nt),w,o,instrument,instrument_indices);}}
   else if(std::regex_search(s,m,ray)){++lines;line(std::stof(m[1]),std::stof(m[2]),std::stof(m[3]),std::stof(m[4]),std::stof(m[5]),std::stof(m[6]),instrument,instrument_indices);}}
  if(rings!=6||lines!=480)throw std::runtime_error("Original compass source topology mismatch");
 }
};
BgfxRenderInterface::BgfxRenderInterface(void* window,const std::filesystem::path& root,int pw,int ph):impl_(std::make_unique<Impl>()){
 auto& p=*impl_;p.root=root;p.load_instrument();bgfx::renderFrame();
 bgfx::Init init;
#ifdef _WIN32
 init.type=bgfx::RendererType::Direct3D11;
#else
 init.type=bgfx::RendererType::Metal;
#endif
 init.fallback=false;init.swapChain.nwh=window;init.swapChain.width=std::uint32_t(pw);init.swapChain.height=std::uint32_t(ph);init.reset=(BGFX_RESET_VSYNC|BGFX_RESET_MSAA_X4);
 if(!bgfx::init(init))throw std::runtime_error("Native GPU renderer initialisation failed");
 try {
 p.pw=pw;p.ph=ph;
 p.layout.begin().add(bgfx::Attrib::Position,3,bgfx::AttribType::Float).add(bgfx::Attrib::TexCoord0,2,bgfx::AttribType::Float).add(bgfx::Attrib::Color0,4,bgfx::AttribType::Uint8,true).end();
 auto vs=bgfx::createEmbeddedShader(shaders,init.type,"vs_debugdraw_fill_texture");
 auto fs=bgfx::createEmbeddedShader(shaders,init.type,"fs_ocornut_imgui");
 if(!bgfx::isValid(vs)||!bgfx::isValid(fs)){if(bgfx::isValid(vs))bgfx::destroy(vs);if(bgfx::isValid(fs))bgfx::destroy(fs);throw std::runtime_error("Cannot load pinned native UI shaders");}
 p.program=bgfx::createProgram(vs,fs,true);
 p.sampler=bgfx::createUniform("s_tex",bgfx::UniformType::Sampler);
 const std::uint32_t white=0xffffffff;p.white=bgfx::createTexture2D(1,1,false,1,bgfx::TextureFormat::RGBA8,0,bgfx::copy(&white,4));
 if(!bgfx::isValid(p.program)||!bgfx::isValid(p.white))throw std::runtime_error("Cannot create native UI program");
 bgfx::setViewClear(0,BGFX_CLEAR_COLOR,0x050403ff,1.f,0);bgfx::setViewMode(0,bgfx::ViewMode::Sequential);bgfx::setViewMode(1,bgfx::ViewMode::Sequential);
 } catch(...) {if(bgfx::isValid(p.white))bgfx::destroy(p.white);if(bgfx::isValid(p.sampler))bgfx::destroy(p.sampler);if(bgfx::isValid(p.program))bgfx::destroy(p.program);bgfx::frame();bgfx::shutdown();throw;}
}
BgfxRenderInterface::~BgfxRenderInterface(){if(!impl_)return;auto& p=*impl_;for(auto* g:p.geometries){bgfx::destroy(g->vb);bgfx::destroy(g->ib);delete g;}for(auto i:p.textures)bgfx::destroy(bgfx::TextureHandle{i});if(bgfx::isValid(p.white))bgfx::destroy(p.white);if(bgfx::isValid(p.sampler))bgfx::destroy(p.sampler);if(bgfx::isValid(p.program))bgfx::destroy(p.program);bgfx::frame();bgfx::shutdown();}
void BgfxRenderInterface::begin_frame(int w,int h,int pw,int ph){auto& p=*impl_;p.lw=std::max(w,1);p.lh=std::max(h,1);if(p.pw!=pw||p.ph!=ph){p.pw=std::max(pw,1);p.ph=std::max(ph,1);bgfx::SwapChain sc;sc.width=std::uint32_t(p.pw);sc.height=std::uint32_t(p.ph);bgfx::reset((BGFX_RESET_VSYNC|BGFX_RESET_MSAA_X4),&sc);}float proj[16];bx::mtxOrtho(proj,0,float(w),float(h),0,0,100,0,bgfx::getCaps()->homogeneousDepth);bgfx::setViewTransform(0,nullptr,proj);bgfx::setViewRect(0,0,0,std::uint16_t(p.pw),std::uint16_t(p.ph));bx::mtxOrtho(proj,0,float(p.pw),float(p.ph),0,0,100,0,bgfx::getCaps()->homogeneousDepth);bgfx::setViewTransform(1,nullptr,proj);bgfx::setViewRect(1,0,0,std::uint16_t(p.pw),std::uint16_t(p.ph));bgfx::setViewClear(1,BGFX_CLEAR_NONE);p.view=0;p.scissors=false;p.transformed=false;bgfx::touch(0);}
void BgfxRenderInterface::begin_ui_frame(){impl_->view=1;impl_->scissors=false;impl_->transformed=false;}
void BgfxRenderInterface::end_frame(){bgfx::frame();}
const char* BgfxRenderInterface::renderer_name()const{return bgfx::getRendererName(bgfx::getRendererType());}
void BgfxRenderInterface::RenderGeometry(Rml::Vertex* verts,int n,int* inds,int ni,Rml::TextureHandle tex,const Rml::Vector2f& t){auto handle=CompileGeometry(verts,n,inds,ni,tex);if(handle){RenderCompiledGeometry(handle,t);ReleaseCompiledGeometry(handle);}}
Rml::CompiledGeometryHandle BgfxRenderInterface::CompileGeometry(Rml::Vertex* verts,int n,int* inds,int ni,Rml::TextureHandle tex){if(n<=0||ni<=0)return 0;std::vector<Vertex> v;v.reserve(n);for(int a=0;a<n;++a){auto& r=verts[a];v.push_back({r.position.x,r.position.y,r.tex_coord.x,r.tex_coord.y,std::uint32_t(r.colour.red)|(std::uint32_t(r.colour.green)<<8)|(std::uint32_t(r.colour.blue)<<16)|(std::uint32_t(r.colour.alpha)<<24)});}auto* g=new Geometry{bgfx::createVertexBuffer(bgfx::copy(v.data(),std::uint32_t(v.size()*sizeof(Vertex))),impl_->layout),bgfx::createIndexBuffer(bgfx::copy(inds,std::uint32_t(ni*sizeof(int))),BGFX_BUFFER_INDEX32),tex?bgfx::TextureHandle{std::uint16_t(tex-1)}:bgfx::TextureHandle{bgfx::kInvalidHandle}};impl_->geometries.insert(g);return reinterpret_cast<Rml::CompiledGeometryHandle>(g);}
void BgfxRenderInterface::RenderCompiledGeometry(Rml::CompiledGeometryHandle h,const Rml::Vector2f& t){auto* g=reinterpret_cast<Geometry*>(h);if(impl_->geometries.count(g)){impl_->view=1;impl_->submit(g->vb,g->ib,g->texture,t.x,t.y);}}
void BgfxRenderInterface::ReleaseCompiledGeometry(Rml::CompiledGeometryHandle h){auto* g=reinterpret_cast<Geometry*>(h);if(impl_->geometries.erase(g)){bgfx::destroy(g->vb);bgfx::destroy(g->ib);delete g;}}
void BgfxRenderInterface::EnableScissorRegion(bool b){impl_->scissors=b;}
void BgfxRenderInterface::SetScissorRegion(int x,int y,int w,int h){impl_->sx=x;impl_->sy=y;impl_->sw=w;impl_->sh=h;}
void BgfxRenderInterface::SetTransform(const Rml::Matrix4f* t){impl_->transformed=t!=nullptr;if(t)std::copy(t->data(),t->data()+16,impl_->transform.begin());}
bool BgfxRenderInterface::GenerateTexture(Rml::TextureHandle& handle,const Rml::byte* src,const Rml::Vector2i& dim){if(dim.x<=0||dim.y<=0||dim.x>16384||dim.y>16384)return false;auto t=bgfx::createTexture2D(std::uint16_t(dim.x),std::uint16_t(dim.y),false,1,bgfx::TextureFormat::RGBA8,BGFX_SAMPLER_U_CLAMP|BGFX_SAMPLER_V_CLAMP,bgfx::copy(src,std::uint32_t(dim.x*dim.y*4)));if(!bgfx::isValid(t))return false;impl_->textures.insert(t.idx);handle=std::uintptr_t(t.idx)+1;return true;}
bool BgfxRenderInterface::LoadTexture(Rml::TextureHandle& h,Rml::Vector2i& dim,const Rml::String& src){try{auto path=std::filesystem::path(std::u8string(src.begin(),src.end()));if(path.is_relative())path=impl_->root/path;auto pixels=load_image(path);dim={pixels.width,pixels.height};return GenerateTexture(h,pixels.rgba.data(),dim);}catch(const std::exception&){return false;}}
void BgfxRenderInterface::ReleaseTexture(Rml::TextureHandle h){if(h&&impl_->textures.erase(std::uint16_t(h-1)))bgfx::destroy(bgfx::TextureHandle{std::uint16_t(h-1)});}
bool BgfxRenderInterface::prepare_image(const std::filesystem::path& path){auto& p=*impl_;if(p.files.count(path))return true;Rml::TextureHandle h=0;Rml::Vector2i dim;if(!LoadTexture(h,dim,path_utf8(path)))return false;p.files.emplace(path,bgfx::TextureHandle{std::uint16_t(h-1)});return true;}
void BgfxRenderInterface::draw_image(const std::filesystem::path& path,float x,float y,float w,float h,float opacity){auto& p=*impl_;auto it=p.files.find(path);if(it==p.files.end()){Rml::TextureHandle handle=0;Rml::Vector2i dim;if(!LoadTexture(handle,dim,path_utf8(path)))throw std::runtime_error("Missing or invalid image: "+path_utf8(path));it=p.files.emplace(path,bgfx::TextureHandle{std::uint16_t(handle-1)}).first;}p.quad(x,y,w,h,color(opacity,255,255,255),it->second);}
void BgfxRenderInterface::draw_menu_shade(float w,float h){auto& p=*impl_;std::vector<Vertex> v;std::vector<int> inds;const int nx=48,ny=36;for(int y=0;y<=ny;++y)for(int x=0;x<=nx;++x){float u=float(x)/nx,z=float(y)/ny,r=std::hypot((u-.51f)/.49f,(z-.43f)/.61f);float t=std::clamp((r-.08f)/.95f,0.f,1.f);float alpha=.95f*t*t*(3-2*t);float bottom=std::clamp((z-.7f)/.3f,0.f,1.f)*.22f;alpha=1-(1-alpha)*(1-bottom);v.push_back({u*w,z*h,0,0,color(alpha,5,4,3)});}for(int y=0;y<ny;++y)for(int x=0;x<nx;++x){int a=y*(nx+1)+x,b=a+nx+1;inds.insert(inds.end(),{a,a+1,b+1,a,b+1,b});}p.draw(v,inds,BGFX_INVALID_HANDLE);}
void BgfxRenderInterface::draw_compass(float cx,float cy,float size,float logo){auto& p=*impl_;p.scissors=false;p.transformed=false;
 // Soft static halo built from translucent annuli, preserving source PNG bytes.
 std::vector<Vertex> halo;std::vector<int> hi;float radius=logo*1.75f;
 for(int j=0;j<20;++j){float a=float(j)/20,b=float(j+1)/20;for(int k=0;k<120;++k){float t=k*2*pi/120,nt=(k+1)*2*pi/120;int base=int(halo.size());float aa=.11f*std::pow(1-a,3),ab=.11f*std::pow(1-b,3);halo.insert(halo.end(),{{cx+radius*a*std::cos(t),cy+radius*a*std::sin(t),0,0,color(aa)},{cx+radius*a*std::cos(nt),cy+radius*a*std::sin(nt),0,0,color(aa)},{cx+radius*b*std::cos(nt),cy+radius*b*std::sin(nt),0,0,color(ab)},{cx+radius*b*std::cos(t),cy+radius*b*std::sin(t),0,0,color(ab)}});hi.insert(hi.end(),{base,base+1,base+2,base,base+2,base+3});}}p.draw(halo,hi,BGFX_INVALID_HANDLE);
 std::vector<Vertex> v=p.instrument;float angle=12*pi/180,cs=std::cos(angle),sn=std::sin(angle);for(auto& a:v){float x=(a.x-500)*size/1000,y=(a.y-500)*size/1000;a.x=cx+x*cs-y*sn;a.y=cy+x*sn+y*cs;}p.draw(v,p.instrument_indices,BGFX_INVALID_HANDLE);
 Rml::TextureHandle handle=0;Rml::Vector2i d;const std::filesystem::path path="assets/source/ui/branding/sonnreich_logo_transparent_fullsize.png";auto it=p.files.find(path);if(it==p.files.end()){if(!LoadTexture(handle,d,path_utf8(path)))throw std::runtime_error("Missing company logo");it=p.files.emplace(path,bgfx::TextureHandle{std::uint16_t(handle-1)}).first;}
 auto c=color(1,255,255,255);std::vector<Vertex> mark={{-logo/2,-logo/2,0,0,c},{logo/2,-logo/2,1,0,c},{logo/2,logo/2,1,1,c},{-logo/2,logo/2,0,1,c}};for(auto& a:mark){float x=a.x,y=a.y;a.x=cx+x*cs-y*sn;a.y=cy+x*sn+y*cs;}p.draw(mark,{0,1,2,0,2,3},it->second);
}
}
