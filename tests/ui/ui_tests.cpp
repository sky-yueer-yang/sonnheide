#include "ui.hpp"
#include "pixel_decorator.hpp"
#include <RmlUi/Core.h>
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
        // RmlUi 5 reports rejected formatting as a warning, not a test failure.
        // Capture that precise rejection without making unrelated warnings fatal.
        if(message.find("will not be formatted")!=Rml::String::npos) layout_rejections.push_back(message);
        return true;
    }
    std::vector<Rml::String> layout_rejections;
};
struct Render final:Rml::RenderInterface {
    void RenderGeometry(Rml::Vertex* vertices,int count,int*,int,Rml::TextureHandle texture,const Rml::Vector2f&) override{
        if(texture!=0 && count==4) {
            bool untinted=true;
            for(int i=0;i<count;++i) untinted=untinted && vertices[i].colour.red==255 && vertices[i].colour.green==255 && vertices[i].colour.blue==255 && vertices[i].colour.alpha==255;
            if(untinted) full_colour_sprite=true;
        }
        if(texture!=0 || count<4) return;
        ++pixel_draws;
        for(int i=0;i<count;++i) {
            const auto c=vertices[i].colour;
            if(c.red==8 && c.green==44 && c.blue==59 && c.alpha==204) translucent_panel=true;
            if(c.red==6 && c.green==43 && c.blue==57 && c.alpha==200) translucent_dock=true;
            if(c.red==10 && c.green==40 && c.blue==54 && c.alpha==176) recessed_input=true;
            if(c.red==9 && c.green==39 && c.blue==53 && c.alpha==232) tooltip_fill=true;
            if(c.red==121 && c.green==70 && c.blue==63 && c.alpha==204) selected_fill=true;
            if(c.red==149 && c.green==85 && c.blue==72 && c.alpha==220) selected_focus=true;
        }
    }
    void EnableScissorRegion(bool) override{}
    void SetScissorRegion(int,int,int,int) override{}
    bool LoadTexture(Rml::TextureHandle& texture,Rml::Vector2i& size,const Rml::String&) override{texture=++next;size={96,96};return true;}
    bool GenerateTexture(Rml::TextureHandle& texture,const Rml::byte*,const Rml::Vector2i&) override{texture=++next;return true;}
    Rml::TextureHandle next=0;
    int pixel_draws=0;
    bool translucent_panel=false,translucent_dock=false,recessed_input=false,tooltip_fill=false,selected_fill=false,selected_focus=false,full_colour_sprite=false;
    void reset_draws(){pixel_draws=0;translucent_panel=translucent_dock=recessed_input=tooltip_fill=selected_fill=selected_focus=full_colour_sprite=false;}
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
        const auto panel=pixel_frame_mesh(80*density,40*density,2*density,2*density,{8,44,59,204},{59,98,118,184},{3,26,38,208});
        require(covers(panel,40*density,20*density) && !covers(panel,.5f*density,.5f*density),"Translucent frame must preserve actual cut corners and centre fill");
        bool fill=false,light=false,dark=false;
        for(const auto& vertex:panel.vertices) {
            const auto c=vertex.colour;
            fill=fill || (c.red==8 && c.green==44 && c.blue==59 && c.alpha==204);
            light=light || (c.red==59 && c.green==98 && c.blue==118 && c.alpha==184);
            dark=dark || (c.red==3 && c.green==26 && c.blue==38 && c.alpha==208);
            require(vertex.position.x==std::round(vertex.position.x) && vertex.position.y==std::round(vertex.position.y),"Inset translucent frames must use whole raster edges at both densities");
        }
        require(fill && light && dark,"Translucent frame must preserve the actual distinct fill/bevel alpha values");
        int centre_layers=0;
        for(std::size_t i=0;i+3<panel.vertices.size();i+=4) {
            const auto a=panel.vertices[i].position,b=panel.vertices[i+2].position;
            if(a.x<39.5f*density && b.x>39.5f*density && a.y<19.5f*density && b.y>19.5f*density) ++centre_layers;
        }
        require(centre_layers==1,"A translucent frame centre must blend exactly once; an outer dark shell must not compound its opacity");

    }
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
Action single(Ui& ui,ActionKind expected) {
    auto actions=ui.take_actions();require(actions.size()==1,"Exactly one typed intention should leave one interaction");require(actions[0].kind==expected,"Unexpected application command");return actions[0];
}
void scenario(const std::filesystem::path& root,int width,int height,float density,Render& render) {
    Ui ui(root);std::string error;require(ui.initialize(static_cast<int>(width*density),static_cast<int>(height*density),density,error),"Native UI initialization failed");
    View view;view.creation.generation=17;view.creation.name="Welt";view.creation.earth_available=true;view.can_continue=false;ui.set_view(view);ui.update();
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
        require(ui.context().GetDocument(0)->GetElementById("main-title")->GetComputedValues().font_weight()==Rml::Style::FontWeight::Normal,"Important text must not request a nonexistent bold face");

    }
    require(!ui.blocks_world_input(),"Ordinary menu/world surface is not a modal input barrier");
    click(ui,"continue-world",density);require(ui.take_actions().empty(),"Unavailable Continue must not issue a fake load");
    click(ui,"new-world",density);single(ui,ActionKind::NewWorld);
    view.screen=Screen::Creation;ui.set_view(view);ui.update();
    for(int size:{256,512,1024,2048,4096}) {
        const auto id="size-"+std::to_string(size);click(ui,id.c_str(),density);
        const auto preset=single(ui,ActionKind::SetCreationSize);
        require(preset.a==size && preset.b==size && preset.generation==17,"A physical-size preset must emit one generation-bound real width/length intent");
    }
    view.creation.mode=CreationMode::Earth;view.creation.selection_visible=true;
    view.creation.selection_left=.2;view.creation.selection_top=.1;view.creation.selection_right=.7;view.creation.selection_bottom=.6;
    ui.set_view(view);ui.update();
    for(const auto locale:{Locale::Chinese,Locale::English,Locale::German}) {
        view.locale=locale;ui.set_view(view);ui.update();layout_contract(ui);
        auto* controls=ui.context().GetDocument(0)->GetElementById("map-controls");
        auto* row=ui.context().GetDocument(0)->GetElementById("map-button-row");
        require(controls->GetComputedValues().display()==Rml::Style::Display::Block && controls->GetComputedValues().position()==Rml::Style::Position::Absolute && row->GetComputedValues().display()==Rml::Style::Display::Flex,"Floating map actions must use an absolute block wrapper around a normally formatted flex row");
        const auto frame=rect(ui,"map-controls",density),viewport=rect(ui,"scene-viewport",density);
        require(frame.w>=263.85f && frame.h>=48 && frame.x>=viewport.x-.15f && frame.y>=viewport.y-.15f && frame.x+frame.w<=viewport.x+viewport.w+.15f && frame.y+frame.h<=viewport.y+viewport.h+.15f,"Compact floating map actions must have actual geometry wholly inside the map viewport");
        const std::array<const char*,6> ids={"map-out","map-in","map-left","map-right","map-up","map-down"};
        const std::array<int,6> horizontal={-1,1,-1,1,0,0},vertical={0,0,0,0,1,-1};
        float previous_right=frame.x;
        const auto first=rect(ui,ids[0],density);
        for(std::size_t i=0;i<ids.size();++i) {
            const auto slot=rect(ui,ids[i],density);
            require(slot.w>=39.85f && slot.h>=39.85f && std::abs(slot.y-first.y)<.15f && slot.x>=previous_right-.15f,"All six map actions need separate real 40dp slots on a formatted row");
            require(slot.x>=frame.x-.15f && slot.y>=frame.y-.15f && slot.x+slot.w<=frame.x+frame.w+.15f && slot.y+slot.h<=frame.y+frame.h+.15f,"Every map button must fit inside its floating parent and map viewport");
            previous_right=slot.x+slot.w;
            click(ui,ids[i],density);
            const auto action=single(ui,i<2?ActionKind::MapZoom:ActionKind::MapPan);
            require(action.a==horizontal[i] && action.b==vertical[i] && action.generation==17,"Each actual map pointer click must emit its exact generation-bound zoom or pan intention");
        }
    }
    auto* map_selection=ui.context().GetDocument(0)->GetElementById("earth-selection");
    auto* retained_name=ui.context().GetDocument(0)->GetElementById("creation-name");
    const auto map=rect(ui,"scene-viewport",density),selected_region=rect(ui,"earth-selection",density);
    require(std::abs(selected_region.x-map.x-map.w*.2f)<1.1f && std::abs(selected_region.y-map.y-map.h*.1f)<1.1f && std::abs(selected_region.w-map.w*.5f)<1.1f,"Earth selection overlay must track the live normalized viewport rectangle");
    require(map_selection->GetComputedValues().pointer_events()==Rml::Style::PointerEvents::None,"Continuous selection overlay must never capture the actual map pointer");
    for(int frame=0;frame<20;++frame) {
        view.creation.selection_right=.7+frame*.01;view.creation.east=15+frame*.1;
        view.creation.height=1024+frame;
        ui.set_view(view);ui.update();
        require(ui.context().GetDocument(0)->GetElementById("earth-selection")==map_selection && ui.context().GetDocument(0)->GetElementById("creation-name")==retained_name,"Motion-only updates must retain the live selection and existing form elements");
        require(ui.take_actions().empty(),"Continuous map presentation synchronization must not emit commands");
        require(dynamic_cast<Rml::ElementFormControl*>(ui.context().GetDocument(0)->GetElementById("creation-height"))->GetValue()==std::to_string(view.creation.height),"Live derived Earth length must update the single authoritative dimension control");
    }
    click(ui,"size-4096",density);const auto earth_size=single(ui,ActionKind::SetCreationSize);
    require(earth_size.a==4096 && earth_size.b==view.creation.height,"Earth physical-width preset must preserve controller-derived map length");
    view.creation.mode=CreationMode::Blank;view.creation.selection_visible=false;ui.set_view(view);ui.update();
    require(map_selection->GetComputedValues().display()==Rml::Style::Display::None,"Blank worlds must not retain an Earth selection overlay");
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
        const auto blank=rect(ui,"blank-mode",density),earth=rect(ui,"earth-mode",density),controls=rect(ui,"creation-controls",density,Rml::Box::CONTENT);
        require(std::abs(blank.y-earth.y)<.15 && blank.x>=controls.x-.15 && earth.x+earth.w<=controls.x+controls.w+.15,"Creation mode choices must share a row with padding included in their widths");
    }
    ui.activate("create");require(ui.take_actions().empty(),"Create remains unavailable before real preview");
    auto* width_input=dynamic_cast<Rml::ElementFormControl*>(ui.context().GetDocument(0)->GetElementById("creation-width"));
    width_input->SetValue("x");ui.take_actions();ui.activate("preview");
    require(ui.blocks_world_input() && ui.take_actions().empty(),"Local numeric validation error is a modal barrier, not a core preview command");
    ui.activate("error-dismiss");require(!ui.blocks_world_input() && ui.take_actions().empty(),"Dismissing local validation restores camera without a World command");
    width_input->SetValue("128");ui.take_actions();
    ui.activate("preview");require(single(ui,ActionKind::RequestPreview).generation==17,"Preview must retain actual candidate generation");
    view.creation.preview_ready=true;view.creation.preview_token=23;ui.set_view(view);ui.update();
    ui.activate("create");const auto create=single(ui,ActionKind::CreateWorld);require(create.generation==17 && create.token==23,"Create must commit the displayed preview only");
    ui.activate("cancel-create");single(ui,ActionKind::CancelCreation);
    view.screen=Screen::World;view.world.session=44;view.world.name="世界 <new>";view.world.cells=16384;view.world.dry_cells=100;ui.set_view(view);ui.update();
    const std::array<const char*,8> tabs={"tab-observe","tab-terrain","tab-life","tab-civilization","tab-construction","tab-economy","tab-world","tab-settings"};
    for(const auto locale:{Locale::Chinese,Locale::English,Locale::German}) {
        view.locale=locale;ui.set_view(view);ui.update();layout_contract(ui);
        const int columns=8;
        const auto bar=rect(ui,"bottom-bar",density,Rml::Box::CONTENT);
        const float pixel_tolerance=1.1f/density;
        const auto outer_bar=rect(ui,"bottom-bar-frame",density);
        require(std::abs(outer_bar.y+outer_bar.h-height)<pixel_tolerance,"Bottom section toolbar must remain anchored to the viewport edge through its absolute wrapper");
        for(int i=0;i<8;++i) {
            const auto button=rect(ui,tabs[i],density),row=rect(ui,tabs[(i/columns)*columns],density);
            require(std::abs(button.y-row.y)<.15,"Compact category tabs must share one row at all supported widths");
            require(button.x>=bar.x-pixel_tolerance && button.x+button.w<=bar.x+bar.w+pixel_tolerance && button.y+button.h<=height+pixel_tolerance,"Bottom button padding must not overflow the viewport or content row");
            require(std::abs(button.w-48)<pixel_tolerance,"Category tabs must use compact fixed 48dp widths rather than filling an eighth of the display");
            auto* element=ui.context().GetDocument(0)->GetElementById(tabs[i]);
            const auto icon=element->GetChild(0)->GetBox().GetSize(Rml::Box::BORDER);
            require(std::abs(icon.x/density-32)<.15 && std::abs(icon.y/density-32)<.15,"Category silhouettes must use whole 32dp logical pixels");
            require(element->GetComputedValues().font_size()==12*density,"Bottom text remains a real compact 12dp pixel font at every DPI");
            require(!element->GetAttribute<Rml::String>("data-tooltip-key","").empty(),"Short toolbar labels require their complete localized name tooltip");

        }
        require(ui.take_actions().empty(),"Layout/localization queries must not emit commands");
    }
    const auto observe_panel=rect(ui,"section-tools",density),camera=rect(ui,"camera-home",density),help=rect(ui,"camera-help",density);
    require(observe_panel.w<=width+.15f && std::abs(camera.w-44)<.15f && std::abs(help.w-44)<.15f && std::abs(camera.y-help.y)<.15f,"Observe tools must occupy compact 44dp tool slots beneath the category row");
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
    for(const auto* id:{"tab-terrain","tab-life","tab-civilization","tab-construction","tab-economy"}) {click(ui,id,density);require(ui.take_actions().empty(),"Unavailable domain tab must not report success");}
    render.reset_draws();ui.render();require(render.pixel_draws>0 && render.translucent_panel && render.translucent_dock,"Real RmlUi must submit true navy panel alpha 204 and dock alpha 200");
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
        require(world_tools.w<=width+.15f && std::abs(save.w-44)<.15f && std::abs(load.w-44)<.15f && std::abs(menu.w-44)<.15f,"World tools must use 44dp individual slots in the lower tool dock");
        require(std::abs(save.y-load.y)<.15f && std::abs(save.y-menu.y)<.15f,"Save, load and menu controls must share a native flex row in all three locales");
        const auto clock_group=rect(ui,"world-clock-group",density),save_group=rect(ui,"world-save-group",density),category=rect(ui,"bottom-bar",density);
        require(clock_group.h>=44 && save_group.h>=44 && clock_group.y>=category.y+category.h-.15f,"Tool groups must have real nonzero layout beneath the category row");
        const std::array<const char*,5> clock_buttons={"pause","speed-one","speed-four","speed-sixteen","speed-sixtyfour"};
        float previous_right=clock_group.x;
        for(const auto* id:clock_buttons) {
            const auto slot=rect(ui,id,density);
            require(slot.h>=43.85f && slot.w>=43.85f && std::abs(slot.y-clock_group.y)<.15f,"Every time control must have real 44dp geometry in its clock row");
            require(slot.x>=previous_right-.15f && slot.x+slot.w<=clock_group.x+clock_group.w+.15f && slot.y+slot.h<=height+.15f,"Clock controls must occupy separate visible slots within their block wrapper");
            previous_right=slot.x+slot.w;
        }
        previous_right=save_group.x;
        for(const auto* id:{"save","world-load","return-menu"}) {
            const auto slot=rect(ui,id,density);
            require(slot.h>=43.85f && slot.x>=previous_right-.15f && slot.x+slot.w<=save_group.x+save_group.w+.15f && slot.y>=save_group.y-.15f && slot.y+slot.h<=save_group.y+save_group.h+.15f,"Every save/menu action must have nonzero visible geometry inside its own block wrapper");
            previous_right=slot.x+slot.w;
        }
        const auto light=rect(ui,"age-light",density),dark=rect(ui,"age-darkness",density);
        require(std::abs(light.w-44)<.15f && std::abs(dark.w-44)<.15f && std::abs(light.y-dark.y)<.15f,"Age tools must occupy two compact neighbouring 44dp slots");
        require(ui.take_actions().empty(),"Responsive tool layout and locale changes must not produce intents");
    }
    click(ui,"speed-sixtyfour",density);require(single(ui,ActionKind::SetSpeed).a==64,"Only approved clock multipliers are exposed");
    click(ui,"age-darkness",density);require(single(ui,ActionKind::SetAgeDarkness).generation==44,"Age intent belongs to current session");
    click(ui,"world-info",density);single(ui,ActionKind::OpenWorldInspector);layout_contract(ui);
    require(ui.blocks_world_input(),"World inspector must block camera input independently of pointer hover");
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
    click(ui,"help-close",density);single(ui,ActionKind::CloseHelp);require(!ui.blocks_world_input(),"Closing help releases camera input");
    click(ui,"tab-settings",density);single(ui,ActionKind::OpenSettings);ui.update();layout_contract(ui);
    require(ui.blocks_world_input(),"Settings must block camera input even without text focus");
    for(const auto locale:{Locale::Chinese,Locale::English,Locale::German}) {
        view.locale=locale;ui.set_view(view);ui.update();layout_contract(ui);
        const auto panel=rect(ui,"settings-panel",density);require(panel.x>=0 && panel.x+panel.w<=width+.1 && panel.y>=0 && panel.y+panel.h<=height+.1,"Native settings panel clips outside viewport");
        const auto volume=rect(ui,"audio-volume",density);require(volume.y>panel.y+100,"Settings business rows must be block layout, not one inline run");
        auto* close=ui.context().GetDocument(0)->GetElementById("settings-close");close->ScrollIntoView(false);ui.update();
        const auto close_rect=rect(ui,"settings-close",density);
        require(close_rect.y>=panel.y-.15 && close_rect.y+close_rect.h<=panel.y+panel.h+.15,"The last control of long localized settings must be reachable through real native scrolling");
        auto* settings=ui.context().GetDocument(0)->GetElementById("settings-panel");
        if(settings->GetScrollHeight()>settings->GetClientHeight()+.5f) require(settings->GetScrollTop()>0,"Overflowing localized settings must genuinely scroll to their final action");
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
        auto* document=ui.context().GetDocument(0);
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
