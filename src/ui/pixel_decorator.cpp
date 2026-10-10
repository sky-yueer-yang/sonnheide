#include "pixel_decorator.hpp"
#include <RmlUi/Core/Decorator.h>
#include <RmlUi/Core/DecoratorInstancer.h>
#include <RmlUi/Core/ComputedValues.h>
#include <RmlUi/Core/Element.h>
#include <RmlUi/Core/Factory.h>
#include <RmlUi/Core/Geometry.h>
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
class FrameDecorator final:public Rml::Decorator {
public:
    FrameDecorator(Rml::Colourb fill,Rml::Colourb light,Rml::Colourb dark,Rml::Property edge,Rml::Property corner):fill_(fill),light_(light),dark_(dark),edge_(std::move(edge)),corner_(std::move(corner)) {}
    Rml::DecoratorDataHandle GenerateElementData(Rml::Element* element) const override {
        const auto size=element->GetBox().GetSize(Rml::Box::PADDING);
        auto fill=fill_,light=light_,dark=dark_;
        const auto opacity=element->GetComputedValues().opacity();
        for(auto* colour:{&fill,&light,&dark}) colour->alpha=static_cast<Rml::byte>(colour->alpha*opacity);
        auto mesh=pixel_frame_mesh(std::round(size.x),std::round(size.y),element->ResolveNumericProperty(&edge_,0),element->ResolveNumericProperty(&corner_,0),fill,light,dark);
        auto* geometry=new Rml::Geometry(element);
        geometry->GetVertices()=std::move(mesh.vertices);geometry->GetIndices()=std::move(mesh.indices);
        return reinterpret_cast<Rml::DecoratorDataHandle>(geometry);
    }
    void ReleaseElementData(Rml::DecoratorDataHandle data) const override { delete reinterpret_cast<Rml::Geometry*>(data); }
    void RenderElement(Rml::Element* element,Rml::DecoratorDataHandle data) const override {
        if(auto* geometry=reinterpret_cast<Rml::Geometry*>(data)) geometry->Render(element->GetAbsoluteOffset(Rml::Box::PADDING).Round());
    }
private:
    Rml::Colourb fill_,light_,dark_; Rml::Property edge_,corner_;
};
class FrameInstancer final:public Rml::DecoratorInstancer {
public:
    FrameInstancer() {
        fill_=RegisterProperty("fill","#082c3bcc").AddParser("color").GetId();
        light_=RegisterProperty("light","#3b6276b8").AddParser("color").GetId();
        dark_=RegisterProperty("dark","#031a26d0").AddParser("color").GetId();
        edge_=RegisterProperty("edge","2dp").AddParser("length").GetId();
        corner_=RegisterProperty("corner","2dp").AddParser("length").GetId();
        RegisterShorthand("decorator","fill, light, dark, edge, corner",Rml::ShorthandType::FallThrough);
    }
    Rml::SharedPtr<Rml::Decorator> InstanceDecorator(const Rml::String&,const Rml::PropertyDictionary& properties,const Rml::DecoratorInstancerInterface&) override {
        return Rml::MakeShared<FrameDecorator>(properties.GetProperty(fill_)->Get<Rml::Colourb>(),properties.GetProperty(light_)->Get<Rml::Colourb>(),properties.GetProperty(dark_)->Get<Rml::Colourb>(),*properties.GetProperty(edge_),*properties.GetProperty(corner_));
    }
private: Rml::PropertyId fill_,light_,dark_,edge_,corner_;
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
PixelMesh pixel_frame_mesh(float width,float height,float edge,float corner,Rml::Colourb fill,Rml::Colourb light,Rml::Colourb dark) {
    PixelMesh mesh;
    if(!std::isfinite(width)||!std::isfinite(height)||width<=0||height<=0) return mesh;
    const float e=std::isfinite(edge)?std::clamp(std::round(edge),1.f,std::max(1.f,std::floor(std::min(width,height)/6))):1.f;
    const float c=std::isfinite(corner)?std::clamp(std::round(corner),0.f,std::floor(std::min(width,height)/8)):0.f;
    const float inner_corner=std::max(0.f,c-e),inner_height=height-2*e;
    const auto inset=[](float y,float h,float corner){return y<corner || y>=h-corner?2*corner:y<2*corner || y>=h-2*corner?corner:0.f;};
    // Partition at every outer/inner stair height. Fill and bevel bands are
    // disjoint, so a translucent centre is blended exactly once, never over
    // a second full-size dark shell. The geometry remains whole-pixel.
    Rml::Vector<float> rows={0,c,2*c,height-2*c,height-c,height,e,e+inner_corner,e+2*inner_corner,height-e-2*inner_corner,height-e-inner_corner,height-e};
    for(auto& y:rows) y=std::clamp(y,0.f,height);
    std::sort(rows.begin(),rows.end());rows.erase(std::unique(rows.begin(),rows.end()),rows.end());
    for(std::size_t i=1;i<rows.size();++i) {
        const float top=rows[i-1],h=rows[i]-top,mid=top+h/2;
        const float outer=inset(mid,height,c),right=width-outer;
        if(mid<e || mid>=height-e || inner_height<=0 || width<=2*e) {
            quad(mesh,outer,top,right-outer,h,mid<e?light:dark);
            continue;
        }
        const float inner=std::max(outer,e+inset(mid-e,inner_height,inner_corner)),inner_right=std::min(right,width-e-inset(mid-e,inner_height,inner_corner));
        quad(mesh,outer,top,inner-outer,h,light);
        quad(mesh,inner,top,inner_right-inner,h,fill);
        quad(mesh,inner_right,top,right-inner_right,h,dark);
    }
    return mesh;
}
void register_pixel_decorator() {
    // Factory retains a non-owning pointer; the instancer must outlive contexts.
    static PixelInstancer instancer;
    static FrameInstancer frame;
    Rml::Factory::RegisterDecoratorInstancer("pixel",&instancer);
    Rml::Factory::RegisterDecoratorInstancer("pixel-down",&instancer);
    Rml::Factory::RegisterDecoratorInstancer("pixel-up",&instancer);
    Rml::Factory::RegisterDecoratorInstancer("pixel-frame",&frame);
}
}
