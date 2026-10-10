#include "ui.hpp"
#include "pixel_decorator.hpp"
#include <RmlUi/Core.h>
#include <RmlUi/Core/FontEngineInterface.h>
#include <RmlUi/Core/Elements/ElementFormControl.h>
#include <RmlUi/Core/Elements/ElementFormControlSelect.h>
#include <array>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
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
    void RenderGeometry(Rml::Vertex* vertices,int count,int*,int,Rml::TextureHandle texture,const Rml::Vector2f&) override{
        if(texture!=0 && count>4) {
            for(int i=0;i<count;++i) {
                const auto c=vertices[i].colour;
                if(c.red==255 && c.green==170 && c.blue==0 && c.alpha==255) bold_pigment=true;
                if(c.red==255 && c.green==255 && c.blue==255 && c.alpha==255) white_glyph_outline=true;
            }
        }
        if(texture!=0 && count==4) {
            bool untinted=true;
            for(int i=0;i<count;++i) untinted=untinted && vertices[i].colour.red==255 && vertices[i].colour.green==255 && vertices[i].colour.blue==255 && vertices[i].colour.alpha==255;
            if(untinted) full_colour_sprite=true;
        }
        if(texture!=0 || count<4) return;
        ++pixel_draws;
        for(int i=0;i<count;++i) {
            const auto c=vertices[i].colour;
            if(c.red==4 && c.green==37 && c.blue==55 && c.alpha==204) translucent_panel=true;
            if(c.red==7 && c.green==45 && c.blue==88 && c.alpha==208) translucent_dock=true;
            if(c.red==4 && c.green==28 && c.blue==50 && c.alpha==176) recessed_input=true;
            if(c.red==4 && c.green==31 && c.blue==58 && c.alpha==232) tooltip_fill=true;
            if(c.red==190 && c.green==63 && c.blue==37 && c.alpha==232) selected_fill=true;
            if(c.red==218 && c.green==77 && c.blue==43 && c.alpha==240) selected_focus=true;
        }
    }
    void EnableScissorRegion(bool) override{}
    void SetScissorRegion(int,int,int,int) override{}
    bool LoadTexture(Rml::TextureHandle& texture,Rml::Vector2i& size,const Rml::String&) override{texture=++next;size={96,96};return true;}
    bool GenerateTexture(Rml::TextureHandle& texture,const Rml::byte*,const Rml::Vector2i&) override{texture=++next;return true;}
    Rml::TextureHandle next=0;
    int pixel_draws=0;
    bool translucent_panel=false,translucent_dock=false,recessed_input=false,tooltip_fill=false,selected_fill=false,selected_focus=false,full_colour_sprite=false,bold_pigment=false,white_glyph_outline=false;
    void reset_draws(){pixel_draws=0;translucent_panel=translucent_dock=recessed_input=tooltip_fill=selected_fill=selected_focus=full_colour_sprite=bold_pigment=white_glyph_outline=false;}
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
struct Rect {float x,y,w,h;};
Rect rect(Ui& ui,const char* id,float density,Rml::Box::Area area=Rml::Box::BORDER) {
    auto* el=ui.context().GetDocument(0)->GetElementById(id); require(el!=nullptr,"Required native element missing");
    const auto p=el->GetAbsoluteOffset(area),s=el->GetBox().GetSize(area);
    return{p.x/density,p.y/density,s.x/density,s.y/density};
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
void click(Ui& ui,const char* id,float density) {
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
    ui.context().ProcessMouseButtonDown(0,0);ui.context().ProcessMouseButtonUp(0,0);ui.update();layout_contract(ui);
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
    hit_layer_contract(ui,"create","create",density);
}
Action single(Ui& ui,ActionKind expected) {
    auto actions=ui.take_actions();require(actions.size()==1,"Exactly one typed intention should leave one interaction");require(actions[0].kind==expected,"Unexpected application command");return actions[0];
}
void outline_contract(Rml::Element* element,float density,bool title=false,bool red=false) {
    require(element!=nullptr,"Important heading must exist");
    const auto effects=element->GetProperty<Rml::FontEffectsPtr>("font-effect");
    require(effects && effects->list.size()==2,"Actual heading must have separate white silhouette and bold pigment layers");
    const std::array<int,2> radii={static_cast<int>(std::round((title?3.f:2.f)*density)),static_cast<int>(std::round(density))};
    for(std::size_t i=0;i<2;++i) {
        Rml::FontGlyph glyph;Rml::Vector2i origin{0,0},size{1,1};
        require(effects->list[i]->GetGlyphMetrics(origin,size,glyph),"Instanced glyph effect must provide actual expanded bitmap metrics");
        if(i==1 && !title) require(origin.x==0 && origin.y==0 && size.x==1+radii[i] && size.y==1,"Actual heading pigment must use density-scaled one-sided emboldening that preserves CJK counters");
        else require(origin.x==-radii[i] && origin.y==-radii[i] && size.x==1+2*radii[i] && size.y==1+2*radii[i],"Native effect radius must scale in physical whole pixels at each display density");
        const auto color=effects->list[i]->GetColour();
        require(color.alpha==255 && (i==0?(color.red==255 && color.green==255 && color.blue==255):red?(color.red==222 && color.green==64 && color.blue==45):(color.red==255 && color.green==170 && color.blue==0)),"Bold pigment and white outer edge must retain their distinct actual glyph layer colors");
    }
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
        require(ui.context().GetDocument(0)->GetProperty<Rml::String>("font-family")=="Sonn Arcade","Every language must use the admitted arcade font family");
        require(ui.context().GetDocument(0)->GetElementById("new-world")->GetComputedValues().font_size()==12*density,"Home actions must use the compact admitted 12dp pixel face");
        require(ui.context().GetDocument(0)->GetElementById("main-title")->GetComputedValues().font_size()==36*density,"Main hierarchy uses a real three-times pixel font size");
        require(ui.context().GetDocument(0)->GetElementById("main-title")->GetComputedValues().font_weight()==Rml::Style::FontWeight::Normal,"Actual glyph emboldening must keep the real Regular face, with independent pigment and outline effects");
        auto* title=ui.context().GetDocument(0)->GetElementById("main-title");outline_contract(title,density,true);
        for(int i=0;i<title->GetNumChildren();++i) if(title->GetChild(i)->GetTagName()=="span") outline_contract(title->GetChild(i),density,true);
        const float word_width=Rml::GetFontEngineInterface()->GetStringWidth(title->GetFontFaceHandle(),"SONNHEIDE")/density+6.f;
        require(word_width<=rect(ui,"main-menu",density,Rml::Box::CONTENT).w,"Actual36dp wordmark advances and white glyph edge must fit its menu content width");


    }
    render.reset_draws();ui.render();require(render.bold_pigment && render.white_glyph_outline,"Native important titles must submit actual yellow bold glyph and white outline geometry");
    require(ui.context().GetDocument(0)->GetElementById("main-title")->GetComputedValues().has_font_effect(),"Important title must use the registered actual glyph effect");
    require(!ui.blocks_world_input(),"Ordinary menu/world surface is not a modal input barrier");
    click(ui,"continue-world",density);require(ui.take_actions().empty(),"Unavailable Continue must not issue a fake load");
    click(ui,"new-world",density);single(ui,ActionKind::NewWorld);
    view.screen=Screen::Creation;ui.set_view(view);ui.update();
    for(int size:{256,512,1024,2048,4096}) {
        const auto id="size-"+std::to_string(size);click(ui,id.c_str(),density);
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
        if(height<=480) require(form->GetScrollHeight()>form->GetClientHeight()+.5f,"The complete centered creation form must retain real scrolling in a short viewport");
        const auto scene=ui.viewport();require(scene.x==0 && scene.y==0 && std::abs(scene.width-width)<.15f && std::abs(scene.height-height)<.15f,"Blank creation uses the complete 3D scene behind its dialog");
        auto* doc=ui.context().GetDocument(0);
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
    require(render.translucent_panel && render.recessed_input,"Creation controls must submit true navy panel alpha 204 and recessed input alpha 176");
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
    click(ui,"create",density);const auto create=single(ui,ActionKind::CreateWorld);require(create.generation==17 && create.token==23,"Create must commit the displayed preview only");
    ui.activate("cancel-create");single(ui,ActionKind::CancelCreation);
    view.screen=Screen::World;view.world.session=44;view.world.name="世界 <new>";view.world.cells=16384;view.world.dry_cells=100;ui.set_view(view);ui.update();
    const std::array<const char*,8> tabs={"tab-observe","tab-terrain","tab-life","tab-civilization","tab-construction","tab-economy","tab-world","tab-settings"};
    for(const auto locale:{Locale::Chinese,Locale::English,Locale::German}) {
        view.locale=locale;ui.set_view(view);ui.update();layout_contract(ui);
        const auto bar=rect(ui,"bottom-bar",density,Rml::Box::CONTENT);
        const float pixel_tolerance=1.1f/density;
        const auto outer_bar=rect(ui,"bottom-bar-frame",density);
        require(std::abs(outer_bar.y+outer_bar.h-height)<pixel_tolerance,"Bottom section toolbar must remain anchored to the viewport edge through its absolute wrapper");
        for(int i=0;i<8;++i) {
            const auto button=rect(ui,tabs[i],density),row=rect(ui,"section-tabs",density);
            require(button.y>=row.y-.15f && button.y+button.h<=outer_bar.y+6.15f,"Folder tabs may overlap the panel by their six-pixel join neck only");
            require(std::abs(button.y+button.h-outer_bar.y-6.f)<.15f && button.h<=44.15f,"Tabs must be low40/44dp folder faces with a real six-pixel join, never tall standalone blocks");
            auto* panel=ui.context().GetDocument(0)->GetElementById("bottom-bar-frame");
            auto* parent=ui.context().GetDocument(0)->GetElementById(tabs[i]);
            while(parent && parent!=panel) parent=parent->GetParentNode();
            require(parent!=panel,"Categories must be separate protrusions, never contained inside the decorated toolbar panel");
            require(button.x>=bar.x-pixel_tolerance && button.x+button.w<=bar.x+bar.w+pixel_tolerance && button.y+button.h<=height+pixel_tolerance,"Bottom button padding must not overflow the viewport or content row");
            require(button.w>=58-pixel_tolerance && button.w<=68+pixel_tolerance,"All eight protruding categories must have substantial bounded slots that fit the viewport");
            auto* element=ui.context().GetDocument(0)->GetElementById(tabs[i]);
            const auto icon=element->GetChild(0)->GetBox().GetSize(Rml::Box::BORDER);
            require(std::abs(icon.x/density-32)<.15 && std::abs(icon.y/density-32)<.15,"Category silhouettes must use whole 32dp logical pixels");
            require(element->GetComputedValues().font_size()==12*density,"Bottom text remains a real compact 12dp pixel font at every DPI");
            require(!element->GetAttribute<Rml::String>("data-tooltip-key","").empty(),"Short toolbar labels require their complete localized name tooltip");

        }
        require(ui.take_actions().empty(),"Layout/localization queries must not emit commands");
    }
    const auto observe_panel=rect(ui,"section-tools",density),camera=rect(ui,"camera-home",density),help=rect(ui,"camera-help",density);
    require(observe_panel.w<=width+.15f && std::abs(camera.w-48)<.15f && std::abs(help.w-48)<.15f && std::abs(camera.y-help.y)<.15f,"Observe tools must occupy compact 48dp tool slots beneath the category row");
    const auto civil=rect(ui,"tab-civilization",density);
    ui.context().ProcessMouseMove(static_cast<int>((civil.x+civil.w/2)*density),static_cast<int>((civil.y+civil.h/2)*density),0);
    ui.update();
    auto* tooltip=ui.context().GetDocument(0)->GetElementById("tool-tooltip");
    require(tooltip->GetComputedValues().display()!=Rml::Style::Display::None && tooltip->GetInnerRML().find("Zivilisation und Institutionen")!=std::string::npos,"Short German toolbar label must expose its real full name on native hover");
    const auto tip=rect(ui,"tool-tooltip",density);
    require(tip.x>=0 && tip.y>=0 && tip.x+tip.w<=width+.15 && tip.y+tip.h<=height+.15,"Native full-name tooltip must stay inside the viewport");
    render.reset_draws();ui.render();require(render.tooltip_fill,"Native full-name tooltip must submit its inset dark material at true alpha 232");
    require(ui.take_actions().empty(),"Tooltip presentation must never become a World command");
    ui.context().ProcessMouseMove(0,0,0);ui.update();
    require(tooltip->GetComputedValues().display()==Rml::Style::Display::None,"Leaving a tool must dismiss its informational tooltip");
    const std::array<const char*,5> future_tabs={"tab-terrain","tab-life","tab-civilization","tab-construction","tab-economy"};
    const std::array<const char*,5> future_panels={"terrain-tools","life-tools","civilization-tools","construction-tools","economy-tools"};
    const std::array<const char*,5> future_buttons={"terrain-pending","life-pending","civilization-pending","construction-pending","economy-pending"};
    for(std::size_t i=0;i<future_tabs.size();++i) {
        click(ui,future_tabs[i],density);require(single(ui,ActionKind::SelectBottomSection).a==static_cast<int>(i)+1,"Every category is navigable and emits one actual section selection");
        for(std::size_t j=0;j<future_panels.size();++j) require((ui.context().GetDocument(0)->GetElementById(future_panels[j])->GetComputedValues().display()!=Rml::Style::Display::None)==(j==i),"Only the selected section contents may occupy the toolbar body");
        click(ui,future_buttons[i],density);require(ui.take_actions().empty(),"Unavailable future tools stay visibly disabled without claiming a World edit");
    }
    click(ui,"tab-observe",density);single(ui,ActionKind::SelectBottomSection);
    render.reset_draws();ui.render();require(render.pixel_draws>0 && render.translucent_panel && render.translucent_dock,"Real RmlUi must submit true navy panel alpha204 and dock alpha208");
    view.presentation_darkness=true;ui.set_view(view);ui.update();
    ui.context().ProcessMouseMove(0,0,0);
    ui.context().GetDocument(0)->GetElementById("tab-observe")->Focus();
    ui.update();render.reset_draws();ui.render();
    require(render.translucent_panel && render.selected_focus,"World Age must preserve translucent panels and visible red keyboard focus");
    ui.context().GetDocument(0)->GetElementById("camera-home")->Focus();
    ui.update();render.reset_draws();ui.render();
    require(render.translucent_panel && render.selected_fill,"Selection remains distinguishable independently of keyboard focus and world Age");
    require(render.full_colour_sprite,"Native colour sprites must be submitted without monochrome tint");
    require(ui.context().GetDocument(0)->GetElementById("camera-home")->GetChild(0)->GetAttribute<Rml::String>("src","")=="../generated/ui/icons/camera-96.png","World Age must not replace the individually coloured icon artwork with a global theme");
    require(ui.take_actions().empty(),"Age presentation synchronization must not mutate World or issue commands");
    click(ui,"tab-world",density);single(ui,ActionKind::SelectBottomSection);
    for(const auto locale:{Locale::Chinese,Locale::English,Locale::German}) {
        view.locale=locale;ui.set_view(view);ui.update();layout_contract(ui);
        const auto world_tools=rect(ui,"section-tools",density),save=rect(ui,"save",density),load=rect(ui,"world-load",density),menu=rect(ui,"return-menu",density);
        require(world_tools.w<=width+.15f && std::abs(save.w-48)<.15f && std::abs(load.w-48)<.15f && std::abs(menu.w-48)<.15f,"World tools must use 48dp individual slots in the lower tool dock");
        require(std::abs(save.y-load.y)<.15f && std::abs(save.y-menu.y)<.15f,"Save, load and menu controls must share a native flex row in all three locales");
        const auto clock_group=rect(ui,"world-clock-group",density),save_group=rect(ui,"world-save-group",density),category=rect(ui,"bottom-bar",density);
        require(clock_group.h>=48 && save_group.h>=48 && clock_group.y>=category.y+category.h-.15f,"Tool groups must have real nonzero layout beneath the category row");
        const std::array<const char*,5> clock_buttons={"pause","speed-one","speed-four","speed-sixteen","speed-sixtyfour"};
        float previous_right=clock_group.x;
        for(const auto* id:clock_buttons) {
            const auto slot=rect(ui,id,density);
            require(slot.h>=47.85f && slot.w>=47.85f && std::abs(slot.y-clock_group.y)<.15f,"Every time control must have real 48dp geometry in its clock row");
            require(slot.x>=previous_right-.15f && slot.x+slot.w<=clock_group.x+clock_group.w+.15f && slot.y+slot.h<=height+.15f,"Clock controls must occupy separate visible slots within their block wrapper");
            previous_right=slot.x+slot.w;
        }
        previous_right=save_group.x;
        for(const auto* id:{"save","world-load","return-menu"}) {
            const auto slot=rect(ui,id,density);
            require(slot.h>=47.85f && slot.x>=previous_right-.15f && slot.x+slot.w<=save_group.x+save_group.w+.15f && slot.y>=save_group.y-.15f && slot.y+slot.h<=save_group.y+save_group.h+.15f,"Every save/menu action must have nonzero visible geometry inside its own block wrapper");
            previous_right=slot.x+slot.w;
        }
        const auto light=rect(ui,"age-light",density),dark=rect(ui,"age-darkness",density);
        require(std::abs(light.w-48)<.15f && std::abs(dark.w-48)<.15f && std::abs(light.y-dark.y)<.15f,"Age tools must occupy two compact neighbouring 48dp slots");
        require(ui.take_actions().empty(),"Responsive tool layout and locale changes must not produce intents");
    }
    click(ui,"speed-sixtyfour",density);require(single(ui,ActionKind::SetSpeed).a==64,"Only approved clock multipliers are exposed");
    click(ui,"age-darkness",density);require(single(ui,ActionKind::SetAgeDarkness).generation==44,"Age intent belongs to current session");
    click(ui,"world-info",density);single(ui,ActionKind::OpenWorldInspector);layout_contract(ui);
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
    click(ui,"help-close",density);single(ui,ActionKind::CloseHelp);require(!ui.blocks_world_input(),"Closing help releases camera input");
    click(ui,"tab-settings",density);require(single(ui,ActionKind::SelectBottomSection).a==7,"The eighth raised category must navigate to settings tools just like the other seven");
    require(!ui.blocks_world_input(),"Selecting settings tools alone must preserve the world view without prematurely opening a modal");
    click(ui,"settings-open",density);single(ui,ActionKind::OpenSettings);ui.update();layout_contract(ui);
    require(ui.blocks_world_input(),"Settings must block camera input even without text focus");
    hit_layer_contract(ui,"tab-settings","settings-overlay",density);
    require(ui.context().GetDocument(0)->GetElementById("tab-settings")->IsClassSet("selected"),"Settings must select its raised category before opening its local modal");
    for(const auto locale:{Locale::Chinese,Locale::English,Locale::German}) {
        view.locale=locale;ui.set_view(view);ui.update();layout_contract(ui);
        auto* settings_heading=ui.context().GetDocument(0)->GetElementById("settings-panel")->GetChild(0)->GetChild(0);outline_contract(settings_heading,density);
        const auto panel=rect(ui,"settings-panel",density);require(panel.x>=0 && panel.x+panel.w<=width+.1 && panel.y>=0 && panel.y+panel.h<=height+.1,"Native settings panel clips outside viewport");
        const auto volume=rect(ui,"audio-volume",density);require(volume.y>panel.y+100,"Settings business rows must be block layout, not one inline run");
        auto* close=ui.context().GetDocument(0)->GetElementById("settings-close");close->ScrollIntoView(false);ui.update();
        const auto close_rect=rect(ui,"settings-close",density);
        require(close_rect.y>=panel.y-.15 && close_rect.y+close_rect.h<=panel.y+panel.h+.15,"The last control of long localized settings must be reachable through real native scrolling");
        auto* settings=ui.context().GetDocument(0)->GetElementById("settings-panel");
        if(settings->GetScrollHeight()>settings->GetClientHeight()+.5f) require(settings->GetScrollTop()>0,"Overflowing localized settings must genuinely scroll to their final action");
        view.error_key="CREATION_DIMENSIONS";view.error_detail="core width 32..4096";view.busy=true;ui.set_view(view);ui.update();
        error_layer_contract(ui,density);
        hit_layer_contract(ui,"tab-settings","error-overlay",density);
        hit_layer_contract(ui,"busy-indicator","error-overlay",density);
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
    click(ui,"back-load",density);single(ui,ActionKind::CancelLoad);
    view.error_key="CREATION_DIMENSIONS";view.error_detail="core width 32..4096";
    for(const auto locale:{Locale::Chinese,Locale::English,Locale::German}) {
        view.locale=locale;ui.set_view(view);ui.update();layout_contract(ui);
        require(ui.blocks_world_input(),"Business errors must block world camera input");
        error_layer_contract(ui,density);
        auto* document=ui.context().GetDocument(0);
        outline_contract(document->GetElementById("error-panel")->GetChild(0),density,false,true);
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
    try { const std::filesystem::path root=argv[1];
        std::ifstream font(root/"fonts/fusion-pixel-12px-proportional-zh_hans.otf",std::ios::binary);
        require(font.good(),"Admitted arcade font file missing");
        font_bytes.assign(std::istreambuf_iterator<char>(font),std::istreambuf_iterator<char>());
        require(!font_bytes.empty() && Rml::LoadFontFace(font_bytes.data(),static_cast<int>(font_bytes.size()),"Sonn Arcade",Rml::Style::FontStyle::Normal,Rml::Style::FontWeight::Normal,true),"Arcade font alias registration failed");
        pixel_geometry_contract();
        for(float density:{1.f,2.f}) {
            for(const auto dimensions:{std::array<int,2>{1280,800},std::array<int,2>{480,640},std::array<int,2>{640,480}}) {
                scenario(root,dimensions[0],dimensions[1],density,render);
                if(!system.layout_rejections.empty()) throw std::runtime_error("Native RmlUi layout rejection: "+system.layout_rejections.front());
            }
        }
    } catch(const std::exception& failure) {std::cerr<<failure.what()<<'\n';result=1;}
    // The memory font alias explicitly borrows these bytes until Shutdown.
    Rml::Shutdown();return result;
}
