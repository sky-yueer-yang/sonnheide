#include "ui.hpp"
#include "pixel_decorator.hpp"
#include <RmlUi/Core.h>
#include <RmlUi/Core/FontEngineInterface.h>
#include <RmlUi/Core/Elements/ElementFormControl.h>
#include <RmlUi/Core/Elements/ElementFormControlSelect.h>
#include <array>
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <initializer_list>
#include <iterator>
#include <map>
#include <set>
#include <RmlUi/Core/ElementText.h>
#include <stdexcept>
#include <vector>

using namespace sonnheide::ui;
namespace {
void require(bool condition,const char* why) { if(!condition) throw std::runtime_error(why); }
struct System final:Rml::SystemInterface {
    double GetElapsedTime() override{return 0;}
    bool LogMessage(Rml::Log::Type,const Rml::String& message) override {
        // Locked RmlUi reports unsupported layout and stylesheet properties as
        // warnings. Treat actual parser rejection as a test failure, including
        // document load, so an ignored design declaration cannot pass silently.
        if(message.find("will not be formatted")!=Rml::String::npos ||
           message.find("Syntax error parsing property declaration")!=Rml::String::npos ||
           message.find("Could not parse font-effect")!=Rml::String::npos ||
           message.find("could not be instanced")!=Rml::String::npos)
            layout_rejections.push_back(message);
        return true;
    }
    std::vector<Rml::String> layout_rejections;
};
struct Render final:Rml::RenderInterface {
    void RenderGeometry(Rml::Vertex* vertices,int count,int* indices,int index_count,Rml::TextureHandle texture,const Rml::Vector2f& translation) override{
        const auto material=material_textures.find(texture);
        if(material!=material_textures.end()) {
            materials_drawn.insert(material->second);
            material_draws.push_back({material->second,translation,std::vector<Rml::Vertex>(vertices,vertices+count),std::vector<int>(indices,indices+index_count)});
            // Material pixels are not stretched colour blocks: native geometry
            // supplies bounded UVs to the admitted finite-palette texture.
            for(int i=0;i<count;++i) require(vertices[i].tex_coord.x>=0 && vertices[i].tex_coord.x<=1 && vertices[i].tex_coord.y>=0 && vertices[i].tex_coord.y<=1,"Actual physical material UVs must address their admitted pixel tile");
            return;
        }
        if(texture!=0 && count>4) {
            for(int i=0;i<count;++i) {
                const auto c=vertices[i].colour;
                if(c.red==217 && c.green==145 && c.blue==0 && c.alpha==255) bold_pigment=true;
                if(c.red==255 && c.green==255 && c.blue==255 && c.alpha==255) white_glyph_outline=true;
            }
        }
        if(icon_textures.count(texture) && count==4) {
            bool untinted=true;
            for(int i=0;i<count;++i) untinted=untinted && vertices[i].colour.red==255 && vertices[i].colour.green==255 && vertices[i].colour.blue==255 && vertices[i].colour.alpha==255;
            if(untinted) full_colour_sprite=true;
        }
    }
    Rml::CompiledGeometryHandle CompileGeometry(Rml::Vertex* vertices,int count,int* indices,int index_count,Rml::TextureHandle texture) override {
        const auto handle=++next_geometry;
        compiled_geometry.emplace(handle,CompiledGeometry{std::vector<Rml::Vertex>(vertices,vertices+count),std::vector<int>(indices,indices+index_count),texture});
        return handle;
    }
    void RenderCompiledGeometry(Rml::CompiledGeometryHandle handle,const Rml::Vector2f& translation) override {
        auto found=compiled_geometry.find(handle);require(found!=compiled_geometry.end(),"Native rendering must not reuse a released material geometry handle");
        auto& geometry=found->second;
        RenderGeometry(geometry.vertices.data(),static_cast<int>(geometry.vertices.size()),geometry.indices.data(),static_cast<int>(geometry.indices.size()),geometry.texture,translation);
        ++compiled_draws;
    }
    void ReleaseCompiledGeometry(Rml::CompiledGeometryHandle handle) override {
        require(compiled_geometry.erase(handle)==1,"Native geometry cache release must own one live compiled handle");
    }
    void EnableScissorRegion(bool) override{}
    void SetScissorRegion(int,int,int,int) override{}
    bool LoadTexture(Rml::TextureHandle& texture,Rml::Vector2i& size,const Rml::String& source) override{
        texture=++next;size={96,96};
        for(const auto& definition:physical_material_definitions()) {
            if(source.find(definition.texture_source)!=Rml::String::npos) {
                material_textures[texture]=definition.name;size={definition.width,definition.height};
                auto path=std::filesystem::path(source);
                if(!path.is_absolute()) path=resource_root/path;
                std::ifstream input(path,std::ios::binary);require(input.good(),"Actual physical material texture file must exist");
                std::array<unsigned char,24> png{};input.read(reinterpret_cast<char*>(png.data()),png.size());
                const auto dimension=[&](int offset){return (static_cast<int>(png[offset])<<24)|(static_cast<int>(png[offset+1])<<16)|(static_cast<int>(png[offset+2])<<8)|png[offset+3];};
                require(png[0]==137 && png[1]=='P' && png[2]=='N' && png[3]=='G' && dimension(16)==definition.width && dimension(20)==definition.height,"Production material file must match the exact admitted texture dimensions");
                break;
            }
        }
        if(source.find("/icons/")!=Rml::String::npos) icon_textures.insert(texture);
        return true;
    }
    bool GenerateTexture(Rml::TextureHandle& texture,const Rml::byte*,const Rml::Vector2i&) override{texture=++next;return true;}
    Rml::TextureHandle next=0;
    std::filesystem::path resource_root;
    std::map<Rml::TextureHandle,std::string> material_textures;
    std::set<Rml::TextureHandle> icon_textures;
    std::set<std::string> materials_drawn;
    struct CompiledGeometry {std::vector<Rml::Vertex> vertices;std::vector<int> indices;Rml::TextureHandle texture;};
    Rml::CompiledGeometryHandle next_geometry=0;
    std::map<Rml::CompiledGeometryHandle,CompiledGeometry> compiled_geometry;
    std::size_t compiled_draws=0;
    struct MaterialDraw {std::string family;Rml::Vector2f position;std::vector<Rml::Vertex> vertices;std::vector<int> indices;};
    std::vector<MaterialDraw> material_draws;
    struct MaterialSample {Rml::Vector2f uv;Rml::byte alpha;Rml::Colourb tint;};
    std::vector<MaterialSample> material_samples(const char* family,Rml::Vector2f screen_point,const Rml::Vector2f* draw_origin=nullptr) const {
        std::vector<MaterialSample> samples;
        const auto cross=[](Rml::Vector2f a,Rml::Vector2f b){return a.x*b.y-a.y*b.x;};
        for(const auto& draw:material_draws) {
            if(draw.family!=family || (draw_origin && draw.position!=*draw_origin)) continue;
            const auto point=screen_point-draw.position;
            for(std::size_t i=0;i+2<draw.indices.size();i+=3) {
                const auto& a=draw.vertices.at(static_cast<std::size_t>(draw.indices[i]));
                const auto& b=draw.vertices.at(static_cast<std::size_t>(draw.indices[i+1]));
                const auto& c=draw.vertices.at(static_cast<std::size_t>(draw.indices[i+2]));
                const float area=cross(b.position-a.position,c.position-a.position);
                if(std::abs(area)<.0001f) continue;
                const float wb=cross(point-a.position,c.position-a.position)/area;
                const float wc=cross(b.position-a.position,point-a.position)/area;
                const float wa=1.f-wb-wc;
                if(wa<-.0001f || wb<-.0001f || wc<-.0001f) continue;
                samples.push_back({a.tex_coord*wa+b.tex_coord*wb+c.tex_coord*wc,static_cast<Rml::byte>(std::round(a.colour.alpha*wa+b.colour.alpha*wb+c.colour.alpha*wc)),a.colour});
                // One triangle can share its edge with another in this draw.
                // Count a draw once; separate overlapping draws still count.
                break;
            }
        }
        return samples;
    }
    bool material_at(const char* family,Rml::Vector2f position) const {
        for(const auto& draw:material_draws)if(draw.family==family && draw.position==position)return true;
        return false;
    }
    bool full_colour_sprite=false,bold_pigment=false,white_glyph_outline=false;
    void reset_draws(){materials_drawn.clear();material_draws.clear();full_colour_sprite=bold_pigment=white_glyph_outline=false;}
};

bool covers(const PixelMesh& mesh,float x,float y) {
    const auto cross=[](Rml::Vector2f a,Rml::Vector2f b,Rml::Vector2f p){return(b.x-a.x)*(p.y-a.y)-(b.y-a.y)*(p.x-a.x);};
    for(std::size_t i=0;i+2<mesh.indices.size();i+=3) {
        const auto a=mesh.vertices[mesh.indices[i]].position,b=mesh.vertices[mesh.indices[i+1]].position,c=mesh.vertices[mesh.indices[i+2]].position;
        const Rml::Vector2f p{x,y};const float u=cross(a,b,p),v=cross(b,c,p),w=cross(c,a,p);
        if((u>=0 && v>=0 && w>=0) || (u<=0 && v<=0 && w<=0)) return true;
    }
    return false;
}
void pixel_geometry_contract() {
    for(float density:{1.f,2.f}) {
        const auto mesh=pixel_mesh(80*density,40*density,4*density,0,{12,24,42,248},{0,0,0,0});
        require(covers(mesh,40*density,20*density),"Pixel panel fill must cover its centre");
        require(!covers(mesh,.5f*density,.5f*density) && !covers(mesh,5*density,2*density),"Pixel panel must have genuine two-step cut corners, not a rectangular recolour");
        require(covers(mesh,9*density,2*density) && covers(mesh,5*density,6*density) && covers(mesh,.5f*density,10*density),"Pixel silhouette must retain each filled stair step");
        for(const auto& vertex:mesh.vertices) require(vertex.position.x==std::round(vertex.position.x) && vertex.position.y==std::round(vertex.position.y),"Pixel silhouette edges must lie on whole raster pixels at every DPI");
        const auto shadow=pixel_mesh(80*density,40*density,4*density,4*density,{12,24,42,248},{0,0,0,128});
        require(covers(shadow,40*density,46*density),"Pixel panel shadow must be a real offset filled shape");
        const auto panel=pixel_rim_mesh(80*density,40*density,3*density,2*density,2*density,{4,37,55,204},{1,12,32,255},{1,11,32,208});
        require(covers(panel,40*density,20*density) && !covers(panel,.5f*density,.5f*density),"Translucent frame must preserve actual cut corners and centre fill");
        bool fill=false,light=false,dark=false;
        for(const auto& vertex:panel.vertices) {
            const auto c=vertex.colour;
            fill=fill || (c.red==4 && c.green==37 && c.blue==55 && c.alpha==204);
            light=light || (c.red==1 && c.green==12 && c.blue==32 && c.alpha==255);
            dark=dark || (c.red==1 && c.green==11 && c.blue==32 && c.alpha==208);
            require(vertex.position.x==std::round(vertex.position.x) && vertex.position.y==std::round(vertex.position.y),"Inset translucent frames must use whole raster edges at both densities");
        }
        require(fill && light && dark,"Dark rim must preserve distinct fill, uniform outline and dark depth alpha values");
        int centre_layers=0;
        for(std::size_t i=0;i+3<panel.vertices.size();i+=4) {
            const auto a=panel.vertices[i].position,b=panel.vertices[i+2].position;
            if(a.x<39.5f*density && b.x>39.5f*density && a.y<19.5f*density && b.y>19.5f*density) ++centre_layers;
        }
        require(centre_layers==1,"A translucent frame centre must blend exactly once; an outer dark shell must not compound its opacity");
        for(const auto& vertex:panel.vertices) require(!(vertex.colour.red==255 && vertex.colour.green==255 && vertex.colour.blue==255),"UI frame must not use a white outline or bevel to create depth");
        const auto tab=pixel_tab_mesh(68*density,44*density,3*density,3*density,6*density,{190,63,37,232},{1,12,32,255},{7,45,88,208});
        require(!covers(tab,5*density,5*density) && covers(tab,10*density,5*density),"Wide low tab face needs a genuinely narrow top, not a rectangular recolor");
        require(!covers(tab,4*density,18*density) && covers(tab,7*density,18*density) && covers(tab,4*density,30*density) && covers(tab,1*density,40*density),"Folder tab shoulders must expand through most of the face height in three raster steps");
        bool open=false;for(std::size_t i=0;i+3<tab.vertices.size();i+=4) {const auto& a=tab.vertices[i];const auto& b=tab.vertices[i+2];if(a.position.x<34*density && b.position.x>34*density && a.position.y<=43*density && b.position.y>43*density)open=a.colour.red==7 && a.colour.green==45 && a.colour.blue==88 && a.colour.alpha==208;}
        require(open,"A tab must have an actual open-bottom join face matching the toolbar, without a bottom outline");
        const auto dock=pixel_rim_mesh(600*density,80*density,6*density,3*density,2*density,{7,45,88,208},{1,12,32,255},{1,11,32,208},8*density,544*density);
        require(!covers(dock,42*density,4.5f*density) && covers(tab,34*density,42.5f*density),"Tab join neck must replace the panel top band rather than blend over its outline or fill");
        require(covers(dock,580*density,4.5f*density),"Panel top outline must remain present outside the exact category join interval");


    }
    // The exact implementation registered for native text must dilate a glyph
    // into full square cells, with no fractional circular diagonal pixels.
    for(int radius:{1,2,4}) {
        Rml::FontGlyph glyph;const Rml::byte centre=255;
        glyph.bitmap_data=&centre;glyph.bitmap_dimensions={1,1};glyph.color_format=Rml::ColorFormat::A8;
        const int width=1+radius*2,stride=width*4+8;
        std::vector<Rml::byte> target(static_cast<std::size_t>(stride*width),73);
        pixel_glyph_dilate(target.data(),{width,width},stride,glyph,radius);
        for(int y=0;y<width;++y) {
            for(int x=0;x<width;++x) require(target[y*stride+x*4+3]==255,"Actual bold/outline dilation must fill even diagonal corners with opaque integer pixels");
            for(int x=width*4;x<stride;++x) require(target[y*stride+x]==73,"Glyph effect must honor byte stride without touching adjacent atlas pixels");
        }
    }

    // A representative counter has two empty source cells between vertical
    // strokes. Directional pigment must leave an empty cell after thickening.
    const std::array<Rml::byte,5> strokes={255,0,0,255,0};
    Rml::FontGlyph counter;counter.bitmap_data=strokes.data();counter.bitmap_dimensions={5,1};counter.color_format=Rml::ColorFormat::A8;
    std::array<Rml::byte,24> bold{};pixel_glyph_bolden(bold.data(),{6,1},24,counter,1);
    require(bold[3]==255 && bold[7]==255 && bold[11]==0 && bold[15]==255 && bold[19]==255 && bold[23]==0,"Actual emboldening must add pigment while preserving a narrow character counter");

}
void material_geometry_contract() {
    for(float density:{1.f,2.f}) {
        const auto panel=pixel_material_mesh(1280*density,800*density,density,3*density,2*density,3*density,{255,255,255,255});
        require(!panel.vertices.empty() && panel.vertices.size()<1024,"Full physical windows must use a bounded tiled mesh, not one quad per raster pixel");
        require(covers(panel,640*density,400*density) && !covers(panel,.5f*density,.5f*density),"Real material windows retain their centre and small cut-corner object silhouette");
        const auto pigment_at=[](const PixelMesh& mesh,float x,float y) {
            for(std::size_t i=0;i+3<mesh.vertices.size();i+=4) {
                const auto& a=mesh.vertices[i];const auto& c=mesh.vertices[i+2];
                if(x>a.position.x && x<c.position.x && y>a.position.y && y<c.position.y)return a.colour.red;
            }
            return Rml::byte{0};
        };
        require(pigment_at(panel,40.5f*density,2.5f*density)==105 && pigment_at(panel,40.5f*density,4.5f*density)==255,"Physical window must have a genuine3dp heavy dark rim before its material face begins");
        require(pigment_at(panel,40.5f*density,795.5f*density)==152 && pigment_at(panel,40.5f*density,792.5f*density)==255,"Physical depth must be a genuine3dp dark lower edge rather than a white highlight");
        int centre_layers=0;
        for(std::size_t i=0;i+3<panel.vertices.size();i+=4) {
            const auto& a=panel.vertices[i];const auto& c=panel.vertices[i+2];
            if(a.position.x<639.5f*density && c.position.x>639.5f*density && a.position.y<399.5f*density && c.position.y>399.5f*density) ++centre_layers;
            require(std::abs((c.tex_coord.x-a.tex_coord.x)*512.f*density-(c.position.x-a.position.x))<.01f && std::abs((c.tex_coord.y-a.tex_coord.y)*512.f*density-(c.position.y-a.position.y))<.01f,"Every production material texel retains its actual dp scale across wide panels and high DPI");
        }
        require(centre_layers==1,"Material colour at the centre must be painted once, without a hidden repeated opacity shell");
        for(const auto& v:panel.vertices) require(v.position.x==std::round(v.position.x) && v.position.y==std::round(v.position.y) && v.tex_coord.x>=0 && v.tex_coord.x<=1 && v.tex_coord.y>=0 && v.tex_coord.y<=1,"Material silhouette and tile seams must retain exact whole raster edges and admitted UV bounds");
        const auto tab=pixel_stone_mesh(72*density,62*density,density,3*density,2*density,6*density,{255,255,255,255},true);
        const auto dock=pixel_stone_mesh(1280*density,88*density,density,3*density,2*density,3*density,{255,255,255,255},false,true);
        require(!covers(tab,3*density,3*density) && covers(tab,10*density,3*density) && covers(tab,36*density,59*density),"Carved tabs need stepped shoulders and a full real rock foot");
        for(float x=.5f;x<1280;x+=8) require(covers(dock,x*density,4.5f*density),"The projecting stone ledge must retain its full visible span beneath all eight tablets");
        require(covers(dock,.5f*density,3.5f*density) && !covers(dock,.5f*density,10.5f*density),"The stone lip must actually project beyond the two-dp body inset, rather than paint a flat line");
        const auto stone=pixel_stone_mesh(1280*density,800*density,density,3*density,2*density,3*density,{255,255,255,255});
        require(!stone.vertices.empty() && stone.vertices.size()<4096,"Stone edge damage must use bounded carved geometry rather than per-texel face geometry");
        int clean_layers=0;
        for(std::size_t i=0;i+3<stone.vertices.size();i+=4) {
            const auto& a=stone.vertices[i];const auto& b=stone.vertices[i+2];
            if(a.position.x<640*density && b.position.x>640*density && a.position.y<400*density && b.position.y>400*density) {
                ++clean_layers;require(a.colour.red==232 && a.colour.green==232 && a.colour.blue==232,"The centre of a dark stone tablet must retain its flat material face shading without face-wide damage");
            }
            if(a.colour.red<180 && b.position.x>12*density && a.position.x<1268*density && b.position.y>24*density && a.position.y<776*density)
                require(false,"Deep stone damage must stay within the physical edge band instead of repeating inside the slab");
            require(std::abs((b.tex_coord.x-a.tex_coord.x)*512.f*density-(b.position.x-a.position.x))<.01f && std::abs((b.tex_coord.y-a.tex_coord.y)*512.f*density-(b.position.y-a.position.y))<.01f,"Dark stone texture must retain one logical texel per dp under its carved geometry");
        }
        require(clean_layers==1,"The dark stone centre must be shaded once");
        require(!covers(stone,75.5f*density,.5f*density) && covers(stone,75.5f*density,3.5f*density),"A worn stone chip must remove actual edge geometry while preserving the face below it");
        // Small physical buttons and tablets also need a genuine upper-edge
        // fracture. Their icon/text centre stays clean; the crack is confined
        // to the shoulder rather than repeated across the material tile.
        for(bool small_tab:{false,true}) for(unsigned seed:{0u,1u}) {
            const float w=(small_tab?72.f:60.f)*density,h=(small_tab?62.f:60.f)*density;
            const auto control=pixel_stone_mesh(w,h,density,3*density,2*density,(small_tab?6.f:3.f)*density,{255,255,255,255},small_tab,false,seed);
            require(pigment_at(control,w*.5f+.5f*density,h*.5f+.5f*density)==232,"Small stone actions must retain a clean shaded centre beneath their artwork or text");
            bool groove=false,open_crack=false;
            for(std::size_t i=0;i+3<control.vertices.size();i+=4) {
                const auto& a=control.vertices[i];const auto& b=control.vertices[i+2];
                if(a.colour.red<180 && a.position.x>6*density && b.position.x<w-6*density && a.position.y>=density && b.position.y<=10*density)groove=true;
            }
            for(float x=8.5f*density;x<w-8*density;x+=density) if(!covers(control,x,.5f*density) && covers(control,x,12.5f*density))open_crack=true;
            require(groove && open_crack,"Each small stone control must have actual dark fracture geometry entering a missing top-edge cell, rather than a flat grey recolour");
        }
    }
    std::set<std::string> families;
    for(const auto& definition:physical_material_definitions()) {
        require(definition.width==512 && definition.height==512 && std::string(definition.texture_source).find("generated/ui/materials/")==0,"Every physical object must use the exact admitted original high-resolution pixel material path");
        require(families.insert(definition.name).second,"Physical object material families must be distinct");
    }
    require(families.size()==9,"Nine concrete real-object materials must exist rather than aliases of one generic slab");
}
struct Rect {float x,y,w,h;};
Rect rect(Ui& ui,const char* id,float density,Rml::Box::Area area=Rml::Box::BORDER) {
    auto* el=ui.context().GetDocument(0)->GetElementById(id); require(el!=nullptr,"Required native element missing");
    const auto p=el->GetAbsoluteOffset(area),s=el->GetBox().GetSize(area);
    return{p.x/density,p.y/density,s.x/density,s.y/density};
}
Render::MaterialSample material_sample(const Render& render,const char* family,Rml::Vector2f point) {
    const auto samples=render.material_samples(family,point);
    require(samples.size()==1,"A real material probe must be painted exactly once, without a missing neck or overlapping dock");
    require(samples[0].alpha==255,"The stone object must retain opaque alpha rather than hide its layered relation with blending");
    return samples[0];
}
void rock_neck_probe(Ui& ui,const Render& render,const char* id,float density) {
    auto* doc=ui.context().GetDocument(0);
    auto* dock=doc->GetElementById("bottom-bar-frame");auto* tab=doc->GetElementById(id);
    const auto anchor=dock->GetAbsoluteOffset(Rml::Box::PADDING).Round();
    const auto origin=tab->GetAbsoluteOffset(Rml::Box::PADDING).Round();
    const auto size=tab->GetBox().GetSize(Rml::Box::PADDING).Round();
    const float span=512.f*density;
    const Rml::Vector2f foot_point{origin.x+size.x*.5f,origin.y+size.y-2.5f*density};
    require(origin.y+size.y<=anchor.y+.15f,"A carved tablet must stand above the continuous lip, never occupy or paint over the ledge");
    const Rml::Vector2f dock_point{foot_point.x,anchor.y+4.5f*density};
    const auto foot=material_sample(render,"rock",foot_point),body=material_sample(render,"rock",dock_point);
    require(std::abs(std::remainder((foot.uv.x-body.uv.x)*span-(foot_point.x-dock_point.x),span))<.04f &&
            std::abs(std::remainder((foot.uv.y-body.uv.y)*span-(foot_point.y-dock_point.y),span))<.04f,
            "Compiled stone feet must retain the current dock grain phase while remaining geometrically above the visible lip");
    const int state_tint=tab->IsPseudoClassSet("hover") || tab->IsPseudoClassSet("focus")?255:tab->IsClassSet("selected")?238:187;
    require(foot.tint.red==state_tint && foot.tint.green==state_tint && foot.tint.blue==state_tint,
            "The actual stone tab foot must inherit the selected or focused face brightness rather than keep a stale pale foot");
    const Rml::Vector2f face_point{origin.x+size.x*.5f,origin.y+20.5f*density};
    const auto face=material_sample(render,"rock",face_point);
    require(face.tint.red==state_tint*232/255 && face.tint.green==face.tint.red && face.tint.blue==face.tint.red,
            "Every actual tab state must draw the same stone family with its own measurable neutral brightness");
    require(std::abs(std::remainder(face.uv.x*span-(face_point.x-origin.x),span))<.04f &&
            std::abs(std::remainder(face.uv.y*span-(face_point.y-origin.y),span))<.04f,
            "Every dark stone tablet state must preserve its original local face UVs");
}
void rock_join_contract(Ui& ui,Render& render,float density) {
    render.reset_draws();ui.render();
    const std::array<const char*,8> tabs={"tab-observe","tab-terrain","tab-life","tab-civilization","tab-construction","tab-economy","tab-world","tab-settings"};
    auto* doc=ui.context().GetDocument(0);auto* dock=doc->GetElementById("bottom-bar-frame");
    const auto anchor=dock->GetAbsoluteOffset(Rml::Box::PADDING).Round();
    const float span=512.f*density;
    const auto horizontal_join=[&](float x) {
        const Rml::Vector2f left{x-.5f*density,anchor.y+4.5f*density},right{x+.5f*density,anchor.y+4.5f*density};
        const auto a=material_sample(render,"rock",left),b=material_sample(render,"rock",right);
        require(std::abs(std::remainder((b.uv.x-a.uv.x)*span-density,span))<.04f &&
                std::abs(std::remainder((b.uv.y-a.uv.y)*span,span))<.04f,
                "The uninterrupted real stone lip must advance one texel across tablet boundaries and512-texel tile seams");
    };
    for(std::size_t i=0;i<tabs.size();++i) {
        rock_neck_probe(ui,render,tabs[i],density);
        if(i)horizontal_join(doc->GetElementById(tabs[i])->GetAbsoluteOffset(Rml::Box::PADDING).Round().x);
    }
    auto* last=doc->GetElementById(tabs.back());
    const float last_right=last->GetAbsoluteOffset(Rml::Box::PADDING).Round().x+last->GetBox().GetSize(Rml::Box::PADDING).Round().x;
    if(anchor.x+span<last_right-.5f*density) horizontal_join(anchor.x+span);
    const float dock_width=dock->GetBox().GetSize(Rml::Box::PADDING).Round().x;
    for(float x=.5f*density;x<dock_width;x+=8.f*density)material_sample(render,"rock",{anchor.x+x,anchor.y+4.5f*density});
    require(render.compiled_draws>0,"Stone lip probes must include actual compiled geometry rendering, not only the immediate fallback path");
}
void rock_neck_cache_contract(Ui& ui,Render& render,int width,int height,float density) {
    rock_join_contract(ui,render,density);
    const auto compiled_before=render.next_geometry;
    rock_join_contract(ui,render,density);
    require(render.next_geometry==compiled_before,"An unchanged dock phase must reuse its existing compiled material cache");
    auto* doc=ui.context().GetDocument(0);auto* tab=doc->GetElementById("tab-observe");auto* dock=doc->GetElementById("bottom-bar-frame");
    const auto before=tab->GetAbsoluteOffset(Rml::Box::PADDING).Round();
    tab->SetProperty("position","relative");tab->SetProperty("left","13dp");tab->SetProperty("top","-2dp");ui.update();
    const auto moved=tab->GetAbsoluteOffset(Rml::Box::PADDING).Round();
    require(std::abs(moved.x-before.x-13.f*density)<.15f && std::abs(moved.y-before.y+2.f*density)<.15f,"Cache invalidation probe must actually translate an existing tab relative to its dock");
    render.reset_draws();ui.render();rock_neck_probe(ui,render,"tab-observe",density);
    dock->SetProperty("left","17dp");ui.update();
    render.reset_draws();ui.render();rock_neck_probe(ui,render,"tab-observe",density);
    dock->RemoveProperty("left");tab->RemoveProperty("position");tab->RemoveProperty("left");tab->RemoveProperty("top");ui.update();
    rock_join_contract(ui,render,density);
    const int alternate_width=width==1280?640:1280;
    ui.resize(static_cast<int>(alternate_width*density),static_cast<int>((height+16)*density),density);ui.update();
    rock_join_contract(ui,render,density);
    ui.resize(static_cast<int>(width*density),static_cast<int>(height*density),density);ui.update();
    rock_join_contract(ui,render,density);
    require(ui.take_actions().empty(),"Material phase maintenance, movement and resize must never become a World command");
}
std::string visible_text(Rml::Element* element) {
    if(element->GetComputedValues().display()==Rml::Style::Display::None)return {};
    if(auto* text=dynamic_cast<Rml::ElementText*>(element))return text->GetText();
    std::string value;
    for(int i=0;i<element->GetNumChildren();++i)value+=visible_text(element->GetChild(i));
    return value;
}
float actual_glyph_width(Rml::Element* element,const std::string& value,float density) {
    require(element!=nullptr,"Actual glyph measurement needs a native element");
    auto* engine=Rml::GetFontEngineInterface();require(engine!=nullptr,"Actual glyph measurement needs the native font engine");
    const auto face=element->GetFontFaceHandle();require(face!=0,"Actual glyph measurement must never query a missing native face");
    return engine->GetStringWidth(face,value)/density;
}
void ink_contract(Rml::Element* element,int red,int green,int blue) {
    require(element!=nullptr,"Readable material text requires a real native element");
    const auto colour=element->GetComputedValues().color();
    require(colour.red==red && colour.green==green && colour.blue==blue && colour.alpha==255,"Text must use the actual warm grey ink assigned to its dark stone or recessed control surface");
}
void stone_heading_contract(Rml::Element* element,float density) {
    ink_contract(element,238,234,224);
    require(element->GetComputedValues().font_size()==24*density,"Dark stone headings retain the real large24dp typeface");
    const auto effects=element->GetProperty<Rml::FontEffectsPtr>("font-effect");
    require(effects && effects->list.size()==1,"A dark stone heading must use solid warm grey pigment without a white or yellow outline");
    const auto pigment=effects->list[0]->GetColour();
    require(pigment.red==238 && pigment.green==234 && pigment.blue==224 && pigment.alpha==255,"The actual stone heading weight must carry the same bright warm grey as its glyphs");
    Rml::FontGlyph glyph;Rml::Vector2i origin{0,0},size{1,1};
    require(effects->list[0]->GetGlyphMetrics(origin,size,glyph) && origin.x==0 && origin.y==0 &&
            size.x==1+static_cast<int>(std::round(density)) && size.y==1,
            "The single warm grey stone pigment must have actual density-scaled directional weight while preserving character counters");
}
int stone_element_tint(Ui& ui,const Render& render,const char* id,float density) {
    auto* element=ui.context().GetDocument(0)->GetElementById(id);
    const auto origin=element->GetAbsoluteOffset(Rml::Box::PADDING).Round();
    const auto size=element->GetBox().GetSize(Rml::Box::PADDING).Round();
    const auto samples=render.material_samples("rock",{origin.x+size.x*.5f,origin.y+size.y*.5f},&origin);
    require(samples.size()==1,"The particular native control must actually draw stone, independently of its stone parent or another selected control");
    const auto tint=samples[0].tint;
    require(tint.red==tint.green && tint.green==tint.blue,"Stone control states must use neutral material brightness rather than coloured plaques");
    require(size.y>20*density,"Stone control probe must address a real formatted control");
    return tint.red;
}
void stone_surface_contract(Ui& ui,const Render& render,std::initializer_list<const char*> ids) {
    for(const char* id:ids) {
        auto* element=ui.context().GetDocument(0)->GetElementById(id);
        require(element && element->IsVisible(true),"Stone surface proof requires the actual visible native object");
        require(render.material_at("rock",element->GetAbsoluteOffset(Rml::Box::PADDING).Round()),"Every particular visible panel or control must submit its own stone material geometry, independently of other stone objects");
    }
    for(const auto& family:render.materials_drawn) require(family=="rock" || family=="obsidian","Visible surfaces must use shared dark stone or recessed obsidian, never the previous coloured plaque families");
}
void filled_action_contract(Ui& ui,const char* id,float density) {
    auto* element=ui.context().GetDocument(0)->GetElementById(id);
    require(element && element->IsVisible(true),"Filled action must be a real visible native control");
    const auto box=rect(ui,id,density),content=rect(ui,id,density,Rml::Box::CONTENT);
    const float font=element->GetComputedValues().font_size()/density;
    require(font==24 && font/box.h>=.34f,"Important execution words must actually use large doubled pixel glyphs and occupy their plaque height");
    float icon_width=0,icon_margin=0;
    for(int i=0;i<element->GetNumChildren();++i) if(element->GetChild(i)->GetTagName()=="img") {
        auto* image=element->GetChild(i);const auto size=image->GetBox().GetSize(Rml::Box::BORDER);
        require(std::abs(size.x/density-48)<.15f && std::abs(size.y/density-48)<.15f,"Action must render its actual48px original object silhouette at exact integer density");
        icon_width=size.x/density;icon_margin=image->GetBox().GetEdge(Rml::Box::MARGIN,Rml::Box::RIGHT)/density;
    }
    const float glyph_width=actual_glyph_width(element,visible_text(element),density);
    require(glyph_width>0 && glyph_width+icon_width+icon_margin<=content.w+.6f,"Actual localized glyph advances and object artwork must fit their painted plaque content; CSS image size alone is insufficient");
    require(content.x>=box.x+3.f-.15f && content.y>=box.y+3.f-.15f && content.x+content.w<=box.x+box.w-3.f+.15f,"Action content must clear its genuine carved material rim");
}
void object_icon_contract(Ui& ui,const char* id,float density,float minimum_fill=.75f) {
    auto* element=ui.context().GetDocument(0)->GetElementById(id);
    auto* icon=element->GetChild(0);require(icon && icon->GetTagName()=="img","Object tool must contain its actual original pixel illustration");
    const auto box=rect(ui,id,density);
    const auto position=icon->GetAbsoluteOffset(Rml::Box::BORDER),size=icon->GetBox().GetSize(Rml::Box::BORDER);
    require(std::abs(size.x/density-48)<.15f && std::abs(size.y/density-48)<.15f && size.y/density/box.h>=minimum_fill,"Tool artwork must fill its painted hit object instead of floating in an oversized empty square");
    require(position.x/density>=box.x-.15f && position.y/density>=box.y-.15f && (position.x+size.x)/density<=box.x+box.w+.15f && (position.y+size.y)/density<=box.y+box.h+.15f,"Actual native object sprite must stay inside its physical painted control at every density");
}
void layout_contract(Ui& ui) {
    const auto visit=[&](auto&& self,Rml::Element* element)->void {
        const auto& computed=element->GetComputedValues();
        if(computed.display()==Rml::Style::Display::None) return;
        if(computed.display()==Rml::Style::Display::Flex) {
            const auto* parent=element->GetParentNode();
            if(computed.position()==Rml::Style::Position::Absolute || computed.position()==Rml::Style::Position::Fixed || computed.float_()!=Rml::Style::Float::None || (parent && parent->GetComputedValues().display()==Rml::Style::Display::Flex))
                throw std::runtime_error("Unsupported RmlUi 5 flex formatting: "+element->GetAddress());
        }
        if(element->GetTagName()=="button") {
            const auto size=element->GetBox().GetSize(Rml::Box::BORDER);
            if(size.x<=0 || size.y<=0) throw std::runtime_error("Visible native button has no formatted geometry: "+element->GetAddress());
        }
        for(int i=0;i<element->GetNumChildren(true);++i) self(self,element->GetChild(i));
    };
    visit(visit,ui.context().GetDocument(0));
}
void click(Ui& ui,const char* id,float density,Render* state_render=nullptr) {
    auto* element=ui.context().GetDocument(0)->GetElementById(id);
    require(element!=nullptr,"Real pointer target must exist");
    element->ScrollIntoView(false);ui.update();layout_contract(ui);
    const auto r=rect(ui,id,density);
    require(r.w>0 && r.h>0,"Real pointer target must have nonzero geometry");
    const Rml::Vector2f point{(r.x+r.w/2)*density,(r.y+r.h/2)*density};
    const auto viewport=ui.context().GetDimensions();
    require(point.x>=0 && point.y>=0 && point.x<viewport.x && point.y<viewport.y,"Real pointer target must be reachable inside the physical viewport");
    auto* hit=ui.context().GetElementAtPoint(point);
    while(hit && hit!=element) hit=hit->GetParentNode();
    require(hit==element,"The visible button centre must actually hit the intended native control");
    ui.context().ProcessMouseMove(static_cast<int>(point.x),static_cast<int>(point.y),0);
    ui.context().ProcessMouseButtonDown(0,0);
    if(state_render) {
        ui.update();state_render->reset_draws();ui.render();
        require(element->IsPseudoClassSet("active") && stone_element_tint(ui,*state_render,id,density)==140*232/255,
                "A real pointer press must darken the same stone for ordinary, primary and selected tab controls");
    }
    ui.context().ProcessMouseButtonUp(0,0);ui.update();layout_contract(ui);
    if(state_render) {
        state_render->reset_draws();ui.render();
        require(!element->IsPseudoClassSet("active") && stone_element_tint(ui,*state_render,id,density)==232,
                "Pointer release must restore the brighter focused stone rather than leave the pressed or coloured plaque state");
    }
}
void hit_layer_contract(Ui& ui,const char* point_id,const char* layer_id,float density) {
    auto* layer=ui.context().GetDocument(0)->GetElementById(layer_id);
    require(layer && layer->IsVisible(true),"Expected top interactive layer must actually be visible");
    const auto point_rect=rect(ui,point_id,density);
    require(point_rect.w>0 && point_rect.h>0,"Layer hit probe requires real formatted bounds");
    const Rml::Vector2f point{(point_rect.x+point_rect.w/2)*density,(point_rect.y+point_rect.h/2)*density};
    const auto viewport=ui.context().GetDimensions();
    require(point.x>=0 && point.y>=0 && point.x<viewport.x && point.y<viewport.y,"Layer probe must be inside the actual physical viewport");
    ui.context().ProcessMouseMove(static_cast<int>(point.x),static_cast<int>(point.y),0);ui.update();
    auto* hit=ui.context().GetElementAtPoint(point);
    while(hit && hit!=layer) hit=hit->GetParentNode();
    require(hit==layer,"Actual pointer must hit the intended front layer, never a raised tab or form behind its modal shield");
}
void error_layer_contract(Ui& ui,float density) {
    const auto viewport=ui.context().GetDimensions();
    auto* panel=ui.context().GetDocument(0)->GetElementById("error-panel");
    require(panel && panel->IsVisible(true),"Business/local error requires a real visible native modal");
    const auto r=rect(ui,"error-panel",density);
    require(r.w>0 && r.h>0 && r.x>=0 && r.y>=0 && (r.x+r.w)*density<=viewport.x+.5f && (r.y+r.h)*density<=viewport.y+.5f,"Actual error window bounds must fit the viewport at every locale and density");
    for(const auto* id:{"error-message","error-dismiss"}) {
        auto* element=ui.context().GetDocument(0)->GetElementById(id);
        element->ScrollIntoView(false);ui.update();
        const auto child=rect(ui,id,density);
        require(child.x>=r.x-.15f && child.y>=r.y-.15f && child.x+child.w<=r.x+r.w+.15f && child.y+child.h<=r.y+r.h+.15f,"Error reason and acknowledgement must be genuinely reachable inside the visible window");
        hit_layer_contract(ui,id,id,density);
    }
}
void preview_tray_contract(Ui& ui,float density) {
    auto* panel=ui.context().GetDocument(0)->GetElementById("creation-controls");
    require(panel->GetScrollHeight()<=panel->GetClientHeight()+.5f && panel->GetScrollWidth()<=panel->GetClientWidth()+.5f,"Collapsed creation tray must fit its real content, not hide overflow or expose a needless scrollbar");
    require(panel->GetScrollTop()==0 && panel->GetScrollLeft()==0,"Preview tray must begin at the true content origin");
    for(int i=0;i<panel->GetNumChildren(true);++i) {
        auto* child=panel->GetChild(i);
        if(child->GetTagName()=="scrollbarvertical" || child->GetTagName()=="scrollbarhorizontal")
            require(!child->IsVisible(true),"Collapsed tray must have no actual visible native scrollbar");
    }
    const auto content=rect(ui,"creation-controls",density,Rml::Box::CONTENT),create=rect(ui,"create",density);
    require(create.x>=content.x-.15f && create.y>=content.y-.15f && create.x+create.w<=content.x+content.w+.15f && create.y+create.h<=content.y+content.h+.15f,"Entire preview Create button must fit inside tray content without clipping its label or icon");
    auto* action=ui.context().GetDocument(0)->GetElementById("create");
    const float glyph_width=actual_glyph_width(action,visible_text(action),density);
    const auto tray=rect(ui,"creation-controls",density);const float viewport_width=ui.context().GetDimensions().x/density;
    const float expected_width=std::min(std::clamp(std::ceil((glyph_width+100.f)/8.f)*8.f,208.f,480.f),viewport_width-32.f);
    require(std::abs(tray.w-expected_width)<.15f && (glyph_width+48.f+8.f)/create.w>=.55f,"Collapsed physical creation tray must fit its actual large localized words and object icon without a huge empty plaque");
    filled_action_contract(ui,"create",density);
    hit_layer_contract(ui,"create","create",density);
}
Action single(Ui& ui,ActionKind expected) {
    auto actions=ui.take_actions();require(actions.size()==1,"Exactly one typed intention should leave one interaction");require(actions[0].kind==expected,"Unexpected application command");return actions[0];
}
void outline_contract(Rml::Element* element,float density,bool title=false,bool red=false) {
    require(element!=nullptr,"Important heading must exist");
    const auto effects=element->GetProperty<Rml::FontEffectsPtr>("font-effect");
    require(effects && effects->list.size()==2,"Actual heading must have separate white silhouette and bold pigment layers");
    const std::array<int,2> radii={static_cast<int>(std::round(2.f*density)),static_cast<int>(std::round(density))};
    for(std::size_t i=0;i<2;++i) {
        Rml::FontGlyph glyph;Rml::Vector2i origin{0,0},size{1,1};
        require(effects->list[i]->GetGlyphMetrics(origin,size,glyph),"Instanced glyph effect must provide actual expanded bitmap metrics");
        if(i==1 && !title) require(origin.x==0 && origin.y==0 && size.x==1+radii[i] && size.y==1,"Actual heading pigment must use density-scaled one-sided emboldening that preserves CJK counters");
        else require(origin.x==-radii[i] && origin.y==-radii[i] && size.x==1+2*radii[i] && size.y==1+2*radii[i],"Native effect radius must scale in physical whole pixels at each display density");
        const auto color=effects->list[i]->GetColour();
        require(color.alpha==255 && (i==0?(color.red==255 && color.green==255 && color.blue==255):red?(color.red==200 && color.green==58 && color.blue==41):(color.red==217 && color.green==145 && color.blue==0)),"Bold pigment and white outer edge must retain their distinct actual glyph layer colors");
    }
    // Metrics alone can pass while equal-size pigment completely hides the
    // white layer. Compose the actual instanced masks in their shared glyph
    // coordinates and require a genuinely visible, limited outer contour.
    const Rml::byte ink=255;Rml::FontGlyph glyph;
    glyph.bitmap_data=&ink;glyph.bitmap_dimensions={1,1};glyph.color_format=Rml::ColorFormat::A8;
    std::array<Rml::Vector2i,2> origins{{Rml::Vector2i(0,0),Rml::Vector2i(0,0)}};
    std::array<Rml::Vector2i,2> dimensions{{Rml::Vector2i(1,1),Rml::Vector2i(1,1)}};
    std::array<std::vector<Rml::byte>,2> masks;
    for(int i=0;i<2;++i) {
        require(effects->list[i]->GetGlyphMetrics(origins[i],dimensions[i],glyph),"Actual glyph mask metrics must instantiate");
        masks[i].resize(static_cast<std::size_t>(dimensions[i].x*dimensions[i].y*4));
        effects->list[i]->GenerateGlyphTexture(masks[i].data(),dimensions[i],dimensions[i].x*4,glyph);
    }
    int visible_white=0;
    for(int y=0;y<dimensions[0].y;++y)for(int x=0;x<dimensions[0].x;++x) {
        const int px=origins[0].x+x-origins[1].x,py=origins[0].y+y-origins[1].y;
        const bool pigment=px>=0 && py>=0 && px<dimensions[1].x && py<dimensions[1].y && masks[1][static_cast<std::size_t>((py*dimensions[1].x+px)*4+3)]>=128;
        if(masks[0][static_cast<std::size_t>((y*dimensions[0].x+x)*4+3)]>=128 && !pigment)++visible_white;
    }
    require(visible_white>0,"Important glyph contour must remain visibly outside the shaded pigment, not be hidden by equal-radius layers");
}
void scenario(const std::filesystem::path& root,int width,int height,float density,Render& render) {
    Ui ui(root);std::string error;require(ui.initialize(static_cast<int>(width*density),static_cast<int>(height*density),density,error),"Native UI initialization failed");
    View view;view.creation.generation=17;view.creation.name="Welt";view.can_continue=false;ui.set_view(view);ui.update();
    for(const auto locale:{Locale::Chinese,Locale::English,Locale::German}) {
        view.locale=locale;ui.set_view(view);ui.update();layout_contract(ui);
        const auto menu=rect(ui,"main-menu",density),heading=rect(ui,"main-title",density),first=rect(ui,"new-world",density),second=rect(ui,"continue-world",density),last=rect(ui,"exit",density);
        require(menu.x>=0 && menu.y>=0 && menu.x+menu.w<=width+.1 && menu.y+menu.h<=height+.1,"Compact home operations must fit all supported language/viewports");
        require(first.h>=40 && last.h>=40 && first.y>=heading.y+heading.h+8,"Compact home keeps accessible controls and a separate title band");
        require(last.y+last.h<=height+.15,"All five home operations must be immediately visible even at minimum resolution");
        require(second.y>first.y && std::abs(second.x-first.x)<.15,"Home keeps a compact five-operation column across locales and viewports");
        for(const auto* id:{"new-world","continue-world","load","settings-main","exit"}) filled_action_contract(ui,id,density);
        require(ui.context().GetDocument(0)->GetProperty<Rml::String>("font-family")=="Sonn Arcade","Every language must use the admitted arcade font family");
        require(ui.context().GetDocument(0)->GetElementById("new-world")->GetComputedValues().font_size()==24*density,"Home actions must use actual doubled 24dp pixel glyphs");
        require(ui.context().GetDocument(0)->GetElementById("main-title")->GetComputedValues().font_size()==36*density,"Main hierarchy uses a real three-times pixel font size");
        require(ui.context().GetDocument(0)->GetElementById("main-title")->GetComputedValues().font_weight()==Rml::Style::FontWeight::Normal,"Actual glyph emboldening must keep the real Regular face, with independent pigment and outline effects");
        auto* title=ui.context().GetDocument(0)->GetElementById("main-title");outline_contract(title,density,true);
        for(int i=0;i<title->GetNumChildren();++i) if(title->GetChild(i)->GetTagName()=="span") outline_contract(title->GetChild(i),density,true);
        const float word_width=actual_glyph_width(title,"SONNHEIDE",density)+6.f;
        require(word_width<=rect(ui,"main-menu",density,Rml::Box::CONTENT).w,"Actual36dp wordmark advances and white glyph edge must fit its menu content width");
        render.reset_draws();ui.render();stone_surface_contract(ui,render,{"main-menu","new-world","continue-world","load","settings-main","exit"});

    }
    render.reset_draws();ui.render();require(render.bold_pigment && render.white_glyph_outline,"Native important titles must submit actual yellow bold glyph and white outline geometry");
    require(ui.context().GetDocument(0)->GetElementById("main-title")->GetComputedValues().has_font_effect(),"Important title must use the registered actual glyph effect");
    require(!ui.blocks_world_input(),"Ordinary menu/world surface is not a modal input barrier");
    click(ui,"continue-world",density);require(ui.take_actions().empty(),"Unavailable Continue must not issue a fake load");
    click(ui,"new-world",density);single(ui,ActionKind::NewWorld);
    view.screen=Screen::Creation;ui.set_view(view);ui.update();
    for(int size:{256,512,1024,2048,4096}) {
        const auto id="size-"+std::to_string(size);click(ui,id.c_str(),density,size==4096?&render:nullptr);
        const auto preset=single(ui,ActionKind::SetCreationSize);
        require(preset.a==size && preset.b==size && preset.generation==17,"A physical-size preset must emit one generation-bound real width/length intent");
    }
    for(const auto locale:{Locale::Chinese,Locale::English,Locale::German}) {
        view.locale=locale;ui.set_view(view);ui.update();layout_contract(ui);
        const auto dialog=rect(ui,"creation-controls",density);
        require(std::abs(dialog.x+dialog.w/2-width/2.f)<=.6f && std::abs(dialog.y+dialog.h/2-height/2.f)<=.6f,"Creation must be one genuinely centered dialog in every locale, viewport and DPI");
        require(dialog.x>=15.5f && dialog.y>=15.5f && dialog.x+dialog.w<=width-15.5f && dialog.y+dialog.h<=height-15.5f,"Centered creation dialog must respect viewport safe margins and real scrolling");
        require(ui.blocks_world_input(),"Centered creation form must shield background camera actions");
        auto* form=ui.context().GetDocument(0)->GetElementById("creation-controls");
        stone_heading_contract(ui.context().GetDocument(0)->GetElementById("creation-heading")->GetChild(0),density);
        ink_contract(ui.context().GetDocument(0)->GetElementById("creation-population"),222,219,212);
        ink_contract(ui.context().GetDocument(0)->GetElementById("creation-scale"),181,179,173);
        ink_contract(ui.context().GetDocument(0)->GetElementById("creation-theme-label"),222,219,212);
        ink_contract(ui.context().GetDocument(0)->GetElementById("creation-name"),222,219,212);
        ink_contract(ui.context().GetDocument(0)->GetElementById("preview"),222,219,212);
        if(height<=480) require(form->GetScrollHeight()>form->GetClientHeight()+.5f,"The complete centered creation form must retain real scrolling in a short viewport");
        const auto scene=ui.viewport();require(scene.x==0 && scene.y==0 && std::abs(scene.width-width)<.15f && std::abs(scene.height-height)<.15f,"Blank creation uses the complete 3D scene behind its dialog");
        auto* doc=ui.context().GetDocument(0);
        for(const auto* id:{"preview","create"}) filled_action_contract(ui,id,density);
        for(const auto* removed:{"earth-mode","earth-controls","earth-selection","map-controls","map-in","earth-west"}) require(doc->GetElementById(removed)==nullptr,"World-map entry, controls and selection must be removed rather than hidden");
    }
    auto* creation_name=ui.context().GetDocument(0)->GetElementById("creation-name");creation_name->Focus();
    for(int step=0;step<20;++step) {
        ui.context().ProcessKeyDown(Rml::Input::KI_TAB,0);ui.context().ProcessKeyUp(Rml::Input::KI_TAB,0);ui.update();
        auto* focus=ui.context().GetFocusElement();require(focus!=nullptr,"Centered creation Tab cycle must retain a real focus target");
        auto* ancestor=focus;
        while(ancestor && ancestor->GetId()!="creation-controls") { require(ancestor->GetComputedValues().display()!=Rml::Style::Display::None,"Creation keyboard cycle must never focus a hidden theme or preview control");ancestor=ancestor->GetParentNode(); }
        require(ancestor!=nullptr,"Creation keyboard focus must remain inside the actual centered dialog");
    }
    render.reset_draws();ui.render();
    require(render.materials_drawn==std::set<std::string>{"obsidian","rock"},"Actual creation must use only dark stone surfaces with recessed obsidian inputs");
    stone_surface_contract(ui,render,{"creation-controls"});
    view.creation.can_select_theme=true;view.creation.theme="maple_field";ui.set_view(view);ui.update();
    require(ui.take_actions().empty(),"Snapshot/theme synchronization must not issue a player action or invalidate its preview");
    auto* theme=dynamic_cast<Rml::ElementFormControlSelect*>(ui.context().GetDocument(0)->GetElementById("creation-theme"));
    require(theme && theme->GetNumOptions()==8 && theme->GetValue()=="maple_field","Native theme selector must reflect authoritative snapshot");
    theme->Focus();ui.context().ProcessKeyDown(Rml::Input::KI_DOWN,0);ui.context().ProcessKeyUp(Rml::Input::KI_DOWN,0);ui.update();
    const auto selected=single(ui,ActionKind::SetCreationTheme);
    require(selected.text=="cherry_field" && selected.generation==17,"Real dropdown keyboard selection must emit one generation-bound theme intention");
    view.creation.theme=selected.text;
    const std::array<std::array<const char*,8>,3> theme_names={{
        {{"雪原","花甸","枫原","樱野","湿地","荒原","圣原","炎地"}},
        {{"Snowfield","Flower meadow","Maple plain","Cherry field","Wetland","Wasteland","Sacred land","Ember land"}},
        {{"Schneefeld","Blumenwiese","Ahornland","Kirschland","Feuchtgebiet","Ödland","Heiliges Land","Glutland"}}
    }};
    for(const auto locale:{Locale::Chinese,Locale::English,Locale::German}) {
        view.locale=locale;ui.set_view(view);ui.update();layout_contract(ui);
        for(int i=0;i<theme->GetNumOptions();++i) require(theme->GetOption(i)->GetInnerRML()==theme_names[static_cast<std::size_t>(locale)][i],"Non-DOM dropdown options must translate in every locale");
        Rml::Element* selected_label=nullptr;
        for(int i=0;i<theme->GetNumChildren(true);++i) if(theme->GetChild(i)->GetTagName()=="selectvalue") selected_label=theme->GetChild(i);
        require(selected_label && selected_label->GetInnerRML()==theme_names[static_cast<std::size_t>(locale)][3],"Visible selected theme label must refresh without changing its selection");
        require(theme->GetValue()=="cherry_field" && ui.take_actions().empty(),"Localization must preserve selection and never become a theme command");
        const auto soil=rect(ui,"blank-soil",density),ocean=rect(ui,"blank-ocean",density);
        require(ocean.y>=soil.y && soil.w>0 && ocean.w>0,"Blank creation retains both actual soil and ocean choices");
    }
    ui.activate("create");require(ui.take_actions().empty(),"Create remains unavailable before real preview");
    auto* width_input=dynamic_cast<Rml::ElementFormControl*>(ui.context().GetDocument(0)->GetElementById("creation-width"));
    for(const auto locale:{Locale::Chinese,Locale::English,Locale::German}) {
        view.locale=locale;ui.set_view(view);ui.update();
        width_input->SetValue("x");ui.take_actions();click(ui,"preview",density);
        require(ui.blocks_world_input() && ui.take_actions().empty(),"Local numeric validation error is a modal barrier, not a core preview command");
        error_layer_contract(ui,density);
        hit_layer_contract(ui,"creation-controls","error-overlay",density);
        click(ui,"error-dismiss",density);
        require(ui.blocks_world_input() && ui.take_actions().empty(),"Real error acknowledgement returns to centered creation without a World command");
        require(!ui.context().GetDocument(0)->GetElementById("error-overlay")->IsVisible(true),"Actual pointer acknowledgement must remove the local error overlay");
        auto* focus=ui.context().GetFocusElement();
        while(focus && focus->GetId()!="creation-controls") focus=focus->GetParentNode();
        require(focus!=nullptr,"Error acknowledgement must restore keyboard focus to the actual creation form");
    }
    width_input->SetValue("128");ui.take_actions();
    ui.activate("preview");require(single(ui,ActionKind::RequestPreview).generation==17,"Preview must retain actual candidate generation");
    view.creation.preview_ready=true;view.creation.preview_token=23;ui.set_view(view);ui.update();
    require(!ui.blocks_world_input() && !ui.text_input_focused(),"Successful preview must expose camera and move focus out of the hidden form");
    const auto tray=rect(ui,"creation-controls",density);require(tray.h<=96.1f && tray.y>height/2.f,"Preview replaces the centered form with a compact bottom action tray");
    preview_tray_contract(ui,density);
    for(const auto locale:{Locale::Chinese,Locale::English,Locale::German}) {
        view.locale=locale;ui.set_view(view);ui.update();preview_tray_contract(ui,density);
    }
    click(ui,"creation-options",density);require(ui.take_actions().empty() && ui.blocks_world_input(),"Preview editing reopens the same centered form through a local control");
    const auto reopened=rect(ui,"creation-controls",density);require(std::abs(reopened.x+reopened.w/2-width/2.f)<=.6f && std::abs(reopened.y+reopened.h/2-height/2.f)<=.6f,"Preview edits must reopen an actually centered dialog");
    view.creation.preview_ready=false;ui.set_view(view);ui.update();require(ui.blocks_world_input(),"Invalidating a preview must retain the centered editable form");
    view.creation.preview_ready=true;ui.set_view(view);ui.update();
    preview_tray_contract(ui,density);
    render.reset_draws();ui.render();stone_surface_contract(ui,render,{"creation-controls","preview-caption","create"});
    click(ui,"create",density,&render);const auto create=single(ui,ActionKind::CreateWorld);require(create.generation==17 && create.token==23,"Create must commit the displayed preview only");
    ui.activate("cancel-create");single(ui,ActionKind::CancelCreation);
    view.screen=Screen::World;view.world.session=44;view.world.name="世界 <new>";view.world.cells=16384;view.world.dry_cells=100;ui.set_view(view);ui.update();
    click(ui,"tab-observe",density,&render);single(ui,ActionKind::SelectBottomSection);
    const std::array<const char*,8> tabs={"tab-observe","tab-terrain","tab-life","tab-civilization","tab-construction","tab-economy","tab-world","tab-settings"};
    for(const auto locale:{Locale::Chinese,Locale::English,Locale::German}) {
        view.locale=locale;ui.set_view(view);ui.update();layout_contract(ui);
        rock_join_contract(ui,render,density);
        const auto bar=rect(ui,"bottom-bar",density,Rml::Box::CONTENT);
        const float pixel_tolerance=1.1f/density;
        const auto outer_bar=rect(ui,"bottom-bar-frame",density);
        require(std::abs(outer_bar.y+outer_bar.h-height)<pixel_tolerance,"Bottom section toolbar must remain anchored to the viewport edge through its absolute wrapper");
        for(int i=0;i<8;++i) {
            const auto button=rect(ui,tabs[i],density),row=rect(ui,"section-tabs",density);
            require(button.y>=row.y-.15f && button.y+button.h<=outer_bar.y+.15f,"Tablets must sit above the continuous projecting stone ledge without overpainting it");
            require(std::abs(button.y+button.h-outer_bar.y)<.15f && button.h<=62.15f,"Low58/62dp tablets must end exactly at the ledge top, leaving its full six-dp face visible");
            auto* panel=ui.context().GetDocument(0)->GetElementById("bottom-bar-frame");
            auto* parent=ui.context().GetDocument(0)->GetElementById(tabs[i]);
            while(parent && parent!=panel) parent=parent->GetParentNode();
            require(parent!=panel,"Categories must be separate protrusions, never contained inside the decorated toolbar panel");
            require(button.x>=bar.x-pixel_tolerance && button.x+button.w<=bar.x+bar.w+pixel_tolerance && button.y+button.h<=height+pixel_tolerance,"Bottom button padding must not overflow the viewport or content row");
            require(button.w>=58-pixel_tolerance && button.w<=72+pixel_tolerance,"All eight protruding categories must have substantial bounded slots that fit the viewport");
            auto* element=ui.context().GetDocument(0)->GetElementById(tabs[i]);
            const auto icon=element->GetChild(0)->GetBox().GetSize(Rml::Box::BORDER);
            require(std::abs(icon.x/density-48)<.15 && std::abs(icon.y/density-48)<.15,"Category silhouettes must render every actual48px logical cell at whole1×/2× scale");
            require(element->GetComputedValues().font_size()==12*density,"Bottom text remains a real compact 12dp pixel font at every DPI");
            require(!element->GetAttribute<Rml::String>("data-tooltip-key","").empty(),"Short toolbar labels require their complete localized name tooltip");
            object_icon_contract(ui,tabs[i],density,.77f);
            const auto image_offset=element->GetChild(0)->GetAbsoluteOffset(Rml::Box::BORDER);
            require(image_offset.y/density+48.f<=button.y+button.h-6.f+.15f,"Actual48px category illustration must fit wholly on its carved face, never across the six-dp rock neck");
            require(std::abs(button.h-(element->IsClassSet("selected")?62.f:58.f))<.15f,"Selected and ordinary carved page dimensions must be actual62/58dp native geometry");

        }
        require(ui.take_actions().empty(),"Layout/localization queries must not emit commands");
    }
    rock_neck_cache_contract(ui,render,width,height,density);
    const auto observe_panel=rect(ui,"section-tools",density),camera=rect(ui,"camera-home",density),help=rect(ui,"camera-help",density);
    require(observe_panel.w<=width+.15f && std::abs(camera.w-60)<.15f && std::abs(help.w-60)<.15f && std::abs(camera.y-help.y)<.15f,"Observe tools must occupy60dp physical slots with48dp object silhouettes");
    object_icon_contract(ui,"camera-home",density);object_icon_contract(ui,"camera-help",density);
    const auto civil=rect(ui,"tab-civilization",density);
    ui.context().ProcessMouseMove(static_cast<int>((civil.x+civil.w/2)*density),static_cast<int>((civil.y+civil.h/2)*density),0);
    ui.update();
    auto* tooltip=ui.context().GetDocument(0)->GetElementById("tool-tooltip");
    require(tooltip->GetComputedValues().display()!=Rml::Style::Display::None && tooltip->GetInnerRML().find("Zivilisation und Institutionen")!=std::string::npos,"Short German toolbar label must expose its real full name on native hover");
    const auto tip=rect(ui,"tool-tooltip",density);
    require(tip.x>=0 && tip.y>=0 && tip.x+tip.w<=width+.15 && tip.y+tip.h<=height+.15,"Native full-name tooltip must stay inside the viewport");
    render.reset_draws();ui.render();require(render.material_at("rock",tooltip->GetAbsoluteOffset(Rml::Box::PADDING).Round()),"Native full-name tooltip must actually draw the shared dark stone material at its own panel");
    require(ui.take_actions().empty(),"Tooltip presentation must never become a World command");
    ui.context().ProcessMouseMove(0,0,0);ui.update();
    require(tooltip->GetComputedValues().display()==Rml::Style::Display::None,"Leaving a tool must dismiss its informational tooltip");
    const std::array<const char*,5> future_tabs={"tab-terrain","tab-life","tab-civilization","tab-construction","tab-economy"};
    const std::array<const char*,5> future_panels={"terrain-tools","life-tools","civilization-tools","construction-tools","economy-tools"};
    const std::array<const char*,5> future_buttons={"terrain-pending","life-pending","civilization-pending","construction-pending","economy-pending"};
    for(std::size_t i=0;i<future_tabs.size();++i) {
        click(ui,future_tabs[i],density);require(single(ui,ActionKind::SelectBottomSection).a==static_cast<int>(i)+1,"Every category is navigable and emits one actual section selection");
        rock_join_contract(ui,render,density);
        for(std::size_t j=0;j<future_panels.size();++j) require((ui.context().GetDocument(0)->GetElementById(future_panels[j])->GetComputedValues().display()!=Rml::Style::Display::None)==(j==i),"Only the selected section contents may occupy the toolbar body");
        ink_contract(ui.context().GetDocument(0)->GetElementById(future_panels[i])->GetChild(1),222,219,212);
        click(ui,future_buttons[i],density);require(ui.take_actions().empty(),"Unavailable future tools stay visibly disabled without claiming a World edit");
    }
    click(ui,"tab-observe",density);single(ui,ActionKind::SelectBottomSection);
    render.reset_draws();ui.render();require(render.materials_drawn==std::set<std::string>{"rock"},"Real world HUD and projecting dock must draw the same dark stone without old coloured materials");
    stone_surface_contract(ui,render,{"world-status","bottom-bar-frame","world-info","camera-home","camera-help"});
    auto* world_info=ui.context().GetDocument(0)->GetElementById("world-info");
    ui.context().ProcessMouseMove(0,0,0);ui.context().GetDocument(0)->GetElementById("camera-home")->Focus();ui.update();render.reset_draws();ui.render();
    const int plain_world_tint=stone_element_tint(ui,render,"world-info",density);
    world_info->Focus();ui.update();render.reset_draws();ui.render();
    require(world_info->IsPseudoClassSet("focus") && stone_element_tint(ui,render,"world-info",density)==232 && 232>plain_world_tint+40,"Actual keyboard focus must visibly brighten the specific world-info stone plaque rather than pass because another selected surface is stone");
    require(ui.take_actions().empty(),"Focusing an instrument must not emit a World edit");
    view.presentation_darkness=true;ui.set_view(view);ui.update();
    ui.context().ProcessMouseMove(0,0,0);
    ui.context().GetDocument(0)->GetElementById("tab-observe")->Focus();
    ui.update();render.reset_draws();ui.render();
    require(render.materials_drawn==std::set<std::string>{"rock"} && stone_element_tint(ui,render,"tab-observe",density)==232,"World Age must preserve shared stone surfaces and the actual brighter keyboard focused tab");
    ui.context().GetDocument(0)->GetElementById("camera-home")->Focus();
    ui.update();render.reset_draws();ui.render();
    require(render.materials_drawn==std::set<std::string>{"rock"} && stone_element_tint(ui,render,"tab-observe",density)==238*232/255,"The selected stone tab must keep its own brightness when keyboard focus moves to another control");
    require(render.full_colour_sprite,"Native colour sprites must be submitted without monochrome tint");
    require(ui.context().GetDocument(0)->GetElementById("camera-home")->GetChild(0)->GetAttribute<Rml::String>("src","")=="../generated/ui/icons/camera-96.png","World Age must not replace the individually coloured icon artwork with a global theme");
    require(ui.take_actions().empty(),"Age presentation synchronization must not mutate World or issue commands");
    click(ui,"tab-world",density);single(ui,ActionKind::SelectBottomSection);
    for(const auto locale:{Locale::Chinese,Locale::English,Locale::German}) {
        view.locale=locale;ui.set_view(view);ui.update();layout_contract(ui);
        const auto world_tools=rect(ui,"section-tools",density),save=rect(ui,"save",density),load=rect(ui,"world-load",density),menu=rect(ui,"return-menu",density);
        require(world_tools.h<=(width>=792?72.2f:136.2f),"Physical world instruments must share one compact real row when they fit, rather than reserve an empty double-height dock");
        for(const auto* id:{"save","world-load","return-menu","age-light","age-darkness","age-auto","age-manual"}) object_icon_contract(ui,id,density);
        require(world_tools.w<=width+.15f && std::abs(save.w-60)<.15f && std::abs(load.w-60)<.15f && std::abs(menu.w-60)<.15f,"World tools must use60dp individual physical slots in the lower tool dock");
        require(std::abs(save.y-load.y)<.15f && std::abs(save.y-menu.y)<.15f,"Save, load and menu controls must share a native flex row in all three locales");
        const auto clock_group=rect(ui,"world-clock-group",density),save_group=rect(ui,"world-save-group",density),category=rect(ui,"bottom-bar",density);
        require(clock_group.h>=48 && save_group.h>=48 && clock_group.y>=category.y+category.h-.15f,"Tool groups must have real nonzero layout beneath the category row");
        const std::array<const char*,5> clock_buttons={"pause","speed-one","speed-four","speed-sixteen","speed-sixtyfour"};
        float previous_right=clock_group.x;
        for(const auto* id:clock_buttons) {
            const auto slot=rect(ui,id,density);
            require(slot.h>=59.85f && slot.w>=59.85f && std::abs(slot.y-clock_group.y)<.15f,"Every time control must have real60dp geometry in its clock row");
            require(slot.x>=previous_right-.15f && slot.x+slot.w<=clock_group.x+clock_group.w+.15f && slot.y+slot.h<=height+.15f,"Clock controls must occupy separate visible slots within their block wrapper");
            previous_right=slot.x+slot.w;
        }
        previous_right=save_group.x;
        for(const auto* id:{"save","world-load","return-menu"}) {
            const auto slot=rect(ui,id,density);
            require(slot.h>=59.85f && slot.x>=previous_right-.15f && slot.x+slot.w<=save_group.x+save_group.w+.15f && slot.y>=save_group.y-.15f && slot.y+slot.h<=save_group.y+save_group.h+.15f,"Every save/menu action must have nonzero visible geometry inside its own block wrapper");
            previous_right=slot.x+slot.w;
        }
        const auto light=rect(ui,"age-light",density),dark=rect(ui,"age-darkness",density);
        require(std::abs(light.w-60)<.15f && std::abs(dark.w-60)<.15f && std::abs(light.y-dark.y)<.15f,"Age tools must occupy two neighbouring60dp physical slots");
        require(ui.take_actions().empty(),"Responsive tool layout and locale changes must not produce intents");
    }
    view.locale=Locale::English;ui.set_view(view);ui.update();
    const auto save_target=rect(ui,"save",density);
    ui.context().ProcessMouseMove(static_cast<int>((save_target.x+save_target.w/2)*density),static_cast<int>((save_target.y+save_target.h/2)*density),0);ui.update();
    const auto short_tip=rect(ui,"tool-tooltip",density);
    auto* short_tooltip=ui.context().GetDocument(0)->GetElementById("tool-tooltip");
    const float save_word_width=actual_glyph_width(short_tooltip,"Save",density);
    require(short_tooltip->IsVisible(true) && short_tip.w<128.f && std::abs(short_tip.w-std::max(56.f,std::ceil(save_word_width+24.f)))<.6f,"Short Save tooltip must actually fit its stone label rather than reserve a272dp empty plaque");
    require(short_tip.x>=0 && short_tip.y>=0 && short_tip.x+short_tip.w<=width+.15f && short_tip.y+short_tip.h<=height+.15f,"Sized short stone tooltip must preserve its actual bounded native anchor");
    ui.context().ProcessMouseMove(0,0,0);ui.update();
    click(ui,"speed-sixtyfour",density);require(single(ui,ActionKind::SetSpeed).a==64,"Only approved clock multipliers are exposed");
    click(ui,"age-darkness",density);require(single(ui,ActionKind::SetAgeDarkness).generation==44,"Age intent belongs to current session");
    click(ui,"world-info",density);single(ui,ActionKind::OpenWorldInspector);layout_contract(ui);
    stone_heading_contract(ui.context().GetDocument(0)->GetElementById("world-panel")->GetChild(0)->GetChild(0),density);
    render.reset_draws();ui.render();stone_surface_contract(ui,render,{"world-panel","world-close"});
    ink_contract(ui.context().GetDocument(0)->GetElementById("world-facts"),222,219,212);
    ink_contract(ui.context().GetDocument(0)->GetElementById("rename-consequences"),222,219,212);
    ink_contract(ui.context().GetDocument(0)->GetElementById("world-name-draft"),222,219,212);
    require(ui.blocks_world_input(),"World inspector must block camera input independently of pointer hover");
    hit_layer_contract(ui,"tab-world","world-overlay",density);
    auto* name=dynamic_cast<Rml::ElementFormControl*>(ui.context().GetDocument(0)->GetElementById("world-name-draft"));require(name!=nullptr,"World name must use a real editable native control");
    std::string full_name;for(int i=0;i<128;++i) full_name+="汉";
    name->SetValue("");name->Focus();ui.context().ProcessTextInput(full_name+"汉");ui.update();
    require(name->GetValue()==full_name,"Native Name maxlength must allow 128 Unicode code points and clamp the 129th, not count UTF-8 bytes");
    ui.take_actions();
    name->SetValue("");name->Focus();ui.context().ProcessTextInput("帝国");ui.update();
    auto typed=ui.take_actions();require(!typed.empty() && typed.back().kind==ActionKind::SetWorldNameDraft && typed.back().text=="帝国","Native UTF-8 input must emit draft text, not change World directly");
    view.locale=Locale::German;view.world.revision=3;ui.set_view(view);ui.update();require(name->GetValue()=="帝国","Locale/late world query must not discard current name draft");
    ui.activate("rename-preview");require(single(ui,ActionKind::PreviewWorldName).text=="帝国","Name preview uses retained draft");
    ui.activate("rename-commit");require(ui.take_actions().empty(),"No commit without approved consequences");
    view.world.rename_preview_ready=true;view.world.rename_preview_token=99;view.world.rename_consequences="Name only";ui.set_view(view);ui.update();ui.activate("rename-commit");require(single(ui,ActionKind::CommitWorldName).token==99,"Name commit must carry displayed consequence token");
    click(ui,"world-close",density);single(ui,ActionKind::CloseWorldInspector);
    require(!ui.blocks_world_input(),"Closing the world inspector must restore camera input");
    click(ui,"tab-observe",density);single(ui,ActionKind::SelectBottomSection);
    click(ui,"camera-help",density);single(ui,ActionKind::OpenHelp);layout_contract(ui);require(ui.blocks_world_input(),"Camera help is a modal input barrier");
    hit_layer_contract(ui,"tab-observe","help-overlay",density);
    render.reset_draws();ui.render();stone_surface_contract(ui,render,{"help-panel","help-dismiss","help-close"});
    click(ui,"help-close",density);single(ui,ActionKind::CloseHelp);require(!ui.blocks_world_input(),"Closing help releases camera input");
    click(ui,"tab-settings",density);require(single(ui,ActionKind::SelectBottomSection).a==7,"The eighth raised category must navigate to settings tools just like the other seven");
    require(!ui.blocks_world_input(),"Selecting settings tools alone must preserve the world view without prematurely opening a modal");
    click(ui,"settings-open",density);single(ui,ActionKind::OpenSettings);ui.update();layout_contract(ui);
    require(ui.blocks_world_input(),"Settings must block camera input even without text focus");
    hit_layer_contract(ui,"tab-settings","settings-overlay",density);
    require(ui.context().GetDocument(0)->GetElementById("tab-settings")->IsClassSet("selected"),"Settings must select its raised category before opening its local modal");
    for(const auto locale:{Locale::Chinese,Locale::English,Locale::German}) {
        view.locale=locale;ui.set_view(view);ui.update();layout_contract(ui);
        auto* settings_heading=ui.context().GetDocument(0)->GetElementById("settings-panel")->GetChild(0)->GetChild(0);stone_heading_contract(settings_heading,density);
        const auto panel=rect(ui,"settings-panel",density);require(panel.x>=0 && panel.x+panel.w<=width+.1 && panel.y>=0 && panel.y+panel.h<=height+.1,"Native settings panel clips outside viewport");
        const auto volume=rect(ui,"audio-volume",density);require(volume.y>panel.y+100,"Settings business rows must be block layout, not one inline run");
        auto* close=ui.context().GetDocument(0)->GetElementById("settings-close");close->ScrollIntoView(false);ui.update();
        const auto close_rect=rect(ui,"settings-close",density);
        require(close_rect.y>=panel.y-.15 && close_rect.y+close_rect.h<=panel.y+panel.h+.15,"The last control of long localized settings must be reachable through real native scrolling");
        auto* settings=ui.context().GetDocument(0)->GetElementById("settings-panel");
        if(settings->GetScrollHeight()>settings->GetClientHeight()+.5f) require(settings->GetScrollTop()>0,"Overflowing localized settings must genuinely scroll to their final action");
        render.reset_draws();ui.render();stone_surface_contract(ui,render,{"settings-panel","settings-close"});
        view.error_key="CREATION_DIMENSIONS";view.error_detail="core width 32..4096";view.busy=true;ui.set_view(view);ui.update();
        ink_contract(ui.context().GetDocument(0)->GetElementById("busy-label"),222,219,212);
        error_layer_contract(ui,density);
        hit_layer_contract(ui,"tab-settings","error-overlay",density);
        hit_layer_contract(ui,"busy-indicator","error-overlay",density);
        render.reset_draws();ui.render();stone_surface_contract(ui,render,{"error-panel","error-dismiss","busy-indicator"});
        click(ui,"error-dismiss",density);single(ui,ActionKind::DismissError);
        auto* acknowledged=ui.context().GetFocusElement();
        require(acknowledged && acknowledged->GetId()=="error-dismiss","Core error remains the keyboard barrier until its authoritative acknowledgement completes");
        view.error_key.clear();view.error_detail.clear();view.busy=false;ui.set_view(view);ui.update();
        auto* focused=ui.context().GetFocusElement();auto* restored=focused;
        while(restored && restored!=settings) restored=restored->GetParentNode();
        require(restored==settings && ui.blocks_world_input(),"Clearing a nested error must restore focus and the barrier to the underlying settings modal");
        hit_layer_contract(ui,focused->GetId().c_str(),focused->GetId().c_str(),density);
    }
    ui.context().ProcessKeyDown(Rml::Input::KI_ESCAPE,0);single(ui,ActionKind::CloseSettings);
    require(!ui.blocks_world_input(),"Escape from settings releases the modal input barrier");
    view.screen=Screen::Load;view.saves={{"save1","Original world",""}};view.selected_save="save1";ui.set_view(view);ui.update();layout_contract(ui);click(ui,"load-selected",density);require(single(ui,ActionKind::LoadSelected).text=="save1","Load must reference selected real save");
    const auto save=rect(ui,"save-entry-0",density),save_content=rect(ui,"save-panel",density,Rml::Box::CONTENT);
    require(save.x>=save_content.x-.15 && save.x+save.w<=save_content.x+save_content.w+.15,"Save entry percentage width must include its horizontal padding");
    render.reset_draws();ui.render();stone_surface_contract(ui,render,{"save-panel","save-entry-0","load-selected","back-load"});
    click(ui,"back-load",density);single(ui,ActionKind::CancelLoad);
    view.error_key="CREATION_DIMENSIONS";view.error_detail="core width 32..4096";
    for(const auto locale:{Locale::Chinese,Locale::English,Locale::German}) {
        view.locale=locale;ui.set_view(view);ui.update();layout_contract(ui);
        require(ui.blocks_world_input(),"Business errors must block world camera input");
        error_layer_contract(ui,density);
        auto* document=ui.context().GetDocument(0);
        stone_heading_contract(document->GetElementById("error-panel")->GetChild(0),density);
        render.reset_draws();ui.render();stone_surface_contract(ui,render,{"error-panel","error-dismiss"});
        require(document->GetElementById("error-message")->GetInnerRML().find("32–4096")!=std::string::npos,"Creation dimensions must have a localized business reason in all three languages");
        require(document->GetElementById("error-detail")->GetComputedValues().display()==Rml::Style::Display::None,"Raw technical details must remain hidden by default");
    }
    click(ui,"error-details",density);ui.update();
    require(ui.context().GetDocument(0)->GetElementById("error-detail")->GetComputedValues().display()!=Rml::Style::Display::None && ui.take_actions().empty(),"Technical details expand locally on demand without a World command");
    view.error_detail="derived height 32..4096";ui.set_view(view);ui.update();
    require(ui.context().GetDocument(0)->GetElementById("error-detail")->GetComputedValues().display()==Rml::Style::Display::None,"A new failure must not inherit the old expanded technical details state");
    std::cout<<"Native UI "<<width<<'x'<<height<<" density="<<density<<": real layout, pointer, UTF-8 draft and typed actions passed\n";
}
}
int main(int argc,char** argv) {
    if(argc!=2) {std::cerr<<"Pass runtime resource directory\n";return 1;}
    System system;Render render;Rml::SetSystemInterface(&system);Rml::SetRenderInterface(&render);if(!Rml::Initialise())return 1;int result=0;
    std::vector<Rml::byte> font_bytes;
    try { const std::filesystem::path root=argv[1];render.resource_root=root;
        std::ifstream font(root/"fonts/fusion-pixel-12px-proportional-zh_hans.otf",std::ios::binary);
        require(font.good(),"Admitted arcade font file missing");
        font_bytes.assign(std::istreambuf_iterator<char>(font),std::istreambuf_iterator<char>());
        require(!font_bytes.empty() && Rml::LoadFontFace(font_bytes.data(),static_cast<int>(font_bytes.size()),"Sonn Arcade",Rml::Style::FontStyle::Normal,Rml::Style::FontWeight::Normal,true),"Arcade font alias registration failed");
        pixel_geometry_contract();
        material_geometry_contract();
        for(float density:{1.f,2.f}) {
            for(const auto dimensions:{std::array<int,2>{1280,800},std::array<int,2>{480,640},std::array<int,2>{640,480},std::array<int,2>{480,480}}) {
                scenario(root,dimensions[0],dimensions[1],density,render);
                if(!system.layout_rejections.empty()) throw std::runtime_error("Native RmlUi layout rejection: "+system.layout_rejections.front());
            }
        }
    } catch(const std::exception& failure) {std::cerr<<failure.what()<<'\n';result=1;}
    // The memory font alias explicitly borrows these bytes until Shutdown.
    Rml::Shutdown();return result;
}
