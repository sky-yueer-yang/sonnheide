#include "pixel_decorator.hpp"
#include <RmlUi/Core/Decorator.h>
#include <RmlUi/Core/DecoratorInstancer.h>
#include <RmlUi/Core/ComputedValues.h>
#include <RmlUi/Core/Element.h>
#include <RmlUi/Core/ElementUtilities.h>
#include <RmlUi/Core/Factory.h>
#include <RmlUi/Core/Geometry.h>
#include <RmlUi/Core/FontEffect.h>
#include <RmlUi/Core/FontEffectInstancer.h>
#include <RmlUi/Core/PropertyDefinition.h>
#include <algorithm>
#include <cmath>
#include <utility>

namespace sonnheide::ui {
namespace {
void quad(PixelMesh& mesh,float x,float y,float w,float h,Rml::Colourb colour) {
    if(w<=0 || h<=0 || colour.alpha==0) return;
    const int n=static_cast<int>(mesh.vertices.size());
    mesh.vertices.push_back({{x,y},colour,{0,0}});
    mesh.vertices.push_back({{x+w,y},colour,{0,0}});
    mesh.vertices.push_back({{x+w,y+h},colour,{0,0}});
    mesh.vertices.push_back({{x,y+h},colour,{0,0}});
    for(int index:{0,1,2,0,2,3}) mesh.indices.push_back(n+index);
}
void shape(PixelMesh& mesh,float x,float y,float w,float h,float s,Rml::Colourb colour) {
    if(s<=0) { quad(mesh,x,y,w,h,colour); return; }
    // Five non-overlapping bands form two whole-pixel steps at every corner.
    quad(mesh,x+2*s,y,w-4*s,s,colour);
    quad(mesh,x+s,y+s,w-2*s,s,colour);
    quad(mesh,x,y+2*s,w,h-4*s,colour);
    quad(mesh,x+s,y+h-2*s,w-2*s,s,colour);
    quad(mesh,x+2*s,y+h-s,w-4*s,s,colour);
}
class PixelDecorator final:public Rml::Decorator {
public:
    PixelDecorator(Rml::Colourb fill,Rml::Property step,Rml::Property shadow,Rml::Colourb shade,int arrow):fill_(fill),shade_(shade),step_(std::move(step)),shadow_(std::move(shadow)),arrow_(arrow) {}
    Rml::DecoratorDataHandle GenerateElementData(Rml::Element* element) const override {
        const auto size=element->GetBox().GetSize(Rml::Box::PADDING);
        auto fill=fill_,shade=shade_;
        const auto opacity=element->GetComputedValues().opacity();
        fill.alpha=static_cast<Rml::byte>(fill.alpha*opacity);shade.alpha=static_cast<Rml::byte>(shade.alpha*opacity);
        PixelMesh mesh;
        if(arrow_) {
            const float cell_x=std::max(1.f,std::floor(size.x/8)),cell_y=std::max(1.f,std::floor(size.y/4));
            const float x=std::round((size.x-8*cell_x)/2),y=std::round((size.y-4*cell_y)/2);
            for(int row=0;row<4;++row) quad(mesh,x+row*cell_x,y+(arrow_>0?row:3-row)*cell_y,(8-2*row)*cell_x,cell_y,fill);
        } else mesh=pixel_mesh(std::round(size.x),std::round(size.y),element->ResolveNumericProperty(&step_,0),element->ResolveNumericProperty(&shadow_,0),fill,shade);
        auto* data=new Rml::Geometry(element);
        data->GetVertices()=std::move(mesh.vertices);data->GetIndices()=std::move(mesh.indices);
        return reinterpret_cast<Rml::DecoratorDataHandle>(data);
    }
    void ReleaseElementData(Rml::DecoratorDataHandle data) const override { delete reinterpret_cast<Rml::Geometry*>(data); }
    void RenderElement(Rml::Element* element,Rml::DecoratorDataHandle data) const override {
        if(auto* geometry=reinterpret_cast<Rml::Geometry*>(data)) geometry->Render(element->GetAbsoluteOffset(Rml::Box::PADDING).Round());
    }
private:
    Rml::Colourb fill_,shade_;
    Rml::Property step_,shadow_;
    int arrow_;
};
class PixelInstancer final:public Rml::DecoratorInstancer {
public:
    PixelInstancer() {
        fill_=RegisterProperty("fill","#0c182af8").AddParser("color").GetId();
        step_=RegisterProperty("step","4dp").AddParser("length").GetId();
        shadow_=RegisterProperty("shadow","4dp").AddParser("length").GetId();
        shade_=RegisterProperty("shade","#00000080").AddParser("color").GetId();
        RegisterShorthand("decorator","fill, step, shadow, shade",Rml::ShorthandType::FallThrough);
    }
    Rml::SharedPtr<Rml::Decorator> InstanceDecorator(const Rml::String& name,const Rml::PropertyDictionary& properties,const Rml::DecoratorInstancerInterface&) override {
        return Rml::MakeShared<PixelDecorator>(properties.GetProperty(fill_)->Get<Rml::Colourb>(),*properties.GetProperty(step_),*properties.GetProperty(shadow_),properties.GetProperty(shade_)->Get<Rml::Colourb>(),name=="pixel-down"?1:name=="pixel-up"?-1:0);
    }
private:
    Rml::PropertyId fill_,step_,shadow_,shade_;
};
class RimDecorator final:public Rml::Decorator {
public:
    RimDecorator(Rml::Colourb fill,Rml::Colourb outline,Rml::Colourb shade,Rml::Property edge,Rml::Property corner,Rml::Property depth,Rml::Property gap_start,Rml::Property gap_width,bool tab):fill_(fill),outline_(outline),shade_(shade),edge_(std::move(edge)),corner_(std::move(corner)),depth_(std::move(depth)),gap_start_(std::move(gap_start)),gap_width_(std::move(gap_width)),tab_(tab) {}
    Rml::DecoratorDataHandle GenerateElementData(Rml::Element* element) const override {
        const auto size=element->GetBox().GetSize(Rml::Box::PADDING);
        auto fill=fill_,outline=outline_,shade=shade_;
        for(auto* color:{&fill,&outline,&shade}) color->alpha=static_cast<Rml::byte>(color->alpha*element->GetComputedValues().opacity());
        const auto value=[&](const Rml::Property& property){return element->ResolveNumericProperty(&property,0);};
        auto mesh=tab_?pixel_tab_mesh(std::round(size.x),std::round(size.y),value(edge_),value(corner_),value(depth_),fill,outline,shade):pixel_rim_mesh(std::round(size.x),std::round(size.y),value(edge_),value(corner_),value(depth_),fill,outline,shade,value(gap_start_),value(gap_width_));
        auto* geometry=new Rml::Geometry(element);geometry->GetVertices()=std::move(mesh.vertices);geometry->GetIndices()=std::move(mesh.indices);
        return reinterpret_cast<Rml::DecoratorDataHandle>(geometry);
    }
    void ReleaseElementData(Rml::DecoratorDataHandle data) const override { delete reinterpret_cast<Rml::Geometry*>(data); }
    void RenderElement(Rml::Element* element,Rml::DecoratorDataHandle data) const override { if(auto* geometry=reinterpret_cast<Rml::Geometry*>(data)) geometry->Render(element->GetAbsoluteOffset(Rml::Box::PADDING).Round()); }
private:
    Rml::Colourb fill_,outline_,shade_;Rml::Property edge_,corner_,depth_,gap_start_,gap_width_;bool tab_;
};
class RimInstancer final:public Rml::DecoratorInstancer {
public:
    RimInstancer() {
        fill_=RegisterProperty("fill","#042537cc").AddParser("color").GetId();
        outline_=RegisterProperty("outline","#010c20ff").AddParser("color").GetId();
        shade_=RegisterProperty("shade","#01091cff").AddParser("color").GetId();
        edge_=RegisterProperty("edge","4dp").AddParser("length").GetId();
        corner_=RegisterProperty("corner","3dp").AddParser("length").GetId();
        depth_=RegisterProperty("depth","2dp").AddParser("length").GetId();
        gap_start_=RegisterProperty("gap-start","0dp").AddParser("length").GetId();
        gap_width_=RegisterProperty("gap-width","0dp").AddParser("length").GetId();
        RegisterShorthand("decorator","fill, outline, shade, edge, corner, depth, gap-start, gap-width",Rml::ShorthandType::FallThrough);
    }
    Rml::SharedPtr<Rml::Decorator> InstanceDecorator(const Rml::String& name,const Rml::PropertyDictionary& properties,const Rml::DecoratorInstancerInterface&) override {
        return Rml::MakeShared<RimDecorator>(properties.GetProperty(fill_)->Get<Rml::Colourb>(),properties.GetProperty(outline_)->Get<Rml::Colourb>(),properties.GetProperty(shade_)->Get<Rml::Colourb>(),*properties.GetProperty(edge_),*properties.GetProperty(corner_),*properties.GetProperty(depth_),*properties.GetProperty(gap_start_),*properties.GetProperty(gap_width_),name=="pixel-tab");
    }
private:Rml::PropertyId fill_,outline_,shade_,edge_,corner_,depth_,gap_start_,gap_width_;
};

struct MaterialGeometry {
    explicit MaterialGeometry(Rml::Element* element):face(element),neck(element) {}
    Rml::Geometry face,neck;
};
class MaterialDecorator final:public Rml::Decorator {
public:
    MaterialDecorator(int material,Rml::Colourb tint,Rml::Property edge,Rml::Property corner,Rml::Property depth,Rml::Property gap_start,Rml::Property gap_width,bool tab):tint_(tint),edge_(std::move(edge)),corner_(std::move(corner)),depth_(std::move(depth)),gap_start_(std::move(gap_start)),gap_width_(std::move(gap_width)),tab_(tab) {
        const auto& definitions=physical_material_definitions();
        // These sources are deliberately relative to the admitted runtime
        // resource root. Inline dock sizing has no RCSS PropertySource path.
        Rml::Texture texture;texture.Set(definitions[material].texture_source);
        face_texture_=AddTexture(texture);
        texture.Set(definitions[0].texture_source);neck_texture_=AddTexture(texture);
    }
    Rml::DecoratorDataHandle GenerateElementData(Rml::Element* element) const override {
        const auto size=element->GetBox().GetSize(Rml::Box::PADDING);
        const auto value=[&](const Rml::Property& property){return element->ResolveNumericProperty(&property,0);};
        const float scale=Rml::ElementUtilities::GetDensityIndependentPixelRatio(element);
        auto tint=tint_;tint.alpha=static_cast<Rml::byte>(tint.alpha*element->GetComputedValues().opacity());
        const float depth=value(depth_);
        auto mesh=pixel_material_mesh(std::round(size.x),std::round(size.y),scale,value(edge_),value(corner_),depth,tint,tab_,value(gap_start_),value(gap_width_));
        auto* geometry=new MaterialGeometry(element);
        geometry->face.SetTexture(GetTexture(face_texture_));geometry->neck.SetTexture(GetTexture(neck_texture_));
        const float neck_top=std::round(size.y)-std::clamp(std::round(depth),0.f,std::max(0.f,std::round(size.y)-1));
        // Tabs are a carved face attached to a continuous marble foot. The
        // foot uses the dock's material even when the selected face is brass.
        for(std::size_t i=0;i+3<mesh.vertices.size();i+=4) {
            auto& part=tab_ && mesh.vertices[i].position.y>=neck_top?geometry->neck:geometry->face;
            const int offset=static_cast<int>(part.GetVertices().size());
            for(std::size_t v=0;v<4;++v)part.GetVertices().push_back(mesh.vertices[i+v]);
            for(int index:{0,1,2,0,2,3})part.GetIndices().push_back(offset+index);
        }
        return reinterpret_cast<Rml::DecoratorDataHandle>(geometry);
    }
    void ReleaseElementData(Rml::DecoratorDataHandle data) const override {delete reinterpret_cast<MaterialGeometry*>(data);}
    void RenderElement(Rml::Element* element,Rml::DecoratorDataHandle data) const override {
        if(auto* geometry=reinterpret_cast<MaterialGeometry*>(data)) {
            const auto offset=element->GetAbsoluteOffset(Rml::Box::PADDING).Round();
            if(geometry->face)geometry->face.Render(offset);
            if(geometry->neck)geometry->neck.Render(offset);
        }
    }
private:
    Rml::Colourb tint_;Rml::Property edge_,corner_,depth_,gap_start_,gap_width_;
    bool tab_;int face_texture_=-1,neck_texture_=-1;
};
class MaterialInstancer final:public Rml::DecoratorInstancer {
public:
    explicit MaterialInstancer(bool tab):tab_(tab) {
        material_=RegisterProperty("material","marble").AddParser("keyword","marble, brass, crimson, obsidian, wood, parchment, leather, iron, enamel").GetId();
        tint_=RegisterProperty("tint","#ffffffff").AddParser("color").GetId();
        edge_=RegisterProperty("edge","3dp").AddParser("length").GetId();
        corner_=RegisterProperty("corner","2dp").AddParser("length").GetId();
        depth_=RegisterProperty("depth",tab?"6dp":"3dp").AddParser("length").GetId();
        gap_start_=RegisterProperty("gap-start","0dp").AddParser("length").GetId();
        gap_width_=RegisterProperty("gap-width","0dp").AddParser("length").GetId();
        RegisterShorthand("decorator","material, tint, edge, corner, depth, gap-start, gap-width",Rml::ShorthandType::FallThrough);
    }
    Rml::SharedPtr<Rml::Decorator> InstanceDecorator(const Rml::String&,const Rml::PropertyDictionary& properties,const Rml::DecoratorInstancerInterface&) override {
        const int material=properties.GetProperty(material_)->Get<int>();
        if(material<0 || material>=static_cast<int>(physical_material_definitions().size()))return nullptr;
        return Rml::MakeShared<MaterialDecorator>(material,properties.GetProperty(tint_)->Get<Rml::Colourb>(),*properties.GetProperty(edge_),*properties.GetProperty(corner_),*properties.GetProperty(depth_),*properties.GetProperty(gap_start_),*properties.GetProperty(gap_width_),tab_);
    }
private:
    bool tab_;Rml::PropertyId material_,tint_,edge_,corner_,depth_,gap_start_,gap_width_;
};

class PixelOutline final:public Rml::FontEffect {
public:
    explicit PixelOutline(int radius,bool pigment=false):radius_(radius),pigment_(pigment) { SetLayer(Layer::Back); }
    bool HasUniqueTexture() const override { return true; }
    bool GetGlyphMetrics(Rml::Vector2i& origin,Rml::Vector2i& dimensions,const Rml::FontGlyph&) const override {
        if(dimensions.x<=0 || dimensions.y<=0) return false;
        if(pigment_) dimensions.x+=radius_;
        else { origin.x-=radius_;origin.y-=radius_;dimensions.x+=2*radius_;dimensions.y+=2*radius_; }
        return true;
    }
    void GenerateGlyphTexture(Rml::byte* destination,Rml::Vector2i size,int stride,const Rml::FontGlyph& glyph) const override {
        if(pigment_) pixel_glyph_bolden(destination,size,stride,glyph,radius_);
        else pixel_glyph_dilate(destination,size,stride,glyph,radius_);
    }
private: int radius_;bool pigment_;
};
class PixelOutlineInstancer final:public Rml::FontEffectInstancer {
public:
    PixelOutlineInstancer() {
        radius_=RegisterProperty("width","1dp",true).AddParser("length").GetId();
        colour_=RegisterProperty("color","#ffffff",false).AddParser("color").GetId();
        RegisterShorthand("font-effect","width, color",Rml::ShorthandType::FallThrough);
    }
    Rml::SharedPtr<Rml::FontEffect> InstanceFontEffect(const Rml::String& name,const Rml::PropertyDictionary& properties) override {
        const float width=properties.GetProperty(radius_)->Get<float>();
        if(!std::isfinite(width) || width<=0 || width>16) return nullptr;
        auto effect=Rml::MakeShared<PixelOutline>(std::max(1,static_cast<int>(std::round(width))),name=="pixel-bold");
        effect->SetColour(properties.GetProperty(colour_)->Get<Rml::Colourb>());
        return effect;
    }
private: Rml::PropertyId radius_,colour_;
};

}
PixelMesh pixel_mesh(float width,float height,float step,float shadow,Rml::Colourb fill,Rml::Colourb shade) {
    PixelMesh mesh;
    if(!std::isfinite(width) || !std::isfinite(height) || width<=0 || height<=0) return mesh;
    const float limit=std::floor(std::min(width,height)/5);
    const float s=std::isfinite(step)?std::clamp(std::round(step),0.f,limit):0.f;
    const float offset=std::isfinite(shadow)?std::max(0.f,std::round(shadow)):0.f;
    if(offset>0) shape(mesh,offset,2*offset,width,height,s,shade);
    shape(mesh,0,0,width,height,s,fill);
    return mesh;
}
PixelMesh pixel_rim_mesh(float width,float height,float edge,float corner,float depth,Rml::Colourb fill,Rml::Colourb outline,Rml::Colourb shade,float gap_start,float gap_width) {
    PixelMesh mesh;
    if(!std::isfinite(width)||!std::isfinite(height)||width<=0||height<=0) return mesh;
    const float e=std::clamp(std::round(edge),1.f,std::max(1.f,std::floor(std::min(width,height)/6)));
    const float c=std::clamp(std::round(corner),0.f,std::floor(std::min(width,height)/8));
    const float d=std::clamp(std::round(depth),0.f,std::max(0.f,std::floor(std::min(width,height)/6)));
    const auto inset=[&](float y){return y<c || y>=height-c?2*c:y<2*c || y>=height-2*c?c:0.f;};
    Rml::Vector<float> rows={0,c,2*c,height-2*c,height-c,height,e,height-e,height-e-d,c+e,2*c+e,height-c-e,height-2*c-e};
    for(auto& y:rows)y=std::clamp(y,0.f,height);
    std::sort(rows.begin(),rows.end());rows.erase(std::unique(rows.begin(),rows.end()),rows.end());
    const float gap_left=std::clamp(std::round(gap_start),0.f,width),gap_right=std::clamp(std::round(gap_start+std::max(0.f,gap_width)),0.f,width);
    for(std::size_t i=1;i<rows.size();++i) {
        const float top=rows[i-1],h=rows[i]-top,mid=top+h/2,outer=inset(mid),right=width-outer;
        if(mid<e || mid>=height-e) {
            // A tab neck supplies this top-ring gap. No panel paint beneath it,
            // so the neck's translucent join face is blended exactly once.
            if(mid<e && gap_right>gap_left) {quad(mesh,outer,top,std::max(0.f,std::min(right,gap_left)-outer),h,outline);quad(mesh,std::max(outer,gap_right),top,std::max(0.f,right-std::max(outer,gap_right)),h,outline);}
            else quad(mesh,outer,top,right-outer,h,outline);
            continue;
        }
        const float inner=std::max(outer,e+std::max(inset(mid-e),inset(mid+e))),inner_right=std::min(right,width-inner);
        quad(mesh,outer,top,inner-outer,h,outline);quad(mesh,inner_right,top,right-inner_right,h,outline);
        if(mid>=height-e-d)quad(mesh,inner,top,inner_right-inner,h,shade);
        else {const float shade_left=std::max(inner,inner_right-d);quad(mesh,inner,top,shade_left-inner,h,fill);quad(mesh,shade_left,top,inner_right-shade_left,h,shade);}
    }
    return mesh;
}
PixelMesh pixel_tab_mesh(float width,float height,float edge,float shoulder,float neck,Rml::Colourb fill,Rml::Colourb outline,Rml::Colourb join) {
    PixelMesh mesh;
    if(!std::isfinite(width)||!std::isfinite(height)||width<=0||height<=0) return mesh;
    const float e=std::clamp(std::round(edge),1.f,std::max(1.f,std::floor(std::min(width,height)/6)));
    const float c=std::clamp(std::round(shoulder),1.f,std::max(1.f,std::floor(std::min(width,height)/8)));
    const float n=std::clamp(std::round(neck),0.f,std::max(0.f,height-e));
    const float shoulder_height=height-n;
    const float s1=std::round(shoulder_height*.3f),s2=std::round(shoulder_height*.6f),s3=std::round(shoulder_height*.9f);
    // The outline widens throughout almost the complete face, rather than
    // hiding a square button behind a pair of small cuts at its upper corners.
    const auto inset=[&](float y){return y<s1?3*c:y<s2?2*c:y<s3?c:0.f;};
    Rml::Vector<float> rows={0,s1,s2,s3,height,e,s1+e,s2+e,s3+e,height-n};
    for(auto& y:rows)y=std::clamp(y,0.f,height);
    std::sort(rows.begin(),rows.end());rows.erase(std::unique(rows.begin(),rows.end()),rows.end());
    for(std::size_t i=1;i<rows.size();++i) {
        const float top=rows[i-1],h=rows[i]-top,mid=top+h/2,outer=inset(mid),right=width-outer;
        if(mid<e){quad(mesh,outer,top,right-outer,h,outline);continue;}
        const float inner=std::max(outer,e+inset(mid-e)),inner_right=std::min(right,width-inner);
        quad(mesh,outer,top,inner-outer,h,outline);quad(mesh,inner_right,top,right-inner_right,h,outline);
        quad(mesh,inner,top,inner_right-inner,h,mid>=height-n?join:fill);
    }
    return mesh;
}
const std::array<PhysicalMaterialDefinition,9>& physical_material_definitions() {
    static const std::array<PhysicalMaterialDefinition,9> definitions={{{"marble","generated/ui/materials/marble.png",512,512},{"brass","generated/ui/materials/brass.png",512,512},{"crimson","generated/ui/materials/crimson.png",512,512},{"obsidian","generated/ui/materials/obsidian.png",512,512},{"wood","generated/ui/materials/wood.png",512,512},{"parchment","generated/ui/materials/parchment.png",512,512},{"leather","generated/ui/materials/leather.png",512,512},{"iron","generated/ui/materials/iron.png",512,512},{"enamel","generated/ui/materials/enamel.png",512,512}}};
    return definitions;
}
PixelMesh pixel_material_mesh(float width,float height,float texel_scale,float edge,float corner,float depth,Rml::Colourb tint,bool tab,float gap_start,float gap_width) {
    PixelMesh result;
    if(!std::isfinite(width)||!std::isfinite(height)||!std::isfinite(texel_scale)||width<=0||height<=0||texel_scale<=0||width>32768||height>32768)return result;
    width=std::round(width);height=std::round(height);
    if(width<=0||height<=0)return result;
    const auto finite=[](float value){return std::isfinite(value)?value:0.f;};
    edge=finite(edge);corner=finite(corner);depth=finite(depth);gap_start=finite(gap_start);gap_width=finite(gap_width);
    const auto dim=[&](int pigment){return Rml::Colourb(static_cast<Rml::byte>(tint.red*pigment/255),static_cast<Rml::byte>(tint.green*pigment/255),static_cast<Rml::byte>(tint.blue*pigment/255),tint.alpha);};
    PixelMesh regions;
    if(tab) {
        const float neck=std::clamp(std::round(depth),0.f,std::max(0.f,height-1));
        regions=pixel_tab_mesh(width,height-neck,edge,corner,0,tint,dim(105),tint);
        quad(regions,0,height-neck,width,neck,tint);
    } else regions=pixel_rim_mesh(width,height,edge,corner,depth,tint,dim(105),dim(152));
    const float tile_span=std::max(1.f,std::round(512.f*texel_scale));
    const float gap_left=std::clamp(std::round(gap_start),0.f,width),gap_right=std::clamp(std::round(gap_start+std::max(0.f,gap_width)),0.f,width);
    const float gap_bottom=std::min(height,std::max(1.f,std::round(6.f*texel_scale)));
    const auto tiled=[&](float x,float y,float w,float h,Rml::Colourb color) {
        if(w<=0||h<=0||color.alpha==0)return;
        const float right=x+w,bottom=y+h;
        for(float ty=y;ty<bottom;) {
            const float origin_y=std::floor(ty/tile_span)*tile_span,end_y=std::min(bottom,origin_y+tile_span);
            for(float tx=x;tx<right;) {
                const float origin_x=std::floor(tx/tile_span)*tile_span,end_x=std::min(right,origin_x+tile_span);
                const int n=static_cast<int>(result.vertices.size());
                const Rml::Vector2f uv0{(tx-origin_x)/tile_span,(ty-origin_y)/tile_span},uv1{(end_x-origin_x)/tile_span,(end_y-origin_y)/tile_span};
                result.vertices.push_back({{tx,ty},color,uv0});result.vertices.push_back({{end_x,ty},color,{uv1.x,uv0.y}});
                result.vertices.push_back({{end_x,end_y},color,uv1});result.vertices.push_back({{tx,end_y},color,{uv0.x,uv1.y}});
                for(int index:{0,1,2,0,2,3})result.indices.push_back(n+index);
                tx=end_x;
            }
            ty=end_y;
        }
    };
    for(std::size_t i=0;i+3<regions.vertices.size();i+=4) {
        const auto& a=regions.vertices[i];const auto& b=regions.vertices[i+2];
        const float x=a.position.x,y=a.position.y,right=b.position.x,bottom=b.position.y;
        if(tab||gap_right<=gap_left||y>=gap_bottom||right<=gap_left||x>=gap_right)tiled(x,y,right-x,bottom-y,a.colour);
        else {
            const float clipped_bottom=std::min(bottom,gap_bottom);
            tiled(x,y,std::max(0.f,std::min(right,gap_left)-x),clipped_bottom-y,a.colour);
            const float clipped_right=std::max(x,gap_right);
            tiled(clipped_right,y,std::max(0.f,right-clipped_right),clipped_bottom-y,a.colour);
            if(bottom>gap_bottom)tiled(x,gap_bottom,right-x,bottom-gap_bottom,a.colour);
        }
    }
    return result;
}
void pixel_glyph_dilate(Rml::byte* destination,Rml::Vector2i size,int stride,const Rml::FontGlyph& glyph,int radius) {
    if(!destination || size.x<=0 || size.y<=0 || stride<size.x*4 || radius<1 || radius>16) return;
    const bool rgba=glyph.color_format==Rml::ColorFormat::RGBA8;
    const int channels=rgba?4:1;
    for(int y=0;y<size.y;++y) for(int x=0;x<size.x;++x) {
        bool covered=false;
        if(glyph.bitmap_data) for(int dy=-radius;dy<=radius && !covered;++dy) {
            const int sy=y-radius+dy;
            if(sy<0 || sy>=glyph.bitmap_dimensions.y) continue;
            for(int dx=-radius;dx<=radius;++dx) {
                const int sx=x-radius+dx;
                if(sx<0 || sx>=glyph.bitmap_dimensions.x) continue;
                if(glyph.bitmap_data[(sy*glyph.bitmap_dimensions.x+sx)*channels+(rgba?3:0)]>=128) { covered=true;break; }
            }
        }
        auto* pixel=destination+y*stride+x*4;
        pixel[0]=pixel[1]=pixel[2]=255;pixel[3]=covered?255:0;
    }
}
void pixel_glyph_bolden(Rml::byte* destination,Rml::Vector2i size,int stride,const Rml::FontGlyph& glyph,int width) {
    if(!destination || size.x<=0 || size.y<=0 || stride<size.x*4 || width<1 || width>16) return;
    // One-sided horizontal emboldening adds genuine pigment to vertical strokes
    // without closing both sides of narrow CJK counters or widening row gaps.
    const bool rgba=glyph.color_format==Rml::ColorFormat::RGBA8;
    const int channels=rgba?4:1;
    for(int y=0;y<size.y;++y) for(int x=0;x<size.x;++x) {
        bool covered=false;
        if(glyph.bitmap_data && y<glyph.bitmap_dimensions.y) for(int dx=0;dx<=width;++dx) {
            const int sx=x-dx;
            if(sx<0 || sx>=glyph.bitmap_dimensions.x) continue;
            if(glyph.bitmap_data[(y*glyph.bitmap_dimensions.x+sx)*channels+(rgba?3:0)]>=128) { covered=true;break; }
        }
        auto* pixel=destination+y*stride+x*4;
        pixel[0]=pixel[1]=pixel[2]=255;pixel[3]=covered?255:0;
    }
}
void register_pixel_decorator() {
    // Factory retains a non-owning pointer; the instancer must outlive contexts.
    static PixelInstancer instancer;
    static PixelOutlineInstancer outline;
    static RimInstancer rim;
    static MaterialInstancer material(false),material_tab(true);
    Rml::Factory::RegisterDecoratorInstancer("pixel",&instancer);
    Rml::Factory::RegisterDecoratorInstancer("pixel-down",&instancer);
    Rml::Factory::RegisterDecoratorInstancer("pixel-up",&instancer);
    Rml::Factory::RegisterDecoratorInstancer("pixel-rim",&rim);
    Rml::Factory::RegisterDecoratorInstancer("pixel-tab",&rim);
    Rml::Factory::RegisterDecoratorInstancer("physical-material",&material);
    Rml::Factory::RegisterDecoratorInstancer("physical-tab",&material_tab);
    Rml::Factory::RegisterFontEffectInstancer("pixel-outline",&outline);
    Rml::Factory::RegisterFontEffectInstancer("pixel-bold",&outline);
}
}
