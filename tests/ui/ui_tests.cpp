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

using namespace sonnheide::ui;
namespace {
void require(bool condition,const char* why) { if(!condition) throw std::runtime_error(why); }
struct System final:Rml::SystemInterface { double GetElapsedTime() override{return 0;} };
struct Render final:Rml::RenderInterface {
    void RenderGeometry(Rml::Vertex* vertices,int count,int*,int,Rml::TextureHandle texture,const Rml::Vector2f&) override{
        if(texture!=0 && count==4) {
            bool black=true;
            for(int i=0;i<count;++i) black=black && vertices[i].colour.red==0 && vertices[i].colour.green==0 && vertices[i].colour.blue==0 && vertices[i].colour.alpha==255;
            if(black) dark_selection_sprite=true;
        }
        if(texture!=0 || count<20) return;
        ++pixel_draws;
        for(int i=0;i<count;++i) {
            const auto c=vertices[i].colour;
            if(c.red==12 && c.green==24 && c.blue==42 && c.alpha==248) light_panel=true;
            if(c.red==8 && c.green==8 && c.blue==8 && c.alpha==248) dark_panel=true;
            if(c.red==255 && c.green==170 && c.blue==0 && c.alpha==255) dark_selection=true;
            if(c.red==255 && c.green==189 && c.blue==64 && c.alpha==255) dark_selection_focus=true;
        }
    }
    void EnableScissorRegion(bool) override{}
    void SetScissorRegion(int,int,int,int) override{}
    bool LoadTexture(Rml::TextureHandle& texture,Rml::Vector2i& size,const Rml::String&) override{texture=++next;size={96,96};return true;}
    bool GenerateTexture(Rml::TextureHandle& texture,const Rml::byte*,const Rml::Vector2i&) override{texture=++next;return true;}
    Rml::TextureHandle next=0;
    int pixel_draws=0;
    bool light_panel=false,dark_panel=false,dark_selection=false,dark_selection_focus=false,dark_selection_sprite=false;
    void reset_draws(){pixel_draws=0;light_panel=dark_panel=dark_selection=dark_selection_focus=dark_selection_sprite=false;}
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
    }
}
struct Rect {float x,y,w,h;};
Rect rect(Ui& ui,const char* id,float density,Rml::Box::Area area=Rml::Box::BORDER) {
    auto* el=ui.context().GetDocument(0)->GetElementById(id); require(el!=nullptr,"Required native element missing");
    const auto p=el->GetAbsoluteOffset(area),s=el->GetBox().GetSize(area);
    return{p.x/density,p.y/density,s.x/density,s.y/density};
}
void click(Ui& ui,const char* id,float density) {
    ui.update();const auto r=rect(ui,id,density);ui.context().ProcessMouseMove(static_cast<int>((r.x+r.w/2)*density),static_cast<int>((r.y+r.h/2)*density),0);
    ui.context().ProcessMouseButtonDown(0,0);ui.context().ProcessMouseButtonUp(0,0);ui.update();
}
Action single(Ui& ui,ActionKind expected) {
    auto actions=ui.take_actions();require(actions.size()==1,"Exactly one typed intention should leave one interaction");require(actions[0].kind==expected,"Unexpected application command");return actions[0];
}
void scenario(const std::filesystem::path& root,int width,int height,float density,Render& render) {
    Ui ui(root);std::string error;require(ui.initialize(static_cast<int>(width*density),static_cast<int>(height*density),density,error),"Native UI initialization failed");
    View view;view.creation.generation=17;view.creation.name="Welt";view.creation.earth_available=true;view.can_continue=false;ui.set_view(view);ui.update();
    for(const auto locale:{Locale::Chinese,Locale::English,Locale::German}) {
        view.locale=locale;ui.set_view(view);ui.update();
        const auto menu=rect(ui,"main-menu",density),heading=rect(ui,"main-title",density),first=rect(ui,"new-world",density),second=rect(ui,"continue-world",density),last=rect(ui,"exit",density);
        require(menu.x>=0 && menu.y>=0 && menu.x+menu.w<=width+.1 && menu.y+menu.h<=height+.1,"Arcade cabinet menu must fit all supported language/viewports");
        require(first.h>=56 && last.h>=56 && first.y>=heading.y+heading.h+16,"Arcade menu needs large filled controls and a separate title band");
        if(width<820) require(second.y>first.y && std::abs(second.x-first.x)<.15,"Narrow arcade menu must become an accessible single column");
        else require(std::abs(first.y-second.y)<.15 && second.x>first.x+first.w,"Wide arcade menu must use two button columns rather than the old website list");
        require(ui.context().GetDocument(0)->GetProperty<Rml::String>("font-family")=="Sonn Arcade","Every language must use the admitted arcade font family");
    }
    require(!ui.blocks_world_input(),"Ordinary menu/world surface is not a modal input barrier");
    click(ui,"continue-world",density);require(ui.take_actions().empty(),"Unavailable Continue must not issue a fake load");
    click(ui,"new-world",density);single(ui,ActionKind::NewWorld);
    view.screen=Screen::Creation;ui.set_view(view);ui.update();
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
        view.locale=locale;ui.set_view(view);ui.update();
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
        view.locale=locale;ui.set_view(view);ui.update();
        const int columns=width<760?4:8;
        const auto bar=rect(ui,"bottom-bar",density,Rml::Box::CONTENT);
        const float pixel_tolerance=1.1f/density;
        const auto outer_bar=rect(ui,"bottom-bar",density);
        require(std::abs(outer_bar.y+outer_bar.h-height)<pixel_tolerance,"Bottom section toolbar must remain anchored to the viewport edge through its absolute wrapper");
        for(int i=0;i<8;++i) {
            const auto button=rect(ui,tabs[i],density),row=rect(ui,tabs[(i/columns)*columns],density);
            require(std::abs(button.y-row.y)<.15,"Bottom sections must use exactly eight columns or four columns per row");
            require(button.x>=bar.x-pixel_tolerance && button.x+button.w<=bar.x+bar.w+pixel_tolerance && button.y+button.h<=height+pixel_tolerance,"Bottom button padding must not overflow the viewport or content row");
            require(std::abs(button.w-bar.w/columns)<pixel_tolerance,"Bottom percentage widths must include padding");
        }
        if(columns==4) require(rect(ui,"tab-construction",density).y>rect(ui,"tab-observe",density).y+.15,"Narrow bottom sections must form exactly two rows");
        require(ui.take_actions().empty(),"Layout/localization queries must not emit commands");
    }
    ui.activate("tab-life");require(ui.take_actions().empty(),"Unavailable domain tab must not report success");
    render.reset_draws();ui.render();require(render.pixel_draws>0 && render.light_panel,"Real RmlUi must render the registered stepped Light panel geometry");
    view.presentation_darkness=true;ui.set_view(view);ui.update();
    // Entering World focuses the selected Observe tab. Verify that palette state
    // separately from the ordinary selected fill; neither focus move is an action.
    ui.context().ProcessMouseMove(0,0,0);
    ui.context().GetDocument(0)->GetElementById("tab-observe")->Focus();
    ui.update();render.reset_draws();ui.render();
    require(render.dark_panel && render.dark_selection_focus && !render.light_panel,"Dark selected keyboard focus must render its visible amber highlight without retaining Light panels");
    ui.context().GetDocument(0)->GetElementById("camera-home")->Focus();
    ui.update();render.reset_draws();ui.render();
    require(render.dark_panel && render.dark_selection && !render.light_panel,"Authoritative presentation Age must switch actual native panel and selection geometry palettes");
    require(render.dark_selection_sprite,"Dark selected tab must submit a black-tinted textured sprite for contrast against amber fill");
    require(ui.context().GetDocument(0)->GetElementById("camera-home")->GetChild(0)->GetAttribute<Rml::String>("src","").find("-dark-96.png")!=std::string::npos,"Actual UI icons must follow the same authoritative Age");
    require(ui.take_actions().empty(),"Age presentation synchronization must not mutate World or issue commands");
    ui.activate("tab-world");single(ui,ActionKind::SelectBottomSection);
    ui.activate("speed-sixtyfour");require(single(ui,ActionKind::SetSpeed).a==64,"Only approved clock multipliers are exposed");
    ui.activate("age-darkness");require(single(ui,ActionKind::SetAgeDarkness).generation==44,"Age intent belongs to current session");
    ui.activate("world-info");single(ui,ActionKind::OpenWorldInspector);
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
    ui.activate("world-close");single(ui,ActionKind::CloseWorldInspector);
    require(!ui.blocks_world_input(),"Closing the world inspector must restore camera input");
    ui.activate("camera-help");single(ui,ActionKind::OpenHelp);require(ui.blocks_world_input(),"Camera help is a modal input barrier");
    ui.activate("help-close");single(ui,ActionKind::CloseHelp);require(!ui.blocks_world_input(),"Closing help releases camera input");
    ui.activate("tab-settings");single(ui,ActionKind::OpenSettings);ui.update();
    require(ui.blocks_world_input(),"Settings must block camera input even without text focus");
    for(const auto locale:{Locale::Chinese,Locale::English,Locale::German}) {
        view.locale=locale;ui.set_view(view);ui.update();
        const auto panel=rect(ui,"settings-panel",density);require(panel.x>=0 && panel.x+panel.w<=width+.1 && panel.y>=0 && panel.y+panel.h<=height+.1,"Native settings panel clips outside viewport");
        const auto volume=rect(ui,"audio-volume",density);require(volume.y>panel.y+100,"Settings business rows must be block layout, not one inline run");
        auto* close=ui.context().GetDocument(0)->GetElementById("settings-close");close->ScrollIntoView(false);ui.update();
        const auto close_rect=rect(ui,"settings-close",density);
        require(close_rect.y>=panel.y-.15 && close_rect.y+close_rect.h<=panel.y+panel.h+.15,"The last control of long localized settings must be reachable through real native scrolling");
        if(width<820) require(ui.context().GetDocument(0)->GetElementById("settings-panel")->GetScrollTop()>0,"Narrow arcade settings must genuinely scroll, not silently clip their final actions");
    }
    ui.context().ProcessKeyDown(Rml::Input::KI_ESCAPE,0);single(ui,ActionKind::CloseSettings);
    require(!ui.blocks_world_input(),"Escape from settings releases the modal input barrier");
    view.screen=Screen::Load;view.saves={{"save1","Original world",""}};view.selected_save="save1";ui.set_view(view);ui.update();ui.activate("load-selected");require(single(ui,ActionKind::LoadSelected).text=="save1","Load must reference selected real save");
    const auto save=rect(ui,"save-entry-0",density),save_content=rect(ui,"save-panel",density,Rml::Box::CONTENT);
    require(save.x>=save_content.x-.15 && save.x+save.w<=save_content.x+save_content.w+.15,"Save entry percentage width must include its horizontal padding");
    ui.activate("back-load");single(ui,ActionKind::CancelLoad);
    view.error_key="CREATION_DIMENSIONS";view.error_detail="core width 32..1008";
    for(const auto locale:{Locale::Chinese,Locale::English,Locale::German}) {
        view.locale=locale;ui.set_view(view);ui.update();
        require(ui.blocks_world_input(),"Business errors must block world camera input");
        auto* document=ui.context().GetDocument(0);
        require(document->GetElementById("error-message")->GetInnerRML().find("32–1008")!=std::string::npos,"Creation dimensions must have a localized business reason in all three languages");
        require(document->GetElementById("error-detail")->GetComputedValues().display()==Rml::Style::Display::None,"Raw technical details must remain hidden by default");
    }
    ui.activate("error-details");ui.update();
    require(ui.context().GetDocument(0)->GetElementById("error-detail")->GetComputedValues().display()!=Rml::Style::Display::None && ui.take_actions().empty(),"Technical details expand locally on demand without a World command");
    view.error_detail="derived height 32..1008";ui.set_view(view);ui.update();
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
        for(float density:{1.f,2.f}) {scenario(root,1280,800,density,render);scenario(root,480,640,density,render);}
    } catch(const std::exception& failure) {std::cerr<<failure.what()<<'\n';result=1;}
    // The memory font alias explicitly borrows these bytes until Shutdown.
    Rml::Shutdown();return result;
}
