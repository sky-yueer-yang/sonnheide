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
void register_pixel_decorator() {
    // Factory retains a non-owning pointer; the instancer must outlive contexts.
    static PixelInstancer instancer;
    Rml::Factory::RegisterDecoratorInstancer("pixel",&instancer);
    Rml::Factory::RegisterDecoratorInstancer("pixel-down",&instancer);
    Rml::Factory::RegisterDecoratorInstancer("pixel-up",&instancer);
}
}
