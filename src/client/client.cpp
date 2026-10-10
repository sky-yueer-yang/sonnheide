#include "client.hpp"
#include <SDL3/SDL.h>
#include <RmlUi/Core.h>
#include <RmlUi/Core/Element.h>
#include <bgfx/bgfx.h>
#include <bx/math.h>
#include <bx/allocator.h>
#include <bimg/decode.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <fstream>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>
#include <vector>
namespace sonnheide::client {
namespace {
constexpr double pi=3.14159265358979323846;
std::string utf8(const std::filesystem::path& p){auto s=p.u8string();return {reinterpret_cast<const char*>(s.data()),s.size()};}
Vec3 subtract(Vec3 a,Vec3 b){return {a.x-b.x,a.y-b.y,a.z-b.z};}
Vec3 add(Vec3 a,Vec3 b){return {a.x+b.x,a.y+b.y,a.z+b.z};}
Vec3 scale(Vec3 a,double k){return {a.x*k,a.y*k,a.z*k};}
double dot(Vec3 a,Vec3 b){return a.x*b.x+a.y*b.y+a.z*b.z;}
Vec3 cross(Vec3 a,Vec3 b){return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};}
Vec3 normalized(Vec3 a){double n=std::sqrt(dot(a,a));return n>1e-12?scale(a,1/n):Vec3{0,1,0};}
struct Basis{Vec3 eye,forward,right,up;};
Basis basis(const Camera& c){double y=c.yaw_deg*pi/180,p=c.pitch_deg*pi/180;Vec3 offset{c.distance_m*std::cos(p)*std::sin(y),c.distance_m*std::sin(p),-c.distance_m*std::cos(p)*std::cos(y)};Vec3 f=normalized(scale(offset,-1)),r=normalized(cross({0,1,0},f));return {add(c.target,offset),f,r,cross(f,r)};}
std::uint32_t rgba(int r,int g,int b,int a=255){return std::uint32_t(r)|(std::uint32_t(g)<<8)|(std::uint32_t(b)<<16)|(std::uint32_t(a)<<24);}
std::uint32_t hash(std::uint32_t x){x^=x>>16;x*=0x7feb352dU;x^=x>>15;x*=0x846ca68bU;return x^(x>>16);}
struct Vertex{float x,y,z,nx,ny,nz,u,v;std::uint32_t color;};
struct ScreenVertex{float x,y,z,u,v;};
struct UiVertex{float x,y,z,u,v;std::uint32_t color;};
struct Buffer{bgfx::VertexBufferHandle vb=BGFX_INVALID_HANDLE;bgfx::IndexBufferHandle ib=BGFX_INVALID_HANDLE;std::size_t vertices{},indices{};};
void release(Buffer& b){if(bgfx::isValid(b.vb))bgfx::destroy(b.vb);if(bgfx::isValid(b.ib))bgfx::destroy(b.ib);b={};}
Buffer upload(const std::vector<Vertex>& v,const std::vector<std::uint32_t>& i,const bgfx::VertexLayout& layout){if(v.empty()||i.empty())return {};Buffer b{bgfx::createVertexBuffer(bgfx::copy(v.data(),std::uint32_t(v.size()*sizeof(Vertex))),layout),bgfx::createIndexBuffer(bgfx::copy(i.data(),std::uint32_t(i.size()*4)),BGFX_BUFFER_INDEX32)};if(!bgfx::isValid(b.vb)||!bgfx::isValid(b.ib)){release(b);throw std::runtime_error("GPU geometry upload failed");}b.vertices=v.size();b.indices=i.size();return b;}
// Exact coplanar renderer rectangle. Does not mutate authoritative cells.
void quad(sonn::Mesh& mesh,const std::array<Vec3,4>& points,Vec3 normal){
 auto base=std::uint32_t(mesh.vertices.size());for(int k=0;k<4;++k){auto p=points[k];mesh.vertices.push_back({float(p.x),float(p.y),float(p.z),float(normal.x),float(normal.y),float(normal.z),float(k==1||k==2),float(k>=2),0});}
 for(auto i:{0U,1U,2U,0U,2U,3U})mesh.indices.push_back(base+i);
}
std::vector<std::uint8_t> read(const std::filesystem::path& p){return sonn::read_file(p);}
bgfx::ProgramHandle program(const std::filesystem::path& folder,const char* vertex,const char* fragment){auto v=read(folder/(std::string(vertex)+".vs.bin")),f=read(folder/(std::string(fragment)+".fs.bin"));auto vs=bgfx::createShader(bgfx::copy(v.data(),std::uint32_t(v.size())));if(!bgfx::isValid(vs))throw std::runtime_error("Invalid native vertex shader");auto fs=bgfx::createShader(bgfx::copy(f.data(),std::uint32_t(f.size())));if(!bgfx::isValid(fs)){bgfx::destroy(vs);throw std::runtime_error("Invalid native fragment shader");}auto p=bgfx::createProgram(vs,fs,true);if(!bgfx::isValid(p))throw std::runtime_error("Native shader linking failed");return p;}
int modifiers(SDL_Keymod m){int r=0;if(m&SDL_KMOD_CTRL)r|=Rml::Input::KM_CTRL;if(m&SDL_KMOD_SHIFT)r|=Rml::Input::KM_SHIFT;if(m&SDL_KMOD_ALT)r|=Rml::Input::KM_ALT;if(m&SDL_KMOD_GUI)r|=Rml::Input::KM_META;if(m&SDL_KMOD_CAPS)r|=Rml::Input::KM_CAPSLOCK;return r;}
Rml::Input::KeyIdentifier key(SDL_Keycode k){using namespace Rml::Input;if(k>=SDLK_A&&k<=SDLK_Z)return KeyIdentifier(KI_A+int(k-SDLK_A));if(k>=SDLK_0&&k<=SDLK_9)return KeyIdentifier(KI_0+int(k-SDLK_0));switch(k){case SDLK_RETURN:return KI_RETURN;case SDLK_ESCAPE:return KI_ESCAPE;case SDLK_BACKSPACE:return KI_BACK;case SDLK_TAB:return KI_TAB;case SDLK_SPACE:return KI_SPACE;case SDLK_DELETE:return KI_DELETE;case SDLK_LEFT:return KI_LEFT;case SDLK_RIGHT:return KI_RIGHT;case SDLK_UP:return KI_UP;case SDLK_DOWN:return KI_DOWN;case SDLK_HOME:return KI_HOME;case SDLK_END:return KI_END;case SDLK_PAGEUP:return KI_PRIOR;case SDLK_PAGEDOWN:return KI_NEXT;case SDLK_LSHIFT:return KI_LSHIFT;case SDLK_RSHIFT:return KI_RSHIFT;case SDLK_LCTRL:return KI_LCONTROL;case SDLK_RCTRL:return KI_RCONTROL;case SDLK_LALT:return KI_LMENU;case SDLK_RALT:return KI_RMENU;default:return KI_UNKNOWN;}}
}
SurfaceView surface_view(const sonn::World& world){
 SurfaceView view;auto& terrain=world.terrain;view.identity=world.id;view.revision=terrain.surface_revision();view.width=terrain.width;view.height=terrain.height;view.cell_m=terrain.cell_mm/1000.;view.profile=terrain.profile;
 for(int k=0;k<8;++k)view.height_levels[k]=terrain.height_for(sonn::TerrainKind(k))/1000.f;
 view.enumerate_fine=[&terrain](int x,int z,int w,int h,const SurfaceView::FineVisitor& visit){
  const int x0=std::clamp(x,0,terrain.width),z0=std::clamp(z,0,terrain.height),x1=int(std::clamp(std::int64_t(x)+w,std::int64_t(0),std::int64_t(terrain.width))),z1=int(std::clamp(std::int64_t(z)+h,std::int64_t(0),std::int64_t(terrain.height)));
  if(x1<=x0||z1<=z0)return;
  for(int row=z0;row<z1;++row){const auto begin=std::uint64_t(row)*terrain.width+x0,end=std::uint64_t(row)*terrain.width+x1;for(auto it=terrain.fine_patches.lower_bound(begin);it!=terrain.fine_patches.end()&&it->first<end;++it)visit(int(it->first%terrain.width),row);}
 };
 view.cell=[&terrain](int x,int z){auto c=terrain.at(x,z);return SurfaceCell{terrain.height_for(c.kind)/1000.f,std::uint8_t(c.kind),c.theme,!terrain.editable(x,z),sonn::terrain_is_water(c.kind)};};
 view.point=[&terrain](double x,double z){auto p=terrain.support_um(std::int64_t(std::llround(x*1000000)),std::int64_t(std::llround(z*1000000)));if(!p)return SurfaceCell{-20,0,0,true,true};return SurfaceCell{p->height_mm/1000.f,std::uint8_t(p->kind),p->theme,!p->editable,p->wet};};
 view.coverage_counts=[&terrain](int x,int z){return terrain.coverage_counts(x,z);};
 view.fine=[&terrain](int x,int z){auto cells=terrain.fine_cells(x,z);std::vector<SurfaceCell> result;result.reserve(cells.size());for(auto c:cells)result.push_back({terrain.height_for(c.kind)/1000.f,std::uint8_t(c.kind),c.theme,!terrain.editable(x,z),sonn::terrain_is_water(c.kind)});return result;};
 view.mesh=[&terrain](int x,int z,int w,int h){return terrain.mesh(x,z,w,h);};view.coarse_mesh=[&terrain](int x,int z,int w,int h){return terrain.mesh_coarse(x,z,w,h);};
 view.raycast=[&terrain](const Ray& ray)->std::optional<SurfaceHit>{auto h=terrain.raycast({ray.origin.x,ray.origin.y,ray.origin.z},{ray.direction.x,ray.direction.y,ray.direction.z});if(!h)return {};return SurfaceHit{{h->position.x,h->position.y,h->position.z},{h->normal.x,h->normal.y,h->normal.z},h->cell_x,h->cell_z};};return view;
}

struct Client::Impl {
 struct Capture final:bgfx::CallbackI {
  std::string error;
  void fatal(const char* path,std::uint16_t line,bgfx::Fatal::Enum code,const char* message)override{SDL_Log("bgfx fatal %s:%u: %s",path,unsigned(line),message);if(code!=bgfx::Fatal::DebugCheck)std::abort();}
  void traceVargs(const char*,std::uint16_t,const char* format,va_list args)override{std::vfprintf(stderr,format,args);}
  void profilerBegin(const char*,std::uint32_t,const char*,std::uint16_t)override{}
  void profilerBeginLiteral(const char*,std::uint32_t,const char*,std::uint16_t)override{}
  void profilerEnd()override{}
  std::uint32_t cacheReadSize(std::uint64_t)override{return 0;}
  bool cacheRead(std::uint64_t,void*,std::uint32_t)override{return false;}
  void cacheWrite(std::uint64_t,const void*,std::uint32_t)override{}
  void captureBegin(std::uint32_t,std::uint32_t,std::uint32_t,bgfx::TextureFormat::Enum,bool)override{error="Video capture is not implemented";}
  void captureEnd()override{}
  void captureFrame(const void*,std::uint32_t)override{}
  // Only serialize pixels supplied by the actual GPU callback. No CPU rasterizer.
  void screenShot(const char* basename,std::uint32_t w,std::uint32_t h,std::uint32_t pitch,bgfx::TextureFormat::Enum format,const void* pixels,std::uint32_t bytes,bool bottom_up)override{
   try{if(!w||!h||w>65535||h>65535||pitch<w*4||std::uint64_t(pitch)*h>bytes||
      (format!=bgfx::TextureFormat::RGBA8&&format!=bgfx::TextureFormat::BGRA8))throw std::runtime_error("Unsupported native GPU screenshot format");
    auto path=std::filesystem::u8path(std::string(basename)+".tga");std::ofstream file(path,std::ios::binary);if(!file)throw std::runtime_error("Cannot write GPU screenshot");
    std::array<std::uint8_t,18> header{};header[2]=2;header[12]=std::uint8_t(w);header[13]=std::uint8_t(w>>8);header[14]=std::uint8_t(h);header[15]=std::uint8_t(h>>8);header[16]=32;header[17]=40;file.write(reinterpret_cast<const char*>(header.data()),header.size());
    auto* source=static_cast<const std::uint8_t*>(pixels);std::vector<std::uint8_t> row(w*4);
    for(std::uint32_t y=0;y<h;++y){auto* input=source+(bottom_up?h-1-y:y)*pitch;std::memcpy(row.data(),input,w*4);if(format==bgfx::TextureFormat::RGBA8)for(std::uint32_t x=0;x<w;++x)std::swap(row[x*4],row[x*4+2]);file.write(reinterpret_cast<const char*>(row.data()),row.size());}
    if(!file)throw std::runtime_error("GPU screenshot write incomplete");
   }catch(const std::exception& e){error=e.what();}
  }
 } capture;
 SDL_Window* window{};SDL_AudioStream* audio{};std::string audio_message;bool sdl{},gpu{},typing{},drag{},orbit{};float volume{.7f};Camera camera;Dimensions size;ClientConfig config;int gpu_width{},gpu_height{};
 bgfx::TextureHandle map_texture=BGFX_INVALID_HANDLE;std::uint64_t map_revision=~0ULL;bool map_visible{};int map_x{},map_y{},map_w{},map_h{};
 bgfx::TextureHandle water_depth=BGFX_INVALID_HANDLE,fine_atlas=BGFX_INVALID_HANDLE,fine_pages=BGFX_INVALID_HANDLE;
 sonn::Uuid depth_identity{};std::uint64_t depth_revision=~0ULL,depth_uploads{};
 std::array<float,4> depth_domain{},fine_domain{};std::array<float,8> surface_heights{};std::uint32_t surface_profile{};
 static constexpr std::size_t geometry_limit=64U*1024U*1024U,frame_upload_limit=8U*1024U*1024U;
 static constexpr int detail_cells=16,page_cells=128,atlas_slots=1024,atlas_side=2048;
 std::vector<std::uint8_t> overview_data,page_data;std::set<std::pair<int,int>> raised_regions,raised_fine_regions;
 std::map<std::uint64_t,int> fine_slots;std::array<std::uint64_t,atlas_slots> slot_keys{};
 int page_origin_x{},page_origin_z{};std::uint64_t fine_texture_uploads{},fine_patch_candidates{},fine_patch_deferred{},fine_page_uploads{},geometry_evictions{},frame_upload_bytes{},frame_texture_upload_bytes{},frame_decoration_samples{},frame_decoration_chunks{},frame_counter{};Buffer land_overview;
 std::set<Rml::Input::KeyIdentifier> pressed_keys;
 MaterialDiagnostic diagnostic=MaterialDiagnostic::Full;Age age=Age::Light;double seconds{};
 bgfx::ProgramHandle world=BGFX_INVALID_HANDLE,sky=BGFX_INVALID_HANDLE,water=BGFX_INVALID_HANDLE,shadow=BGFX_INVALID_HANDLE,ui=BGFX_INVALID_HANDLE;
 bgfx::VertexLayout world_layout,screen_layout,ui_layout;
 bgfx::UniformHandle base_u=BGFX_INVALID_HANDLE,mean_u=BGFX_INVALID_HANDLE,shadow_u=BGFX_INVALID_HANDLE,ui_u=BGFX_INVALID_HANDLE,eye_u=BGFX_INVALID_HANDLE,light_u=BGFX_INVALID_HANDLE,environment_u=BGFX_INVALID_HANDLE,light_matrix_u=BGFX_INVALID_HANDLE,forward_u=BGFX_INVALID_HANDLE,right_u=BGFX_INVALID_HANDLE,up_u=BGFX_INVALID_HANDLE,water_depth_u=BGFX_INVALID_HANDLE,water_domain_u=BGFX_INVALID_HANDLE,fine_atlas_u=BGFX_INVALID_HANDLE,fine_pages_u=BGFX_INVALID_HANDLE,fine_domain_u=BGFX_INVALID_HANDLE,surface_mode_u=BGFX_INVALID_HANDLE,menu_u=BGFX_INVALID_HANDLE;
 std::array<bgfx::TextureHandle,1> textures{},flat{};bgfx::TextureHandle white=BGFX_INVALID_HANDLE,shadow_texture=BGFX_INVALID_HANDLE;bgfx::FrameBufferHandle shadow_frame=BGFX_INVALID_HANDLE;Buffer ocean;sonn::Hash material_hash{};double depth_cell_m{.25};std::uint64_t material_uploads{};bool presenting_menu{};bgfx::VertexBufferHandle screen_vb=BGFX_INVALID_HANDLE;bgfx::IndexBufferHandle screen_ib=BGFX_INVALID_HANDLE;
 struct Chunk{Buffer buffer,decoration;double x{},z{};std::size_t decoration_quads{};std::uint64_t last_frame{};};std::map<std::pair<int,int>,Chunk> chunks,far_chunks;sonn::Uuid mesh_identity{};std::uint64_t mesh_revision=~0ULL,mesh_uploads{},mesh_cache_resets{};
 struct UiGeometry{bgfx::VertexBufferHandle vb;bgfx::IndexBufferHandle ib;bgfx::TextureHandle texture;};std::set<UiGeometry*> ui_geometry;std::set<std::uint16_t> ui_textures;
 bool scissor{},transformed{};int sx{},sy{},sw{},sh{};std::array<float,16> transform{};float light_matrix[16]{};
 struct System final:Rml::SystemInterface{double GetElapsedTime()override{return SDL_GetTicksNS()/1e9;}bool LogMessage(Rml::Log::Type,Rml::String const& message)override{SDL_Log("RmlUi: %s",message.c_str());return true;}void SetClipboardText(const Rml::String& text)override{SDL_SetClipboardText(text.c_str());}void GetClipboardText(Rml::String& text)override{char* p=SDL_GetClipboardText();text=p?p:"";SDL_free(p);}} system;
 struct Files final:Rml::FileInterface{Rml::FileHandle Open(const Rml::String& path)override{auto p=std::filesystem::u8path(path);
#ifdef _WIN32
  auto* f=_wfopen(p.c_str(),L"rb");
#else
  auto* f=std::fopen(p.c_str(),"rb");
#endif
  return reinterpret_cast<Rml::FileHandle>(f);}void Close(Rml::FileHandle h)override{std::fclose(reinterpret_cast<FILE*>(h));}std::size_t Read(void* p,std::size_t n,Rml::FileHandle h)override{return std::fread(p,1,n,reinterpret_cast<FILE*>(h));}bool Seek(Rml::FileHandle h,long n,int origin)override{return std::fseek(reinterpret_cast<FILE*>(h),n,origin)==0;}std::size_t Tell(Rml::FileHandle h)override{auto n=std::ftell(reinterpret_cast<FILE*>(h));return n<0?0:std::size_t(n);}} files;
 struct Render final:Rml::RenderInterface {
  Impl& p;explicit Render(Impl& x):p(x){}
  Rml::CompiledGeometryHandle CompileGeometry(Rml::Vertex* v,int n,int* ix,int count,Rml::TextureHandle t)override{if(n<=0||count<=0)return 0;std::vector<UiVertex> vertices;vertices.reserve(n);for(int k=0;k<n;++k)vertices.push_back({v[k].position.x,v[k].position.y,0,v[k].tex_coord.x,v[k].tex_coord.y,rgba(v[k].colour.red,v[k].colour.green,v[k].colour.blue,v[k].colour.alpha)});auto* g=new UiGeometry{bgfx::createVertexBuffer(bgfx::copy(vertices.data(),std::uint32_t(vertices.size()*sizeof(UiVertex))),p.ui_layout),bgfx::createIndexBuffer(bgfx::copy(ix,std::uint32_t(count*4)),BGFX_BUFFER_INDEX32),t?bgfx::TextureHandle{std::uint16_t(t-1)}:p.white};if(!bgfx::isValid(g->vb)||!bgfx::isValid(g->ib)){if(bgfx::isValid(g->vb))bgfx::destroy(g->vb);if(bgfx::isValid(g->ib))bgfx::destroy(g->ib);delete g;return 0;}p.ui_geometry.insert(g);return reinterpret_cast<Rml::CompiledGeometryHandle>(g);}
  void RenderGeometry(Rml::Vertex* v,int n,int* ix,int count,Rml::TextureHandle texture,const Rml::Vector2f& t)override{auto g=CompileGeometry(v,n,ix,count,texture);if(g){RenderCompiledGeometry(g,t);ReleaseCompiledGeometry(g);}}
  void RenderCompiledGeometry(Rml::CompiledGeometryHandle h,const Rml::Vector2f& t)override{auto* g=reinterpret_cast<UiGeometry*>(h);if(!p.ui_geometry.count(g))return;if(p.scissor){int x=std::clamp(p.sx,0,p.size.pixel_w),y=std::clamp(p.sy,0,p.size.pixel_h),r=std::clamp(p.sx+p.sw,0,p.size.pixel_w),b=std::clamp(p.sy+p.sh,0,p.size.pixel_h);if(r<=x||b<=y)return;bgfx::setScissor(std::uint16_t(x),std::uint16_t(y),std::uint16_t(r-x),std::uint16_t(b-y));}else bgfx::setScissor(UINT16_MAX);float model[16];if(p.transformed)std::copy(p.transform.begin(),p.transform.end(),model);else bx::mtxIdentity(model);for(int r=0;r<4;++r)model[12+r]+=model[r]*t.x+model[4+r]*t.y;bgfx::setTransform(model);bgfx::setVertexBuffer(0,g->vb);bgfx::setIndexBuffer(g->ib);bgfx::setTexture(0,p.ui_u,g->texture);bgfx::setState(BGFX_STATE_WRITE_RGB|BGFX_STATE_WRITE_A|BGFX_STATE_MSAA|BGFX_STATE_BLEND_FUNC_SEPARATE(BGFX_STATE_BLEND_SRC_ALPHA,BGFX_STATE_BLEND_INV_SRC_ALPHA,BGFX_STATE_BLEND_ONE,BGFX_STATE_BLEND_INV_SRC_ALPHA));bgfx::submit(3,p.ui);}
  void ReleaseCompiledGeometry(Rml::CompiledGeometryHandle h)override{auto* g=reinterpret_cast<UiGeometry*>(h);if(p.ui_geometry.erase(g)){bgfx::destroy(g->vb);bgfx::destroy(g->ib);delete g;}}
  void EnableScissorRegion(bool x)override{p.scissor=x;}void SetScissorRegion(int x,int y,int w,int h)override{p.sx=x;p.sy=y;p.sw=w;p.sh=h;}void SetTransform(const Rml::Matrix4f* m)override{p.transformed=m;if(m)std::copy(m->data(),m->data()+16,p.transform.begin());}
  bool generate_texture(Rml::TextureHandle& handle,const Rml::byte* data,const Rml::Vector2i& dimensions,bool nearest){if(dimensions.x<=0||dimensions.y<=0||dimensions.x>16384||dimensions.y>16384)return false;auto t=bgfx::createTexture2D(std::uint16_t(dimensions.x),std::uint16_t(dimensions.y),false,1,bgfx::TextureFormat::RGBA8,BGFX_SAMPLER_U_CLAMP|BGFX_SAMPLER_V_CLAMP|(nearest?BGFX_SAMPLER_MIN_POINT|BGFX_SAMPLER_MAG_POINT:0),bgfx::copy(data,std::uint32_t(dimensions.x*dimensions.y*4)));if(!bgfx::isValid(t))return false;p.ui_textures.insert(t.idx);handle=t.idx+1;return true;}
  bool GenerateTexture(Rml::TextureHandle& h,const Rml::byte* d,const Rml::Vector2i& size)override{return generate_texture(h,d,size,false);}
  bool LoadTexture(Rml::TextureHandle& handle,Rml::Vector2i& dimensions,const Rml::String& source)override{try{auto path=std::filesystem::u8path(source);if(path.is_relative())path=p.config.resources/path;auto data=read(path);bx::DefaultAllocator allocator;auto* image=bimg::imageParse(&allocator,data.data(),std::uint32_t(data.size()),bimg::TextureFormat::RGBA8);if(!image)return false;dimensions={int(image->m_width),int(image->m_height)};bool result=false;if(path.parent_path().filename()=="icons"){auto t=bgfx::createTexture2D(std::uint16_t(dimensions.x),std::uint16_t(dimensions.y),false,1,bgfx::TextureFormat::RGBA8,BGFX_SAMPLER_MIN_POINT|BGFX_SAMPLER_MAG_POINT|BGFX_SAMPLER_MIP_POINT|BGFX_SAMPLER_U_CLAMP|BGFX_SAMPLER_V_CLAMP,bgfx::copy(image->m_data,std::uint32_t(dimensions.x*dimensions.y*4)));result=bgfx::isValid(t);if(result){p.ui_textures.insert(t.idx);handle=t.idx+1;}}else result=GenerateTexture(handle,static_cast<const Rml::byte*>(image->m_data),dimensions);bimg::imageFree(image);return result;}catch(const std::exception& e){SDL_Log("Texture load: %s",e.what());return false;}}
  void ReleaseTexture(Rml::TextureHandle h)override{if(h&&p.ui_textures.erase(std::uint16_t(h-1)))bgfx::destroy(bgfx::TextureHandle{std::uint16_t(h-1)});}
 } render;
 Impl():render(*this){for(auto& t:textures)t=BGFX_INVALID_HANDLE;for(auto& t:flat)t=BGFX_INVALID_HANDLE;}
 ~Impl(){if(gpu){for(auto* g:ui_geometry){bgfx::destroy(g->vb);bgfx::destroy(g->ib);delete g;}for(auto h:ui_textures)bgfx::destroy(bgfx::TextureHandle{h});for(auto& [key,c]:chunks){release(c.buffer);release(c.decoration);}for(auto& [key,c]:far_chunks){release(c.buffer);release(c.decoration);}release(land_overview);release(ocean);if(bgfx::isValid(screen_vb))bgfx::destroy(screen_vb);if(bgfx::isValid(screen_ib))bgfx::destroy(screen_ib);for(auto t:textures)if(bgfx::isValid(t))bgfx::destroy(t);for(auto t:flat)if(bgfx::isValid(t))bgfx::destroy(t);if(bgfx::isValid(white))bgfx::destroy(white);if(bgfx::isValid(map_texture))bgfx::destroy(map_texture);if(bgfx::isValid(water_depth))bgfx::destroy(water_depth);if(bgfx::isValid(fine_atlas))bgfx::destroy(fine_atlas);if(bgfx::isValid(fine_pages))bgfx::destroy(fine_pages);if(bgfx::isValid(shadow_frame))bgfx::destroy(shadow_frame);for(auto p:{world,sky,water,shadow,ui})if(bgfx::isValid(p))bgfx::destroy(p);for(auto u:{base_u,mean_u,shadow_u,ui_u,eye_u,light_u,environment_u,light_matrix_u,forward_u,right_u,up_u,water_depth_u,water_domain_u,fine_atlas_u,fine_pages_u,fine_domain_u,surface_mode_u,menu_u})if(bgfx::isValid(u))bgfx::destroy(u);bgfx::frame();bgfx::shutdown();}if(audio)SDL_DestroyAudioStream(audio);if(window)SDL_DestroyWindow(window);if(sdl)SDL_Quit();}
 // Scene rectangles are SDL logical coordinates. Rendering and picking share
 // this exact rectangle; physical pixels are only used by the GPU/UI adapter.
 std::array<int,4> scene_rect() const {
  int x=std::clamp(map_x,0,std::max(0,size.logical_w-1));
  int y=std::clamp(map_y,0,std::max(0,size.logical_h-1));
  if(map_w<=0||map_h<=0)return {0,0,std::max(1,size.logical_w),std::max(1,size.logical_h)};
  return {x,y,std::max(1,std::min(map_w,size.logical_w-x)),std::max(1,std::min(map_h,size.logical_h-y))};
 }
 std::array<int,4> scene_pixels() const {
  auto r=scene_rect();double dx=double(size.pixel_w)/std::max(1,size.logical_w);
  double dy=double(size.pixel_h)/std::max(1,size.logical_h);
  int x=int(std::lround(r[0]*dx)),y=int(std::lround(r[1]*dy));
  int right=std::min(size.pixel_w,int(std::lround((r[0]+r[2])*dx)));
  int bottom=std::min(size.pixel_h,int(std::lround((r[1]+r[3])*dy)));
  return {x,y,std::max(1,right-x),std::max(1,bottom-y)};
 }
 void update_dimensions(){SDL_GetWindowSize(window,&size.logical_w,&size.logical_h);SDL_GetWindowSizeInPixels(window,&size.pixel_w,&size.pixel_h);size.density=size.logical_w>0?float(size.pixel_w)/size.logical_w:1;}
 const Camera& scene_camera() const{return camera;}
 void pixel_materials(){
  constexpr int side=128,layers=32;
  using Color=std::array<int,3>;
  // Original painted colour clusters: each2m tile has128 real pixels. Fine
  // blades, mineral streaks and larger colour islands form a hierarchy instead
  // of enlarging sixteen random squares. This is colour art, never geometry.
  const std::array<Color,24> base={{{225,238,240},{81,150,105},{167,76,49},{79,130,99},
   {60,121,114},{163,109,0},{121,170,120},{61,51,65},
   {147,160,172},{142,99,65},{130,85,61},{134,93,86},
   {87,94,72},{154,122,78},{140,121,80},{84,58,56},
   {255,170,0},{184,123,0},{150,157,172},{115,123,143},
   {229,240,241},{130,139,155},{70,105,114},{83,103,118}}};
  std::vector<std::uint8_t> data(side*side*layers*4),mean(layers*4);
  for(int layer=0;layer<layers;++layer){std::array<unsigned,3> sum{};
   for(int y=0;y<side;++y)for(int x=0;x<side;++x){
    Color color=layer<24?base[layer]:Color{255,255,255};
    if(layer<24){
     auto large=hash(std::uint32_t(x/23+(y/19)*113+layer*7193));
     auto medium=hash(std::uint32_t(x/7+(y/5)*131+layer*11717));
     auto fine=hash(std::uint32_t(x/2+(y/3)*197+layer*19139));
     int shade=int(large%15)-7+int(medium%11)-5+int(fine%7)-3;
     if(layer<8){
      // Soil motifs use coherent flora hues, rather than differently tinted
      // copies of a rock-noise texture. Pink blossoms are sparse in green grass.
      bool blade=(hash(std::uint32_t(x/4+(y/8)*193+layer*97))%11==0)&&(x%4==1||x%4==2)&&(y%8<5);
      if(blade){shade+=10;color[1]+=6;}
      bool bloom=layer==1||layer==3||layer==6;
      if(bloom&&hash(std::uint32_t(x/13+(y/11)*337+layer*101))%19==0&&x%13<2&&y%11<2){
       color=layer==3?Color{234,179,192}:Color{255,170,0};shade=0;
      }
      if(layer==0){shade=shade/2;if((x+y*2)%59==0)color={187,213,229};}
      if(layer==2&&medium%7==0){color={194,112,67};shade/=2;}
      if(layer==7){shade/=2;if((x/3+y/5)%43==0)color={167,80,52};}
     }else if(layer<16){
      shade+=(y/7)%3==0?-8:2;
      if(y<3){color=base[layer-8];shade=4;}
      if(hash(std::uint32_t(x/6+y/4*331+layer*73))%23==0)shade-=17;
     }else if(layer==16||layer==17){shade=shade/2+((x/4+2*(y/7))%31==0?7:0);}
     else if(layer==18||layer==19||layer==21||layer==23){shade+=(x/13+2*(y/9))%13==0?-13:0;}
     for(int k=0;k<3;++k)color[k]=std::clamp(color[k]+shade,0,255);
    }
    auto offset=std::size_t(layer*side*side+y*side+x)*4;
    for(int k=0;k<3;++k){data[offset+k]=std::uint8_t(color[k]);sum[k]+=unsigned(color[k]);}data[offset+3]=255;
   }
   for(int k=0;k<3;++k)mean[layer*4+k]=std::uint8_t(sum[k]/(side*side));mean[layer*4+3]=255;
  }
  constexpr std::uint64_t point=BGFX_SAMPLER_MIN_POINT|BGFX_SAMPLER_MAG_POINT|BGFX_SAMPLER_MIP_POINT|BGFX_SAMPLER_U_CLAMP|BGFX_SAMPLER_V_CLAMP;
  textures[0]=bgfx::createTexture2D(side,side*layers,false,1,bgfx::TextureFormat::RGBA8,point,bgfx::copy(data.data(),std::uint32_t(data.size())));
  flat[0]=bgfx::createTexture2D(1,layers,false,1,bgfx::TextureFormat::RGBA8,point,bgfx::copy(mean.data(),std::uint32_t(mean.size())));
  if(!bgfx::isValid(textures[0])||!bgfx::isValid(flat[0]))throw std::runtime_error("Original fine pixel atlas upload failed");
  material_hash=sonn::sha256(data);++material_uploads;
 }
 void detail_quad(std::vector<Vertex>& vertices,std::vector<std::uint32_t>& indices,const std::array<Vec3,4>& points,Vec3 normal,std::array<int,3> color,float phase,float weight=1){
  auto base=std::uint32_t(vertices.size());for(int k=0;k<4;++k){auto a=points[k];vertices.push_back({float(a.x),float(a.y),float(a.z),float(normal.x),float(normal.y),float(normal.z),phase,(k==1||k==2)?weight:0,rgba(color[0],color[1],color[2],int(std::lround(25*255.f/31)))});}
  for(auto k:{0U,1U,2U,0U,2U,3U})indices.push_back(base+k);
 }
 Buffer ground_decoration(const SurfaceView& s,int startx,int startz,int w,int h,double ox,double oz,std::size_t& quads,std::size_t maximum_bytes=SIZE_MAX){
  std::vector<Vertex> vertices;std::vector<std::uint32_t> indices;
  // Stable positions in physical space, not one plant per cell. No Actor,
  // TreeRef, food, collision, picking or simulation RNG is created by this art.
  double left=s.origin_x+startx*s.cell_m,right=left+w*s.cell_m;
  double front=s.origin_z+startz*s.cell_m,back=front+h*s.cell_m;
  for(int z=int(std::ceil(front*2));z<int(std::ceil(back*2));++z)for(int x=int(std::ceil(left*2));x<int(std::ceil(right*2));++x){
   ++frame_decoration_samples;auto n=hash(std::uint32_t(x)*73856093U^std::uint32_t(z)*19349663U);
   if(n%9>1)continue;double px=x*.5+.15+double((n>>8)%11)*.018,pz=z*.5+.13+double((n>>16)%11)*.018;
   if(px<left||pz<front||px>=right||pz>=back)continue;
   int cx=int(std::floor((px-s.origin_x)/s.cell_m)),cz=int(std::floor((pz-s.origin_z)/s.cell_m));auto cell=s.point?s.point(px,pz):s.cell(cx,cz);
   if(cell.guard||cell.kind!=std::uint8_t(sonn::TerrainKind::Soil)||cell.theme==0||cell.theme==7)continue;
   double a=px-ox,b=pz-oz,y=cell.height_m+.005,height=.11+double((n>>24)%7)*.013;
   if((vertices.size()+12)*sizeof(Vertex)+(indices.size()+18)*sizeof(std::uint32_t)>maximum_bytes){quads=vertices.size()/4;return upload(vertices,indices,world_layout);}
   std::array<int,3> grass=cell.theme==2?std::array<int,3>{173,93,58}:cell.theme==5?std::array<int,3>{171,114,0}:std::array<int,3>{113,172,115};
   float phase=float(n%1024)*.02f;
   detail_quad(vertices,indices,{{{a-.022,y,b},{a-.011,y+height,b+.009},{a+.016,y+height*.76,b+.015},{a+.022,y,b}}},{0,.8,.2},grass,phase);
   detail_quad(vertices,indices,{{{a,y,b-.023},{a+.008,y+height*.86,b-.012},{a+.014,y+height*.69,b+.018},{a,y,b+.024}}},{.2,.8,0},grass,phase+.7f);
   bool flower=(cell.theme==1||cell.theme==3||cell.theme==6)&&n%31==0;
   if(flower){std::array<int,3> petals=cell.theme==3?std::array<int,3>{249,166,183}:n%2?std::array<int,3>{255,170,0}:std::array<int,3>{193,160,232};
    detail_quad(vertices,indices,{{{a-.042,y+height,b-.032},{a-.042,y+height+.014,b+.032},{a+.042,y+height+.014,b+.032},{a+.042,y+height,b-.032}}},{0,1,0},petals,phase);
   }
  }
  quads=vertices.size()/4;if(vertices.size()*sizeof(Vertex)+indices.size()*4>maximum_bytes){quads=0;return {};}return upload(vertices,indices,world_layout);
 }
 std::size_t buffer_bytes(const Buffer& buffer) const{return buffer.vertices*sizeof(Vertex)+buffer.indices*sizeof(std::uint32_t);}
 std::size_t resident_bytes() const {std::size_t total=0;for(const auto& [key,c]:chunks)total+=buffer_bytes(c.buffer)+buffer_bytes(c.decoration);return total;}
 Buffer geometry(const sonn::Mesh& mesh,double ox,double oz,bool positive_only){
  if(mesh.vertices.size()%4)throw std::runtime_error("Core compact surface quad contract changed");std::vector<Vertex> vertices;std::vector<std::uint32_t> indices;
  for(std::size_t face=0;face<mesh.vertices.size();face+=4){auto n=mesh.vertices[face];if(n.wet)continue;
   if(positive_only&&surface_heights[n.material]<=0)continue;
   if(!positive_only&&surface_heights[n.material]!=0)continue;
   bool top=n.ny>.7f;int layer=int(n.theme)+(top?0:8);
   if(n.material==std::uint8_t(sonn::TerrainKind::Sand))layer=top?16:17;
   else if(n.material==std::uint8_t(sonn::TerrainKind::Hill)||n.material==std::uint8_t(sonn::TerrainKind::Mountain))layer=top?18:19;
   else if(n.material==std::uint8_t(sonn::TerrainKind::HighPeak))layer=top?20:21;
   auto base=std::uint32_t(vertices.size());for(int k=0;k<4;++k){auto v=mesh.vertices[face+k];vertices.push_back({float(v.x-ox),v.y,float(v.z-oz),v.nx,v.ny,v.nz,v.x/2.f,v.z/2.f,rgba(255,255,255,int(std::lround(layer*255.f/31)))});}
   for(auto k:{0U,1U,2U,0U,2U,3U})indices.push_back(base+k);
  }
  std::size_t bytes=vertices.size()*sizeof(Vertex)+indices.size()*4;if(bytes>geometry_limit)throw std::runtime_error("Exact surface region exceeds bounded GPU geometry budget");return upload(vertices,indices,world_layout);
 }
 // RGBA8 stores exact 0..4096 dry-column count, not rounded 8-bit alpha.
 std::array<std::uint8_t,4> coverage_descriptor(unsigned kind,unsigned theme,unsigned dry_count,unsigned wet_kind) const {return {std::uint8_t(kind*8+theme),std::uint8_t(dry_count&255U),std::uint8_t((dry_count>>8)|(wet_kind<<5)),255};}
 std::array<std::uint8_t,4> descriptor(const SurfaceCell& c) const {return coverage_descriptor(c.wet?unsigned(sonn::TerrainKind::Sand):c.kind,c.wet?0:c.theme,c.wet?0:4096,c.wet?c.kind:unsigned(sonn::TerrainKind::ShallowWater));}
 void prepare_water_depth(const SurfaceView& s){
  if(bgfx::isValid(water_depth)&&depth_identity==s.identity&&depth_revision==s.revision)return;
  if(!s.cell||!s.enumerate_fine||!s.coverage_counts||s.width<=0||s.height<=0||s.width>4128||s.height>4128||s.cell_m<=0)throw std::runtime_error("Surface overview requires exact bounded management geometry");
  for(auto& [key,c]:chunks){release(c.buffer);release(c.decoration);}chunks.clear();for(auto& [key,c]:far_chunks){release(c.buffer);release(c.decoration);}far_chunks.clear();raised_regions.clear();raised_fine_regions.clear();fine_slots.clear();slot_keys.fill(~0ULL);++mesh_cache_resets;
  page_data.clear();mesh_identity=s.identity;mesh_revision=s.revision;surface_heights=s.height_levels;surface_profile=s.profile;overview_data.resize(std::size_t(s.width)*s.height*4);
  for(int z=0;z<s.height;++z)for(int x=0;x<s.width;++x){auto c=s.cell(x,z);auto info=descriptor(c);std::copy(info.begin(),info.end(),overview_data.begin()+(std::size_t(z)*s.width+x)*4);if(!c.wet&&c.height_m>0)raised_regions.emplace(x/64,z/64);}
  // Small unresolved islands still contribute their actual dry fraction to
  // the overview; they are never replaced by the coarse centre's water kind.
  s.enumerate_fine(0,0,s.width,s.height,[&](int x,int z){auto counts=s.coverage_counts(x,z);std::array<unsigned,64> dry{},wet{};unsigned dry_count=0,wet_count=0;
   // Exact palette counts avoid4096 expanded samples for every distant coast
   // patch. Binary codecs aggregate64 decoded rows using integer popcount.
   for(std::size_t slot=0;slot<counts.size();++slot){unsigned count=counts[slot],kind=unsigned(slot/8);if(!count)continue;if(sonn::terrain_is_water(sonn::TerrainKind(kind))){wet[kind]+=count;wet_count+=count;}else{dry[slot]+=count;dry_count+=count;if(surface_heights[kind]>0){raised_regions.emplace(x/64,z/64);raised_fine_regions.emplace(x/64,z/64);}}}
   if(dry_count+wet_count!=4096)throw std::runtime_error("Authoritative fine coverage count must contain exactly4096 columns");
   auto dry_i=std::size_t(std::max_element(dry.begin(),dry.end())-dry.begin());auto wet_i=std::size_t(std::max_element(wet.begin(),wet.end())-wet.begin());
   auto offset=(std::size_t(z)*s.width+x)*4;auto info=coverage_descriptor(dry_count?unsigned(dry_i/8):unsigned(sonn::TerrainKind::Sand),unsigned(dry_i%8),dry_count,wet_count?unsigned(wet_i):unsigned(sonn::TerrainKind::ShallowWater));std::copy(info.begin(),info.end(),overview_data.begin()+offset);
  });
  constexpr std::uint64_t point=BGFX_SAMPLER_MIN_POINT|BGFX_SAMPLER_MAG_POINT|BGFX_SAMPLER_MIP_POINT|BGFX_SAMPLER_U_CLAMP|BGFX_SAMPLER_V_CLAMP;
  auto texture=bgfx::createTexture2D(std::uint16_t(s.width),std::uint16_t(s.height),false,1,bgfx::TextureFormat::RGBA8,point,bgfx::copy(overview_data.data(),std::uint32_t(overview_data.size())));
  if(!bgfx::isValid(texture))throw std::runtime_error("Complete authoritative coverage overview GPU upload failed");if(bgfx::isValid(water_depth))bgfx::destroy(water_depth);water_depth=texture;
  depth_identity=s.identity;depth_revision=s.revision;depth_cell_m=s.cell_m;++depth_uploads;depth_domain={float(s.origin_x),float(s.origin_z),float(s.width*s.cell_m),float(s.height*s.cell_m)};
  if(!bgfx::isValid(fine_atlas))fine_atlas=bgfx::createTexture2D(atlas_side,atlas_side,false,1,bgfx::TextureFormat::RGBA8,point);
  if(!bgfx::isValid(fine_pages))fine_pages=bgfx::createTexture2D(page_cells,page_cells,false,1,bgfx::TextureFormat::RGBA8,point);
  if(!bgfx::isValid(fine_atlas)||!bgfx::isValid(fine_pages))throw std::runtime_error("Bounded fine coverage atlas allocation failed");
  release(land_overview);std::vector<Vertex> v={ {0,0,0,0,1,0,0,0,0xffffffff},{float(s.width*s.cell_m),0,0,0,1,0,0,0,0xffffffff},{float(s.width*s.cell_m),0,float(s.height*s.cell_m),0,1,0,0,0,0xffffffff},{0,0,float(s.height*s.cell_m),0,1,0,0,0,0xffffffff} };land_overview=upload(v,{0,1,2,0,2,3},world_layout);
  std::size_t far_bytes=0;
  for(auto key:raised_regions){int x=key.first*64,z=key.second*64;double ox=s.origin_x+x*s.cell_m,oz=s.origin_z+z*s.cell_m;
   auto mesh=raised_fine_regions.count(key)?s.mesh(x,z,std::min(64,s.width-x),std::min(64,s.height-z)):s.coarse_mesh(x,z,std::min(64,s.width-x),std::min(64,s.height-z));auto buffer=geometry(mesh,ox,oz,true);far_bytes+=buffer_bytes(buffer);if(far_bytes>geometry_limit){release(buffer);throw std::runtime_error("Global exact elevated terrain exceeds GPU budget; no partial world is displayed");}far_chunks.emplace(key,Chunk{buffer,{},ox,oz,0,0});++mesh_uploads;
  }
 }
 double projected_cell_pixels(const SurfaceView& s,int x,int z,double bounds_padding=0) const{
  auto b=basis(camera);Vec3 point{s.origin_x+(x+.5)*s.cell_m,0,s.origin_z+(z+.5)*s.cell_m};auto d=subtract(point,b.eye);double depth=dot(d,b.forward);if(depth<=.1)return 0;
  double tangent=std::tan(25*pi/180),aspect=double(scene_rect()[2])/scene_rect()[3];if(std::abs(dot(d,b.right))>depth*tangent*aspect+s.cell_m*2+bounds_padding||std::abs(dot(d,b.up))>depth*tangent+s.cell_m*2+bounds_padding)return 0;
  return s.cell_m*size.pixel_h/(2*tangent*depth);
 }
 void prepare_fine_coverage(const SurfaceView& s){
  page_origin_x=int(std::floor((camera.target.x-s.origin_x)/s.cell_m/16))*16-page_cells/2;page_origin_z=int(std::floor((camera.target.z-s.origin_z)/s.cell_m/16))*16-page_cells/2;
  fine_domain={float(s.origin_x+page_origin_x*s.cell_m),float(s.origin_z+page_origin_z*s.cell_m),float(page_cells),float(s.cell_m)};
  std::vector<std::pair<double,std::uint64_t>> wanted;
  s.enumerate_fine(page_origin_x,page_origin_z,page_cells,page_cells,[&](int x,int z){double pixels=projected_cell_pixels(s,x,z);if(pixels<.8)return;wanted.emplace_back(pixels,std::uint64_t(z)*s.width+x);});
  std::sort(wanted.begin(),wanted.end(),[](const auto& a,const auto& b){return a.first>b.first;});fine_patch_candidates=wanted.size();if(wanted.size()>atlas_slots)wanted.resize(atlas_slots);
  std::set<std::uint64_t> desired;for(auto entry:wanted)desired.insert(entry.second);
  for(auto it=fine_slots.begin();it!=fine_slots.end();){if(!desired.count(it->first)){slot_keys[it->second]=~0ULL;it=fine_slots.erase(it);}else ++it;}
  for(auto [pixels,key]:wanted){if(fine_slots.count(key))continue;if(frame_texture_upload_bytes+64U*64U*4U>4U*1024U*1024U)break;
   auto slot_it=std::find(slot_keys.begin(),slot_keys.end(),~0ULL);if(slot_it==slot_keys.end())break;int slot=int(slot_it-slot_keys.begin());auto fine=s.fine(int(key%s.width),int(key/s.width));if(fine.size()!=64U*64U)throw std::runtime_error("Fine coverage must contain exact64x64 authoritative samples");std::vector<std::uint8_t> pixels_data(64*64*4);
   for(std::size_t i=0;i<fine.size();++i){auto info=descriptor(fine[i]);std::copy(info.begin(),info.end(),pixels_data.begin()+i*4);}
   bgfx::updateTexture2D(fine_atlas,0,0,std::uint16_t((slot%32)*64),std::uint16_t((slot/32)*64),64,64,bgfx::copy(pixels_data.data(),std::uint32_t(pixels_data.size())));frame_texture_upload_bytes+=pixels_data.size();slot_keys[slot]=key;fine_slots.emplace(key,slot);++fine_texture_uploads;
  }
  std::vector<std::uint8_t> pages(page_cells*page_cells*4,0);
  for(auto [key,slot]:fine_slots){int x=int(key%s.width)-page_origin_x,z=int(key/s.width)-page_origin_z;if(x<0||z<0||x>=page_cells||z>=page_cells)continue;int code=slot+1;auto offset=(std::size_t(z)*page_cells+x)*4;pages[offset]=std::uint8_t(code);pages[offset+1]=std::uint8_t(code>>8);pages[offset+2]=255;pages[offset+3]=255;}
  if(pages!=page_data){bgfx::updateTexture2D(fine_pages,0,0,0,0,page_cells,page_cells,bgfx::copy(pages.data(),std::uint32_t(pages.size())));frame_texture_upload_bytes+=pages.size();++fine_page_uploads;page_data=std::move(pages);}fine_patch_deferred=fine_patch_candidates-fine_slots.size();
 }
 std::vector<std::pair<int,int>> prepare_chunks(const SurfaceView& s){
  if(!s.point)throw std::runtime_error("Fine renderer requires exact authoritative point coverage");++frame_counter;
  std::vector<std::pair<double,std::pair<int,int>>> candidates;
  int cx=int(std::floor((camera.target.x-s.origin_x)/(detail_cells*s.cell_m))),cz=int(std::floor((camera.target.z-s.origin_z)/(detail_cells*s.cell_m)));
  int radius=std::clamp(int(std::ceil(camera.distance_m*1.3/(detail_cells*s.cell_m)))+2,2,10);
  for(int z=std::max(0,cz-radius);z<=std::min((s.height-1)/detail_cells,cz+radius);++z)for(int x=std::max(0,cx-radius);x<=std::min((s.width-1)/detail_cells,cx+radius);++x){int px=x*detail_cells+detail_cells/2,pz=z*detail_cells+detail_cells/2;double score=projected_cell_pixels(s,px,pz,detail_cells*s.cell_m*.75);if(score<=0)continue;double dx=x-cx,dz=z-cz;candidates.emplace_back(dx*dx+dz*dz,std::make_pair(x,z));}
  std::sort(candidates.begin(),candidates.end());if(candidates.size()>128)candidates.resize(128);std::set<std::pair<int,int>> desired;for(auto entry:candidates)desired.insert(entry.second);
  for(auto it=chunks.begin();it!=chunks.end();){if(!desired.count(it->first)){release(it->second.buffer);release(it->second.decoration);it=chunks.erase(it);++geometry_evictions;}else ++it;}
  std::vector<std::pair<int,int>> visible;
  // Soil/sand and water share exact0m. The authoritative micro-mask plane
  // below already rasterizes their true edges, material and depth. Building
  // a discarded seabed mesh would multiply work by64² per fine cell.
  for(auto [score,key]:candidates){auto existing=chunks.find(key);if(existing!=chunks.end()){visible.push_back(key);existing->second.last_frame=frame_counter;continue;}if(frame_decoration_chunks>=16||frame_upload_bytes+frame_texture_upload_bytes>=frame_upload_limit||resident_bytes()>=geometry_limit)continue;
   int x=key.first*detail_cells,z=key.second*detail_cells,w=std::min(detail_cells,s.width-x),h=std::min(detail_cells,s.height-z);double ox=s.origin_x+x*s.cell_m,oz=s.origin_z+z*s.cell_m;
   std::size_t available=std::min<std::size_t>(std::size_t(frame_upload_limit-frame_upload_bytes-frame_texture_upload_bytes),geometry_limit-resident_bytes()),count=0;
   ++frame_decoration_chunks;auto decor=ground_decoration(s,x,z,w,h,ox,oz,count,available);frame_upload_bytes+=buffer_bytes(decor);chunks.emplace(key,Chunk{{},decor,ox,oz,count,frame_counter});visible.push_back(key);++mesh_uploads;
  }
  return visible;
 }
 void surface_uniforms(float mode){float flags[4]={mode,float(depth_cell_m),0,0};float domain[4]={float(double(depth_domain[0])-camera.target.x),float(double(depth_domain[1])-camera.target.z),depth_domain[2],depth_domain[3]};float near_domain[4]={float(double(fine_domain[0])-camera.target.x),float(double(fine_domain[1])-camera.target.z),fine_domain[2],fine_domain[3]};bgfx::setUniform(surface_mode_u,flags);bgfx::setUniform(water_domain_u,domain);bgfx::setUniform(fine_domain_u,near_domain);bgfx::setTexture(4,water_depth_u,water_depth);bgfx::setTexture(5,fine_atlas_u,fine_atlas);bgfx::setTexture(6,fine_pages_u,fine_pages);}
 void environment(const Basis& b){const auto& c=scene_camera();float eye[4]={float(b.eye.x-c.target.x),float(b.eye.y),float(b.eye.z-c.target.z),age==Age::Darkness?1.f:0.f};float l[4]={-.5f,-.8f,-.35f,1};float e[4]={float(depth_cell_m),float(seconds),float(c.target.x),float(c.target.z)};bgfx::setUniform(eye_u,eye);bgfx::setUniform(light_u,l);bgfx::setUniform(environment_u,e);}
 void draw_collection(const std::map<std::pair<int,int>,Chunk>& collection,const std::vector<std::pair<int,int>>& visible,bgfx::ViewId view,bgfx::ProgramHandle program){
  const auto& c=scene_camera();for(auto key:visible){const auto& chunk=collection.at(key);if(!bgfx::isValid(chunk.buffer.vb))continue;
   float model[16];bx::mtxTranslate(model,float(chunk.x-c.target.x),0,float(chunk.z-c.target.z));bgfx::setTransform(model);bgfx::setVertexBuffer(0,chunk.buffer.vb);bgfx::setIndexBuffer(chunk.buffer.ib);environment(basis(c));
   if(view==2){surface_uniforms(0);bgfx::setUniform(light_matrix_u,light_matrix);bgfx::setTexture(0,base_u,diagnostic==MaterialDiagnostic::MeanBase?flat[0]:textures[0]);bgfx::setTexture(1,mean_u,flat[0]);bgfx::setTexture(3,shadow_u,shadow_texture);}
   bgfx::setState(BGFX_STATE_WRITE_RGB|BGFX_STATE_WRITE_A|BGFX_STATE_WRITE_Z|BGFX_STATE_DEPTH_TEST_LESS|BGFX_STATE_MSAA);bgfx::submit(view,program);
  }
 }
 void draw_decoration(const std::vector<std::pair<int,int>>& visible,bgfx::ViewId view){
  const auto& c=scene_camera();for(auto key:visible){auto& chunk=chunks.at(key);auto& decoration=chunk.decoration;if(!bgfx::isValid(decoration.vb))continue;
   float model[16];bx::mtxTranslate(model,float(chunk.x-c.target.x),0,float(chunk.z-c.target.z));bgfx::setTransform(model);bgfx::setVertexBuffer(0,decoration.vb);bgfx::setIndexBuffer(decoration.ib);environment(basis(c));
   if(view==2){surface_uniforms(0);bgfx::setUniform(light_matrix_u,light_matrix);bgfx::setTexture(0,base_u,textures[0]);bgfx::setTexture(1,mean_u,flat[0]);bgfx::setTexture(3,shadow_u,shadow_texture);}
   bgfx::setState(BGFX_STATE_WRITE_RGB|BGFX_STATE_WRITE_A|BGFX_STATE_WRITE_Z|BGFX_STATE_DEPTH_TEST_LESS|BGFX_STATE_MSAA);bgfx::submit(view,view==0?shadow:world);
  }
 }
 void draw_overview(){
  if(surface_profile!=2)return;float model[16];bx::mtxTranslate(model,depth_domain[0]-float(camera.target.x),0,depth_domain[1]-float(camera.target.z));bgfx::setTransform(model);environment(basis(camera));surface_uniforms(1);bgfx::setUniform(light_matrix_u,light_matrix);bgfx::setTexture(0,base_u,diagnostic==MaterialDiagnostic::MeanBase?flat[0]:textures[0]);bgfx::setTexture(1,mean_u,flat[0]);bgfx::setTexture(3,shadow_u,shadow_texture);bgfx::setVertexBuffer(0,land_overview.vb);bgfx::setIndexBuffer(land_overview.ib);
  // Pixel-area coverage is composited over the preceding water colour. No
  // depth offset or epsilon separates nominally equal0m ground and water.
  // Fine mask coverage is exact when resolved, so the same plane also writes
  // actual ground depth; unresolved dry/wet area shares the same0m depth.
  bgfx::setState(BGFX_STATE_WRITE_RGB|BGFX_STATE_WRITE_A|BGFX_STATE_WRITE_Z|BGFX_STATE_DEPTH_TEST_LESS|BGFX_STATE_MSAA|BGFX_STATE_BLEND_ALPHA);bgfx::submit(2,world);
 }
 void draw_water(const Basis& b,bool depth_only){
  float identity[16];bx::mtxIdentity(identity);bgfx::setTransform(identity);environment(b);surface_uniforms(0);bgfx::setUniform(light_matrix_u,light_matrix);bgfx::setTexture(3,shadow_u,shadow_texture);bgfx::setVertexBuffer(0,ocean.vb);bgfx::setIndexBuffer(ocean.ib);
  bgfx::setState((depth_only?BGFX_STATE_WRITE_Z:BGFX_STATE_WRITE_RGB|BGFX_STATE_WRITE_A)|BGFX_STATE_DEPTH_TEST_LESS|BGFX_STATE_MSAA);bgfx::submit(2,water);
 }



};
Client::Client(const ClientConfig& config):impl_(std::make_unique<Impl>()){
 auto& p=*impl_;p.config=config;if(!SDL_Init(SDL_INIT_VIDEO|SDL_INIT_EVENTS))throw std::runtime_error(SDL_GetError());p.sdl=true;
 p.window=SDL_CreateWindow("SONNHEIDE",config.width,config.height,SDL_WINDOW_RESIZABLE|SDL_WINDOW_HIGH_PIXEL_DENSITY|(config.fullscreen?SDL_WINDOW_FULLSCREEN:0));if(!p.window)throw std::runtime_error(SDL_GetError());p.update_dimensions();
 void* handle=nullptr;
#ifdef _WIN32
 handle=SDL_GetPointerProperty(SDL_GetWindowProperties(p.window),SDL_PROP_WINDOW_WIN32_HWND_POINTER,nullptr);
#else
 handle=SDL_GetPointerProperty(SDL_GetWindowProperties(p.window),SDL_PROP_WINDOW_COCOA_WINDOW_POINTER,nullptr);
#endif
 if(!handle)throw std::runtime_error("SDL did not provide a native window handle");bgfx::renderFrame();bgfx::Init init;
#ifdef _WIN32
 init.type=bgfx::RendererType::Direct3D11;
#else
 init.type=bgfx::RendererType::Metal;
#endif
 init.fallback=false;init.callback=&p.capture;init.swapChain.nwh=handle;init.swapChain.width=std::uint32_t(p.size.pixel_w);init.swapChain.height=std::uint32_t(p.size.pixel_h);init.swapChain.flags=BGFX_SWAP_CHAIN_MSAA_X4;init.reset=BGFX_RESET_VSYNC;
 if(!bgfx::init(init))throw std::runtime_error("Actual native GPU initialization failed");p.gpu=true;p.gpu_width=p.size.pixel_w;p.gpu_height=p.size.pixel_h;
 auto folder=config.resources/"shaders"/(init.type==bgfx::RendererType::Metal?"metal":"dx11");p.world=program(folder,"world","world");p.sky=program(folder,"sky","sky");p.water=program(folder,"world","water");p.shadow=program(folder,"shadow","shadow");p.ui=program(folder,"ui","ui");
 p.world_layout.begin().add(bgfx::Attrib::Position,3,bgfx::AttribType::Float).add(bgfx::Attrib::Normal,3,bgfx::AttribType::Float).add(bgfx::Attrib::TexCoord0,2,bgfx::AttribType::Float).add(bgfx::Attrib::Color0,4,bgfx::AttribType::Uint8,true).end();
 p.screen_layout.begin().add(bgfx::Attrib::Position,3,bgfx::AttribType::Float).add(bgfx::Attrib::TexCoord0,2,bgfx::AttribType::Float).end();p.ui_layout.begin().add(bgfx::Attrib::Position,3,bgfx::AttribType::Float).add(bgfx::Attrib::TexCoord0,2,bgfx::AttribType::Float).add(bgfx::Attrib::Color0,4,bgfx::AttribType::Uint8,true).end();
 p.base_u=bgfx::createUniform("s_base",bgfx::UniformType::Sampler);p.mean_u=bgfx::createUniform("s_mean",bgfx::UniformType::Sampler);p.shadow_u=bgfx::createUniform("s_shadow",bgfx::UniformType::Sampler);p.ui_u=bgfx::createUniform("s_ui",bgfx::UniformType::Sampler);
 p.eye_u=bgfx::createUniform("u_eyeAge",bgfx::UniformType::Vec4);p.light_u=bgfx::createUniform("u_light",bgfx::UniformType::Vec4);p.environment_u=bgfx::createUniform("u_environment",bgfx::UniformType::Vec4);p.light_matrix_u=bgfx::createUniform("u_lightMatrix",bgfx::UniformType::Mat4);p.forward_u=bgfx::createUniform("u_cameraForward",bgfx::UniformType::Vec4);p.right_u=bgfx::createUniform("u_cameraRight",bgfx::UniformType::Vec4);p.up_u=bgfx::createUniform("u_cameraUp",bgfx::UniformType::Vec4);
 p.water_depth_u=bgfx::createUniform("s_surfaceInfo",bgfx::UniformType::Sampler);p.fine_atlas_u=bgfx::createUniform("s_fineAtlas",bgfx::UniformType::Sampler);p.fine_pages_u=bgfx::createUniform("s_finePages",bgfx::UniformType::Sampler);p.fine_domain_u=bgfx::createUniform("u_fineDomain",bgfx::UniformType::Vec4);p.surface_mode_u=bgfx::createUniform("u_surfaceMode",bgfx::UniformType::Vec4);p.water_domain_u=bgfx::createUniform("u_waterDomain",bgfx::UniformType::Vec4);p.menu_u=bgfx::createUniform("u_menuScene",bgfx::UniformType::Vec4);
 p.pixel_materials();auto white=rgba(255,255,255);p.white=bgfx::createTexture2D(1,1,false,1,bgfx::TextureFormat::RGBA8,0,bgfx::copy(&white,4));
 std::array<bgfx::TextureHandle,2> targets={bgfx::createTexture2D(1024,1024,false,1,bgfx::TextureFormat::RGBA8,BGFX_TEXTURE_RT|BGFX_SAMPLER_MIN_POINT|BGFX_SAMPLER_MAG_POINT|BGFX_SAMPLER_U_CLAMP|BGFX_SAMPLER_V_CLAMP),bgfx::createTexture2D(1024,1024,false,1,bgfx::TextureFormat::D24S8,BGFX_TEXTURE_RT_WRITE_ONLY)};
 if(!bgfx::isValid(targets[0])||!bgfx::isValid(targets[1])){for(auto t:targets)if(bgfx::isValid(t))bgfx::destroy(t);throw std::runtime_error("Shadow framebuffer textures unavailable");}p.shadow_texture=targets[0];p.shadow_frame=bgfx::createFrameBuffer(2,targets.data(),true);if(!bgfx::isValid(p.shadow_frame)){for(auto t:targets)bgfx::destroy(t);throw std::runtime_error("Shadow framebuffer unavailable");}
 std::array<ScreenVertex,4> screen={{{-1,-1,0,0,1},{1,-1,0,1,1},{1,1,0,1,0},{-1,1,0,0,0}}};std::array<std::uint16_t,6> indices={0,1,2,0,2,3};p.screen_vb=bgfx::createVertexBuffer(bgfx::copy(screen.data(),sizeof(screen)),p.screen_layout);p.screen_ib=bgfx::createIndexBuffer(bgfx::copy(indices.data(),sizeof(indices)));
 constexpr float ocean=100000;std::vector<Vertex> ov={ {-ocean,0,-ocean,0,1,0,0,0,0xffffffff},{-ocean,0,ocean,0,1,0,0,0,0xffffffff},{ocean,0,ocean,0,1,0,0,0,0xffffffff},{ocean,0,-ocean,0,1,0,0,0,0xffffffff} };p.ocean=upload(ov,{0,1,2,0,2,3},p.world_layout);
 for(int view=0;view<5;++view)bgfx::setViewMode(bgfx::ViewId(view),bgfx::ViewMode::Sequential);bgfx::setViewFrameBuffer(0,p.shadow_frame);bgfx::setViewRect(0,0,0,1024,1024);bgfx::setViewClear(0,BGFX_CLEAR_COLOR|BGFX_CLEAR_DEPTH,0xffffffffu,1.0f,std::uint8_t(0));bgfx::setViewClear(1,BGFX_CLEAR_COLOR|BGFX_CLEAR_DEPTH,0x060d18ffu,1.0f,std::uint8_t(0));bgfx::setViewClear(2,BGFX_CLEAR_NONE);bgfx::setViewClear(3,BGFX_CLEAR_NONE);bgfx::setViewClear(4,BGFX_CLEAR_COLOR|BGFX_CLEAR_DEPTH,0x060d18ffu,1.0f,std::uint8_t(0));std::array<bgfx::ViewId,5> order={4,0,1,2,3};bgfx::setViewOrder(0,5,order.data());
 if(SDL_InitSubSystem(SDL_INIT_AUDIO)){SDL_AudioSpec spec{SDL_AUDIO_F32,2,48000};p.audio=SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK,&spec,nullptr,nullptr);if(p.audio){SDL_SetAudioStreamGain(p.audio,p.volume);SDL_ResumeAudioStreamDevice(p.audio);p.audio_message="Audio device initialized (no music asset installed)";}else p.audio_message=SDL_GetError();}else p.audio_message=SDL_GetError();
}
Client::~Client()=default;
Dimensions Client::dimensions()const{impl_->update_dimensions();return impl_->size;}
bool Client::poll_event(SDL_Event& e){return SDL_PollEvent(&e);}
bool Client::visible()const{return !(SDL_GetWindowFlags(impl_->window)&(SDL_WINDOW_HIDDEN|SDL_WINDOW_MINIMIZED));}
bool Client::focused()const{return (SDL_GetWindowFlags(impl_->window)&SDL_WINDOW_INPUT_FOCUS)!=0;}
void Client::fullscreen(bool x){if(!SDL_SetWindowFullscreen(impl_->window,x))throw std::runtime_error(SDL_GetError());}
void Client::cancel_input(){auto& p=*impl_;p.drag=false;p.orbit=false;SDL_CaptureMouse(false);if(p.typing){SDL_StopTextInput(p.window);p.typing=false;}}
void Client::sync_text_input(Rml::Context& context){auto& p=*impl_;auto* e=context.GetFocusElement();bool typing=focused()&&e&&(e->GetTagName()=="input"||e->GetTagName()=="textarea");if(typing&&!p.typing){SDL_StartTextInput(p.window);p.typing=true;}else if(!typing&&p.typing){SDL_StopTextInput(p.window);p.typing=false;}if(typing){auto pos=e->GetAbsoluteOffset(Rml::Box::BORDER);auto size=e->GetBox().GetSize(Rml::Box::BORDER);SDL_Rect rect{int(pos.x/p.size.density),int(pos.y/p.size.density),std::max(1,int(size.x/p.size.density)),std::max(1,int(size.y/p.size.density))};SDL_SetTextInputArea(p.window,&rect,rect.w);}}
bool Client::process_ui_event(const SDL_Event& e,Rml::Context& context){auto& p=*impl_;const int mods=modifiers(SDL_GetModState());auto interactive=[&](float x,float y){auto* target=context.GetElementAtPoint({x*p.size.density,y*p.size.density});while(target){auto tag=target->GetTagName();if(tag=="button"||tag=="input"||tag=="textarea"||tag=="select"||tag=="nav"||target->GetId().find("panel")!=std::string::npos)return true;target=target->GetParentNode();}return false;};switch(e.type){case SDL_EVENT_MOUSE_MOTION:{context.ProcessMouseMove(int(e.motion.x*p.size.density),int(e.motion.y*p.size.density),mods);return !p.drag&&interactive(e.motion.x,e.motion.y);}case SDL_EVENT_MOUSE_BUTTON_DOWN:case SDL_EVENT_MOUSE_BUTTON_UP:{context.ProcessMouseMove(int(e.button.x*p.size.density),int(e.button.y*p.size.density),mods);int b=e.button.button==SDL_BUTTON_LEFT?0:e.button.button==SDL_BUTTON_RIGHT?1:2;if(e.type==SDL_EVENT_MOUSE_BUTTON_DOWN)context.ProcessMouseButtonDown(b,mods);else context.ProcessMouseButtonUp(b,mods);sync_text_input(context);return !p.drag&&interactive(e.button.x,e.button.y);}case SDL_EVENT_MOUSE_WHEEL:{context.ProcessMouseMove(int(e.wheel.mouse_x*p.size.density),int(e.wheel.mouse_y*p.size.density),mods);if(interactive(e.wheel.mouse_x,e.wheel.mouse_y)){context.ProcessMouseWheel(-e.wheel.y,mods);return true;}return false;}case SDL_EVENT_KEY_DOWN:{p.pressed_keys.insert(key(e.key.key));bool consumed=!context.ProcessKeyDown(key(e.key.key),modifiers(e.key.mod));sync_text_input(context);return consumed||p.typing;}case SDL_EVENT_KEY_UP:p.pressed_keys.erase(key(e.key.key));return !context.ProcessKeyUp(key(e.key.key),modifiers(e.key.mod))||p.typing;case SDL_EVENT_TEXT_INPUT:context.ProcessTextInput(e.text.text);return p.typing;case SDL_EVENT_TEXT_EDITING:return p.typing;case SDL_EVENT_WINDOW_FOCUS_LOST:for(auto k:p.pressed_keys)context.ProcessKeyUp(k,0);p.pressed_keys.clear();for(int i=0;i<3;++i)context.ProcessMouseButtonUp(i,0);context.ProcessMouseLeave();cancel_input();return false;case SDL_EVENT_WINDOW_MOUSE_LEAVE:context.ProcessMouseLeave();return false;default:return false;}}
bool Client::process_camera_event(const SDL_Event& e){auto& p=*impl_;auto& c=p.camera;switch(e.type){case SDL_EVENT_WINDOW_FOCUS_LOST:cancel_input();return false;case SDL_EVENT_MOUSE_BUTTON_DOWN:if(e.button.button==SDL_BUTTON_RIGHT||e.button.button==SDL_BUTTON_MIDDLE){p.drag=true;p.orbit=e.button.button==SDL_BUTTON_MIDDLE;SDL_CaptureMouse(true);return true;}return false;case SDL_EVENT_MOUSE_BUTTON_UP:if(p.drag){p.drag=false;SDL_CaptureMouse(false);return true;}return false;case SDL_EVENT_MOUSE_MOTION:if(p.drag){if(p.orbit){c.yaw_deg=std::fmod(c.yaw_deg+e.motion.xrel*.32,360.);c.pitch_deg=std::clamp(c.pitch_deg-e.motion.yrel*.24,10.,85.);}else{auto b=basis(c);double mpp=c.distance_m*2*std::tan(25*pi/180)/std::max(1,p.scene_rect()[3]);c.target=add(c.target,add(scale(b.right,-e.motion.xrel*mpp),scale(normalized({b.forward.x,0,b.forward.z}),e.motion.yrel*mpp)));}return true;}return false;case SDL_EVENT_MOUSE_WHEEL:c.distance_m=std::clamp(c.distance_m*std::exp(-e.wheel.y*.15),1.25,14000.);return true;case SDL_EVENT_KEY_DOWN:{auto b=basis(c);double d=c.distance_m*.06;if(e.key.key==SDLK_LEFT||e.key.key==SDLK_A)c.target=add(c.target,scale(b.right,-d));else if(e.key.key==SDLK_RIGHT||e.key.key==SDLK_D)c.target=add(c.target,scale(b.right,d));else if(e.key.key==SDLK_UP||e.key.key==SDLK_W)c.target=add(c.target,scale(normalized({b.forward.x,0,b.forward.z}),d));else if(e.key.key==SDLK_DOWN||e.key.key==SDLK_S)c.target=add(c.target,scale(normalized({b.forward.x,0,b.forward.z}),-d));else if(e.key.key==SDLK_Q)c.yaw_deg-=8;else if(e.key.key==SDLK_E)c.yaw_deg+=8;else if(e.key.key==SDLK_EQUALS)c.distance_m=std::max(1.25,c.distance_m*.85);else if(e.key.key==SDLK_MINUS)c.distance_m=std::min(14000.,c.distance_m/ .85);else return false;return true;}default:return false;}}
Camera& Client::camera(){return impl_->camera;}const Camera& Client::camera()const{return impl_->camera;}
void Client::frame_surface(const SurfaceView& s){auto& c=impl_->camera;c={};c.target={s.origin_x+s.width*s.cell_m*.5,2,s.origin_z+s.height*s.cell_m*.5};if(s.cell){auto v=s.cell(s.width/2,s.height/2);c.target.y=std::max(0.f,v.height_m);}c.distance_m=std::clamp(std::min(s.width,s.height)*s.cell_m*.55,24.,14000.);c.pitch_deg=32;c.yaw_deg=32;}
Ray Client::screen_ray(double x,double y)const {
 auto& p=*impl_;auto b=basis(p.camera);auto rect=p.scene_rect();
 double aspect=double(rect[2])/rect[3],tangent=std::tan(25*pi/180);
 double u=(2*(x-rect[0])/rect[2]-1)*aspect*tangent;
 double v=(1-2*(y-rect[1])/rect[3])*tangent;
 return {b.eye,normalized(add(b.forward,add(scale(b.right,u),scale(b.up,v))))};
}
std::optional<SurfaceHit> Client::pick(const SurfaceView& s,double x,double y)const {
 auto r=impl_->scene_rect();if(!s.raycast||x<r[0]||y<r[1]||x>=r[0]+r[2]||y>=r[1]+r[3])return {};
 return s.raycast(screen_ray(x,y));
}
void Client::begin_frame(const SurfaceView* surface,Age age,double presentation_seconds){auto& p=*impl_;p.update_dimensions();if(p.size.pixel_w<=0||p.size.pixel_h<=0)return;if(p.gpu_width!=p.size.pixel_w||p.gpu_height!=p.size.pixel_h){p.gpu_width=p.size.pixel_w;p.gpu_height=p.size.pixel_h;bgfx::SwapChain sc;sc.width=std::uint32_t(p.size.pixel_w);sc.height=std::uint32_t(p.size.pixel_h);sc.flags=BGFX_SWAP_CHAIN_MSAA_X4;bgfx::reset(BGFX_RESET_VSYNC,&sc);}p.frame_upload_bytes=0;p.frame_texture_upload_bytes=0;p.frame_decoration_samples=0;p.frame_decoration_chunks=0;p.age=age;p.seconds=std::isfinite(presentation_seconds)?std::max(0.,presentation_seconds):0;p.presenting_menu=!surface&&!p.map_visible;const auto& render_camera=p.scene_camera();const int w=p.size.pixel_w,h=p.size.pixel_h;auto rect=p.scene_pixels();auto logical=p.scene_rect();float aspect=float(logical[2])/logical[3];for(int view=1;view<3;++view)bgfx::setViewRect(bgfx::ViewId(view),std::uint16_t(rect[0]),std::uint16_t(rect[1]),std::uint16_t(rect[2]),std::uint16_t(rect[3]));bgfx::setViewRect(3,0,0,std::uint16_t(w),std::uint16_t(h));bgfx::setViewRect(4,0,0,std::uint16_t(w),std::uint16_t(h));bgfx::touch(4);auto b=basis(render_camera);float view[16],projection[16];bx::mtxLookAt(view,{float(b.eye.x-render_camera.target.x),float(b.eye.y),float(b.eye.z-render_camera.target.z)},{0,float(render_camera.target.y),0});bx::mtxProj(projection,50.f,aspect,.08f,50000.f,bgfx::getCaps()->homogeneousDepth);bgfx::setViewTransform(2,view,projection);bgfx::touch(1);
 p.environment(b);float f[4]={float(b.forward.x),float(b.forward.y),float(b.forward.z),0},r[4]={float(b.right.x),float(b.right.y),float(b.right.z),aspect*.46630766f},u[4]={float(b.up.x),float(b.up.y),float(b.up.z),.46630766f};bgfx::setUniform(p.forward_u,f);bgfx::setUniform(p.right_u,r);bgfx::setUniform(p.up_u,u);bgfx::setVertexBuffer(0,p.screen_vb);bgfx::setIndexBuffer(p.screen_ib);float menu[4]={p.presenting_menu?1.f:0.f,float(p.seconds),float(std::max(1,p.size.logical_w)),float(std::max(1,p.size.logical_h))};bgfx::setUniform(p.menu_u,menu);bgfx::setState(BGFX_STATE_WRITE_RGB|BGFX_STATE_WRITE_A);bgfx::submit(1,p.sky);
 if(surface){
  p.prepare_water_depth(*surface);p.prepare_fine_coverage(*surface);auto visible=p.prepare_chunks(*surface);std::vector<std::pair<int,int>> far_visible;for(const auto& [key,chunk]:p.far_chunks)far_visible.push_back(key);
  float light_view[16],light_projection[16];float extent=float(std::max(24.,render_camera.distance_m*1.4));bx::mtxLookAt(light_view,{extent*.5f,float(render_camera.target.y)+extent*.8f,extent*.35f},{0,float(render_camera.target.y),0});bx::mtxOrtho(light_projection,-extent,extent,-extent,extent,1.f,extent*4,0,bgfx::getCaps()->homogeneousDepth);bx::mtxMul(p.light_matrix,light_view,light_projection);bgfx::setViewTransform(0,light_view,light_projection);bgfx::touch(0);
  p.draw_collection(p.far_chunks,far_visible,0,p.shadow);p.draw_collection(p.chunks,visible,0,p.shadow);p.draw_decoration(visible,0);
  p.draw_water(b,false);p.draw_overview();p.draw_collection(p.far_chunks,far_visible,2,p.world);p.draw_collection(p.chunks,visible,2,p.world);p.draw_decoration(visible,2);p.draw_water(b,true);
 }
 if(!surface&&p.map_visible&&bgfx::isValid(p.map_texture)&&p.map_w>0&&p.map_h>0){float projection[16];bx::mtxOrtho(projection,0,float(rect[2]),float(rect[3]),0,0,100,0,bgfx::getCaps()->homogeneousDepth);bgfx::setViewTransform(2,nullptr,projection);float x=0,y=0,mw=float(rect[2]),mh=float(rect[3]);std::array<UiVertex,4> vertices={{{x,y,0,0,0,0xffffffff},{x+mw,y,0,1,0,0xffffffff},{x+mw,y+mh,0,1,1,0xffffffff},{x,y+mh,0,0,1,0xffffffff}}};std::array<std::uint16_t,6> indices={0,1,2,0,2,3};bgfx::TransientVertexBuffer vb;bgfx::TransientIndexBuffer ib;if(bgfx::getAvailTransientVertexBuffer(4,p.ui_layout)!=4||bgfx::getAvailTransientIndexBuffer(6)!=6)throw std::runtime_error("Navigation texture geometry budget exhausted");bgfx::allocTransientVertexBuffer(&vb,4,p.ui_layout);bgfx::allocTransientIndexBuffer(&ib,6);std::memcpy(vb.data,vertices.data(),sizeof(vertices));std::memcpy(ib.data,indices.data(),sizeof(indices));float model[16];bx::mtxIdentity(model);bgfx::setTransform(model);bgfx::setScissor(UINT16_MAX);bgfx::setVertexBuffer(0,&vb);bgfx::setIndexBuffer(&ib);bgfx::setTexture(0,p.ui_u,p.map_texture);bgfx::setState(BGFX_STATE_WRITE_RGB|BGFX_STATE_WRITE_A);bgfx::submit(2,p.ui);}
 begin_ui();
}
void Client::begin_ui(){auto& p=*impl_;p.scissor=false;p.transformed=false;float ortho[16];bx::mtxOrtho(ortho,0,float(p.size.pixel_w),float(p.size.pixel_h),0,0,100,0,bgfx::getCaps()->homogeneousDepth);bgfx::setViewTransform(3,nullptr,ortho);}
void Client::end_frame(){bgfx::frame();if(!impl_->capture.error.empty())throw std::runtime_error(impl_->capture.error);}
void Client::request_screenshot(const std::filesystem::path& name){auto text=utf8(name);bgfx::requestScreenShot(BGFX_INVALID_HANDLE,text.c_str());}
void Client::material_diagnostic(MaterialDiagnostic d){impl_->diagnostic=d;}
void Client::set_map_rgba(int w,int h,std::span<const std::uint8_t> data,std::uint64_t revision){auto& p=*impl_;if(revision==p.map_revision&&bgfx::isValid(p.map_texture))return;if(w<=0||h<=0||w>8192||h>8192||data.size()!=std::size_t(w)*h*4)throw std::runtime_error("Invalid geography navigation texture");auto texture=bgfx::createTexture2D(std::uint16_t(w),std::uint16_t(h),false,1,bgfx::TextureFormat::RGBA8,BGFX_SAMPLER_MIN_POINT|BGFX_SAMPLER_MAG_POINT|BGFX_SAMPLER_U_CLAMP|BGFX_SAMPLER_V_CLAMP,bgfx::copy(data.data(),std::uint32_t(data.size())));if(!bgfx::isValid(texture))throw std::runtime_error("Geography navigation upload failed");if(bgfx::isValid(p.map_texture))bgfx::destroy(p.map_texture);p.map_texture=texture;p.map_revision=revision;}
void Client::map_overlay(int x,int y,int w,int h,bool visible){auto& p=*impl_;p.map_x=x;p.map_y=y;p.map_w=w;p.map_h=h;p.map_visible=visible;}
std::string Client::renderer_name()const{return bgfx::getRendererName(bgfx::getRendererType());}
std::string Client::gpu_parameters_json()const{
 auto& p=*impl_;const auto& c=p.scene_camera();std::size_t vertices=0,indices=0,decoration_quads=0,far_bytes=0;
 for(const auto& [key,chunk]:p.chunks){vertices+=chunk.buffer.vertices+chunk.decoration.vertices;indices+=chunk.buffer.indices+chunk.decoration.indices;decoration_quads+=chunk.decoration_quads;}
 for(const auto& [key,chunk]:p.far_chunks)far_bytes+=p.buffer_bytes(chunk.buffer);
 std::ostringstream s;s<<"{\"renderer\":\""<<renderer_name()<<"\",\"camera\":{\"yaw_deg\":"<<c.yaw_deg<<",\"pitch_deg\":"<<c.pitch_deg<<",\"distance_m\":"<<c.distance_m<<",\"target\":["<<c.target.x<<','<<c.target.y<<','<<c.target.z<<"]},\"pixel_size\":["<<p.size.pixel_w<<','<<p.size.pixel_h<<"],\"world_extent_m\":["<<p.depth_domain[2]<<','<<p.depth_domain[3]<<"],\"terrain_profile\":"<<p.surface_profile<<",\"management_cell_m\":"<<p.depth_cell_m<<",\"micro_surface_m\":0.03125,\"building_grid_independent\":true,\"theme_layers\":24,\"atlas_layers\":32,\"tile_pixels\":128,\"pixels_per_metre\":64,\"material_atlas_bytes\":2097152,\"material_uploads\":"<<p.material_uploads<<",\"material_sha256\":\""<<sonn::hex(p.material_hash)<<"\",\"shadow_map\":1024,\"outline\":false,\"water_level_m\":0,\"soil_height_m\":"<<p.surface_heights[4]<<",\"sand_height_m\":"<<p.surface_heights[3]<<",\"water_land_height_epsilon_m\":0,\"diagnostic\":"<<int(p.diagnostic)<<",\"presentation_only\":"<<(p.presenting_menu?"true":"false")<<",\"presentation_seconds\":"<<p.seconds<<",\"menu_scene\":\"navy_pixel_stars_meteors\",\"menu_geometry_generated\":false,\"material_model\":\"authoritative_sparse_micro_surface_and_pixel_clusters\",\"age\":\""<<(p.age==Age::Light?"Light":"Darkness")<<"\",\"surface_identity\":\""<<sonn::hex(p.mesh_identity)<<"\",\"surface_revision\":"<<(p.mesh_revision==~0ULL?0:p.mesh_revision)<<",\"mesh_uploads\":"<<p.mesh_uploads<<",\"mesh_cache_resets\":"<<p.mesh_cache_resets<<",\"resident_detail_chunks\":"<<p.chunks.size()<<",\"resident_chunk_limit\":128,\"cached_geometry_vertices\":"<<vertices<<",\"cached_geometry_indices\":"<<indices<<",\"cached_geometry_gpu_bytes\":"<<p.resident_bytes()<<",\"geometry_resident_limit_bytes\":"<<p.geometry_limit<<",\"far_elevated_geometry_bytes\":"<<far_bytes<<",\"geometry_evictions\":"<<p.geometry_evictions<<",\"detail_geometry_upload_bytes_this_frame\":"<<p.frame_upload_bytes<<",\"detail_texture_upload_bytes_this_frame\":"<<p.frame_texture_upload_bytes<<",\"detail_upload_limit_bytes_per_frame\":"<<p.frame_upload_limit<<",\"near_surface_geometry\":\"exact_micro_mask_on_shared_zero_plane\",\"land_mask_vertices\":4,\"decoration_cpu_sample_limit_per_frame\":65536,\"decoration_cpu_samples_this_frame\":"<<p.frame_decoration_samples<<",\"decoration_cpu_chunk_limit_per_frame\":16,\"decoration_cpu_chunks_this_frame\":"<<p.frame_decoration_chunks<<",\"complete_overview_gpu_bytes\":"<<p.overview_data.size()<<",\"coverage_count_bits\":13,\"complete_overview_available\":"<<(bgfx::isValid(p.water_depth)?"true":"false")<<",\"fine_coverage_atlas_gpu_bytes\":16777216,\"fine_page_gpu_bytes\":65536,\"fine_atlas_patch_limit\":1024,\"resident_fine_patches\":"<<p.fine_slots.size()<<",\"fine_patch_candidates\":"<<p.fine_patch_candidates<<",\"fine_patch_deferred\":"<<p.fine_patch_deferred<<",\"fine_decode_column_limit_per_frame\":1048576,\"fine_window_management_cell_limit\":16384,\"fine_texture_uploads\":"<<p.fine_texture_uploads<<",\"fine_page_uploads\":"<<p.fine_page_uploads<<",\"decoration_quads\":"<<decoration_quads<<",\"mechanical_trees_spawned_by_renderer\":0,\"water_depth_uploads\":"<<p.depth_uploads<<",\"water_depth_cell_m\":"<<p.depth_cell_m<<",\"outside_water_depth_m\":20,\"scene_logical\":["<<p.scene_rect()[0]<<','<<p.scene_rect()[1]<<','<<p.scene_rect()[2]<<','<<p.scene_rect()[3]<<"]}";return s.str();
}

Rml::RenderInterface* Client::render_interface(){return &impl_->render;}Rml::SystemInterface* Client::system_interface(){return &impl_->system;}Rml::FileInterface* Client::file_interface(){return &impl_->files;}
void Client::audio_volume(float v){impl_->volume=std::clamp(v,0.f,1.f);if(impl_->audio)SDL_SetAudioStreamGain(impl_->audio,impl_->volume);}bool Client::audio_available()const{return impl_->audio;}std::string Client::audio_status()const{return impl_->audio_message;}
} // namespace sonnheide::client
