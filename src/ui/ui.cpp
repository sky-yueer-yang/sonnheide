#include "ui.hpp"
#include "pixel_decorator.hpp"
#include <RmlUi/Core.h>
#include <RmlUi/Core/Context.h>
#include <RmlUi/Core/Element.h>
#include <RmlUi/Core/ElementDocument.h>
#include <RmlUi/Core/Event.h>
#include <RmlUi/Core/FontEngineInterface.h>
#include <RmlUi/Core/Input.h>
#include <RmlUi/Core/Elements/ElementFormControl.h>
#include <RmlUi/Core/Elements/ElementFormControlSelect.h>
#include <algorithm>
#include <cmath>
#include <fstream>
#include <limits>
#include <sstream>
#include <stdexcept>

namespace sonnheide::ui {
namespace {
std::string utf8_path(const std::filesystem::path& path) { const auto u=path.u8string(); return {u.begin(),u.end()}; }
std::string escape(std::string_view value) {
    std::string result;
    for (const auto c:value) { if (c=='&') result+="&amp;"; else if(c=='<') result+="&lt;"; else if(c=='>') result+="&gt;"; else if(c=='\"') result+="&quot;"; else result+=c; }
    return result;
}
void show(Rml::ElementDocument* doc,const char* id,bool visible,const char* display="block") { if(auto* el=doc->GetElementById(id)) el->SetProperty("display",visible?display:"none"); }
void label(Rml::ElementDocument* doc,const char* id,const std::string& value) { if(auto* el=doc->GetElementById(id)) { const auto markup=escape(value); if(el->GetInnerRML()!=markup) el->SetInnerRML(markup); } }
void choice(Rml::ElementDocument* doc,const char* id,bool active) { if(auto* el=doc->GetElementById(id)) el->SetClass("selected",active); }
void enable(Rml::ElementDocument* doc,const char* id,bool enabled) {
    if(auto* el=doc->GetElementById(id)) {
        el->SetPseudoClass("disabled",!enabled); el->SetProperty("focus",enabled?"auto":"none"); el->SetProperty("tab-index",enabled?"auto":"none");
        if(enabled) el->RemoveAttribute("disabled"); else el->SetAttribute("disabled",true);
    }
}
void input_value(Rml::Context* context,Rml::ElementDocument* doc,const char* id,const std::string& value) {
    if(auto* input=dynamic_cast<Rml::ElementFormControl*>(doc->GetElementById(id))) {
        if(context->GetFocusElement()!=input && input->GetValue()!=value) input->SetValue(value);
    }
}
bool parse_number(std::string value,double& result) {
    if(value.empty() || value.size()>32) return false;
    std::replace(value.begin(),value.end(),',','.');
    std::istringstream input(value); input.imbue(std::locale::classic());
    if(!(input>>result)) return false;
    char extra=0; return !(input>>extra) && std::isfinite(result);
}
std::string input_text(Rml::ElementDocument* doc,const char* id) {
    if(auto* input=dynamic_cast<Rml::ElementFormControl*>(doc->GetElementById(id))) return input->GetValue();
    return {};
}
Rml::FontEngineInterface& admitted_font_engine() {
    auto* engine=Rml::GetFontEngineInterface();
    if(!engine) throw std::runtime_error("UI_FONT_ENGINE: native font engine is unavailable");
    return *engine;
}
Rml::FontFaceHandle admitted_arcade_face(Rml::FontEngineInterface& engine,int pixels) {
    // The locked native FontProvider explicitly requires a lowercase family
    // for direct queries. RCSS normalizes its own names, this API does not.
    const auto face=engine.GetFontFaceHandle("sonn arcade",Rml::Style::FontStyle::Normal,Rml::Style::FontWeight::Normal,pixels);
    if(!face) throw std::runtime_error("UI_FONT_MISSING: admitted Sonn Arcade face is unavailable at "+std::to_string(pixels)+"px");
    return face;
}
}
const char* locale_code(Locale locale) { return locale==Locale::English?"en":locale==Locale::German?"de":"zh-CN"; }
Ui::Ui(std::filesystem::path resources):resources_(std::move(resources)) {}
Ui::~Ui() {
    if(document_) { for(const auto* event:{"click","change","input"}) document_->RemoveEventListener(event,this); document_->RemoveEventListener("keydown",this,true); document_->Close(); }
    if(context_) Rml::RemoveContext("sonnheide-ui");
}
bool Ui::initialize(int width,int height,float density,std::string& error) {
    try {
        std::ifstream input(resources_/"ui/locales.tsv");
        if(!input) throw std::runtime_error("Cannot read interface language package");
        std::string line; std::getline(input,line);
        while(std::getline(input,line)) {
            if(!line.empty() && line.back()=='\r') line.pop_back();
            if(line.empty()) continue;
            std::array<std::string,4> fields; std::size_t start=0;
            for(int i=0;i<4;++i) { const auto end=line.find('\t',start); fields[i]=line.substr(start,end==std::string::npos?end:end-start); if(i<3 && end==std::string::npos) throw std::runtime_error("Incomplete interface translation row"); start=end+1; }
            if(!messages_.emplace(fields[0],std::array<std::string,3>{fields[1],fields[2],fields[3]}).second) throw std::runtime_error("Duplicate interface translation key");
        }
        register_pixel_decorator();
        context_=Rml::CreateContext("sonnheide-ui",{width,height});
        if(!context_) throw std::runtime_error("Cannot create native UI context");
        context_->SetDensityIndependentPixelRatio(std::max(.1f,density));
        document_=context_->LoadDocument(utf8_path(resources_/"ui/application.rml"));
        if(!document_) throw std::runtime_error("Cannot load native interface document");
        for(const auto* event:{"click","change","input"}) document_->AddEventListener(event,this);
        document_->AddEventListener("keydown",this,true);
        refresh(); layout(); document_->Show(); context_->Update();
        if(auto* el=document_->GetElementById("new-world")) el->Focus();
        error.clear(); return true;
    } catch(const std::exception& failure) { error=failure.what(); return false; }
}
void Ui::resize(int width,int height,float density) { if(!context_) return; context_->SetDimensions({width,height}); context_->SetDensityIndependentPixelRatio(std::max(.1f,density)); layout(); }
void Ui::set_view(const View& view) {
    if(view_==view) return;
    const bool changed_screen=view_.screen!=view.screen;
    const bool opened_error=view_.error_key.empty() && !view.error_key.empty();
    const bool closed_error=!view_.error_key.empty() && view.error_key.empty();
    const bool changed_session=view_.world.session!=view.world.session;
    if(view_.error_key!=view.error_key || view_.error_detail!=view.error_detail) error_details_open_=false;
    if(view.screen==Screen::Creation && !view_.creation.preview_ready && view.creation.preview_ready) {
        controls_open_=false;pending_focus_="create";
        if(document_) document_->GetElementById("creation-controls")->SetScrollTop(0);
    }
    if(view.screen==Screen::Creation && view_.creation.preview_ready && !view.creation.preview_ready) controls_open_=true;
    view_=view;
    if(creation_generation_!=view.creation.generation) { creation_generation_=view.creation.generation; creation_name_=view.creation.name; }
    if(rename_session_!=view.world.session) { rename_session_=view.world.session; rename_name_=view.world.name; world_open_=false; }
    if(changed_screen) { settings_open_=false; world_open_=false; help_open_=false; if(view.screen==Screen::Creation) controls_open_=true; }
    if(changed_session) section_=BottomSection::Observe;
    if(changed_screen) pending_focus_=view.screen==Screen::MainMenu?"new-world":view.screen==Screen::Creation?"creation-name":view.screen==Screen::Load?"back-load":"tab-observe";
    if(opened_error) pending_focus_="error-dismiss";
    if(closed_error) focus_underlying_controls();
    refresh(); layout();
}
void Ui::focus_underlying_controls() {
    if(!view_.error_key.empty() || !local_error_.empty()) pending_focus_="error-dismiss";
    else if(settings_open_) pending_focus_="locale-zh";
    else if(world_open_ && view_.screen==Screen::World) pending_focus_="world-name-draft";
    else if(help_open_) pending_focus_="help-close";
    else if(view_.screen==Screen::Creation) pending_focus_=controls_open_ || !view_.creation.preview_ready?"creation-width":"create";
    else if(view_.screen==Screen::World) pending_focus_="tab-observe";
    else if(view_.screen==Screen::Load) pending_focus_="back-load";
    else pending_focus_="new-world";
}
std::string Ui::text(const std::string& key) const { const auto it=messages_.find(key); return it==messages_.end()?key:it->second[static_cast<std::size_t>(view_.locale)]; }
void Ui::refresh() {
    if(!document_) return;
    // RmlUi select SetValue dispatches change even for a programmatic update.
    // Snapshot/localization synchronization must never become a player intent.
    struct SyncGuard {
        bool& flag; bool previous;
        explicit SyncGuard(bool& value):flag(value),previous(value) { flag=true; }
        ~SyncGuard() { flag=previous; }
    } guard(syncing_controls_);
    document_->SetClass("age-dark",view_.presentation_darkness);
    document_->SetClass("age-light",!view_.presentation_darkness);
    document_->SetProperty("font-family","Sonn Arcade");
    const auto translate=[this](auto&& self,Rml::Element* element)->void {
        const auto key=element->GetAttribute<Rml::String>("data-key","");
        if(!key.empty()) { const auto markup=escape(text(key)); if(element->GetInnerRML()!=markup) element->SetInnerRML(markup); }
        // Select options live under a non-DOM selectbox in the locked RmlUi.
        if(auto* select=dynamic_cast<Rml::ElementFormControlSelect*>(element)) {
            bool changed=false;
            for(int i=0;i<select->GetNumOptions();++i) {
                auto* option=select->GetOption(i);
                const auto option_key=option->GetAttribute<Rml::String>("data-key","");
                changed=changed || (!option_key.empty() && option->GetInnerRML()!=escape(text(option_key)));
                self(self,option);
            }
            // Updating option markup alone does not invalidate selectvalue.
            // Preserve the current selection while refreshing its visible label.
            if(changed) select->SetValue(select->GetValue());
        }
        for(int i=0;i<element->GetNumChildren();++i) self(self,element->GetChild(i));
    };
    translate(translate,document_);
    // The static multicolour wordmark is authored RML, not a translated field.
    show(document_,"main-screen",view_.screen==Screen::MainMenu);
    show(document_,"creation-screen",view_.screen==Screen::Creation);
    show(document_,"world-screen",view_.screen==Screen::World);
    show(document_,"load-screen",view_.screen==Screen::Load);
    show(document_,"settings-overlay",settings_open_);
    show(document_,"help-overlay",help_open_);
    show(document_,"world-overlay",world_open_ && view_.screen==Screen::World);
    show(document_,"error-overlay",!view_.error_key.empty() || !local_error_.empty());
    show(document_,"busy-indicator",view_.busy || view_.creation.busy);
    label(document_,"busy-label",text(view_.status_key.empty()?"working":view_.status_key));
    enable(document_,"continue-world",view_.can_continue && !view_.busy);
    for(const auto* id:{"new-world","load","exit","return-menu","world-load","save","load-selected"}) enable(document_,id,!view_.busy);
    enable(document_,"load-selected",!view_.selected_save.empty() && !view_.busy);
    const bool themes=view_.creation.can_select_theme && view_.creation.base==BlankBase::Soil;
    show(document_,"creation-theme-label",themes);
    show(document_,"creation-theme",themes);
    show(document_,"preview-caption",view_.creation.preview_ready);
    choice(document_,"blank-soil",view_.creation.base==BlankBase::Soil); choice(document_,"blank-ocean",view_.creation.base==BlankBase::Ocean);
    enable(document_,"preview",!view_.creation.busy);
    enable(document_,"create",view_.creation.preview_ready && !view_.creation.busy);
    input_value(context_,document_,"creation-name",creation_name_);
    input_value(context_,document_,"creation-width",std::to_string(view_.creation.width));
    input_value(context_,document_,"creation-height",std::to_string(view_.creation.height));
    enable(document_,"creation-height",!view_.creation.busy);
    enable(document_,"creation-width",!view_.creation.busy);
    for(int size:{256,512,1024,2048,4096}) {
        const auto id="size-"+std::to_string(size);
        choice(document_,id.c_str(),view_.creation.width==size && view_.creation.height==size);
        enable(document_,id.c_str(),!view_.creation.busy);
    }
    update_creation_scale();
    input_value(context_,document_,"creation-theme",view_.creation.theme);
    label(document_,"world-name",view_.world.name);
    label(document_,"world-time",text("day")+" "+std::to_string(view_.world.day));
    label(document_,"pause-label",text(view_.world.paused?"play":"pause"));
    if(auto* icon=document_->GetElementById("pause-icon")) icon->SetAttribute("data-icon",view_.world.paused?"play":"pause");
    choice(document_,"pause",view_.world.paused);
    choice(document_,"speed-one",view_.world.speed==1); choice(document_,"speed-four",view_.world.speed==4); choice(document_,"speed-sixteen",view_.world.speed==16); choice(document_,"speed-sixtyfour",view_.world.speed==64);
    choice(document_,"age-light",!view_.world.darkness); choice(document_,"age-darkness",view_.world.darkness);
    choice(document_,"age-auto",view_.world.automatic_age); choice(document_,"age-manual",!view_.world.automatic_age);
    choice(document_,"locale-zh",view_.locale==Locale::Chinese); choice(document_,"locale-en",view_.locale==Locale::English); choice(document_,"locale-de",view_.locale==Locale::German);
    choice(document_,"fullscreen",view_.fullscreen); choice(document_,"windowed",!view_.fullscreen);
    choice(document_,"motion-normal",!view_.reduced_motion); choice(document_,"motion-reduced",view_.reduced_motion);
    input_value(context_,document_,"audio-volume",std::to_string(view_.audio_volume));
    const char* tabs[]={"tab-observe","tab-terrain","tab-life","tab-civilization","tab-construction","tab-economy","tab-world","tab-settings"};
    for(int i=0;i<8;++i) { choice(document_,tabs[i],i==static_cast<int>(section_)); enable(document_,tabs[i],true); }
    show(document_,"observe-tools",section_==BottomSection::Observe,"flex"); show(document_,"world-tools",section_==BottomSection::World,"flex"); show(document_,"world-age-group",section_==BottomSection::World); show(document_,"age-tools",section_==BottomSection::World,"flex");
    const char* future_panels[]={"terrain-tools","life-tools","civilization-tools","construction-tools","economy-tools"};
    for(int i=0;i<5;++i) show(document_,future_panels[i],static_cast<int>(section_)==i+1,"flex");
    show(document_,"settings-tools",section_==BottomSection::Settings,"flex");
    if(auto* pause=document_->GetElementById("pause")) pause->SetAttribute("data-tooltip-key",view_.world.paused?"play":"pause");
    show(document_,"no-saves",view_.saves.empty());
    std::string signature;
    for(const auto& save:view_.saves) signature+=save.id+'\n'+save.name+'\n'+save.detail+'\n';
    if(signature!=loaded_saves_signature_) {
        loaded_saves_signature_=signature; std::string content;
        for(std::size_t i=0;i<view_.saves.size();++i) { const auto& save=view_.saves[i]; content+="<button id=\"save-entry-"+std::to_string(i)+"\" class=\"save-entry\"><span>"+escape(save.name)+"</span>"; if(!save.detail.empty()) content+="<span class=\"save-detail\">"+escape(save.detail)+"</span>"; content+="</button>"; }
        document_->GetElementById("save-list")->SetInnerRML(content);
    }
    for(std::size_t i=0;i<view_.saves.size();++i) if(auto* el=document_->GetElementById("save-entry-"+std::to_string(i))) el->SetClass("selected",view_.saves[i].id==view_.selected_save);
    label(document_,"world-facts",std::to_string(view_.world.cells)+" "+text("cells")+" · "+std::to_string(view_.world.dry_cells)+" "+text("dry-cells"));
    input_value(context_,document_,"world-name-draft",rename_name_);
    label(document_,"rename-consequences",view_.world.rename_preview_ready?text("name-change")+" · "+view_.world.name+" → "+rename_name_:"");
    enable(document_,"rename-commit",view_.world.rename_preview_ready && !view_.busy);
    std::string error_key=view_.error_key.empty()?local_error_:view_.error_key;
    if(messages_.find(error_key)==messages_.end()) {
        if(error_key.find("STALE")!=std::string::npos || error_key.find("GENERATION")!=std::string::npos || error_key.find("REVISION")!=std::string::npos) error_key="error-stale";
        else if(error_key.find("DURABILITY")!=std::string::npos) error_key="error-durability";
        else if(error_key.find("SAVE")!=std::string::npos || error_key.find("WRITE")!=std::string::npos || error_key.find("FLUSH")!=std::string::npos) error_key="error-save";
        else if(error_key.find("NAME")!=std::string::npos || error_key=="INVALID_UTF8" || error_key=="NON_NFC") error_key="error-name";
        else if(error_key=="CREATION_DIMENSIONS") error_key="error-dimensions";
        else if(error_key.find("BUDGET")!=std::string::npos || error_key.find("SIZE")!=std::string::npos) error_key="error-size";
        else if(error_key.find("CHECKPOINT")!=std::string::npos || error_key.find("CONTENT")!=std::string::npos || error_key.find("FORMAT")!=std::string::npos) error_key="error-checkpoint";
        else error_key="error-generic";
    }
    label(document_,"error-message",text(error_key));
    label(document_,"error-detail",view_.error_key+(view_.error_detail.empty()?"":" · "+view_.error_detail));
    label(document_,"error-details",text(error_details_open_?"hide-details":"technical-details"));
    show(document_,"error-details",!view_.error_detail.empty(),"inline-block");
    show(document_,"error-detail",error_details_open_ && !view_.error_detail.empty());
    const auto icons=[](auto&& self,Rml::Element* element)->void {
        if(element->GetTagName()=="img") {
            auto icon=element->GetAttribute<Rml::String>("data-icon","");
            if(icon.empty()) {
                const auto src=element->GetAttribute<Rml::String>("src","");
                const auto slash=src.find_last_of('/');
                if(src.size()>=7 && src.substr(src.size()-7)=="-96.png") icon=src.substr(slash==std::string::npos?0:slash+1,src.size()-7-(slash==std::string::npos?0:slash+1));
                if(!icon.empty()) element->SetAttribute("data-icon",icon);
            }
            if(!icon.empty()) {
                const std::string src="../generated/ui/icons/"+icon+"-96.png";
                if(element->GetAttribute<Rml::String>("src","")!=src) element->SetAttribute("src",src);
            }
        }
        for(int i=0;i<element->GetNumChildren();++i) self(self,element->GetChild(i));
    };
    icons(icons,document_);
}
void Ui::layout() {
    if(!document_ || !context_) return;
    const float density=std::max(.1f,context_->GetDensityIndependentPixelRatio()); const auto physical=context_->GetDimensions();
    const float w=physical.x/density,h=physical.y/density;
    const auto property=[this](const char* id,const char* key,float value) { if(auto* el=document_->GetElementById(id)) el->SetProperty(key,std::to_string(value)+"dp"); };
    const auto grid=[](float value){return std::floor(value/8)*8;};
    const bool narrow=w<820;
    document_->SetClass("compact",narrow);
    // RmlUi5's font-effect parser passes length values to the instancer without
    // resolving dp. Resolve our logical widths here into physical whole pixels,
    // including every heading and the wordmark's inheriting inline spans.
    if(outline_density_!=density || outline_compact_!=narrow) {
        const auto apply=[&](auto&& self,Rml::Element* element,bool topbar)->void {
            topbar=topbar || element->IsClassSet("topbar");
            const bool title=element->GetId()=="main-title";
            if(title || element->GetTagName()=="h2" || element->GetId()=="world-name") {
                if(topbar && narrow) element->SetProperty("font-effect","none");
                else {
                    const int outer=std::clamp(static_cast<int>(std::round(2.f*density)),1,16);
                    const int pigment=std::clamp(static_cast<int>(std::round(density)),1,16);
                    const std::string colour=element->IsClassSet("danger-heading")?"#c83a29":"#d99100";
                    element->SetProperty("font-effect","pixel-outline("+std::to_string(outer)+"px #ffffff), "+std::string(title?"pixel-outline":"pixel-bold")+"("+std::to_string(pigment)+"px "+colour+")");
                }
            }
            for(int i=0;i<element->GetNumChildren();++i) self(self,element->GetChild(i),topbar);
        };
        apply(apply,document_,false);
        outline_density_=density;outline_compact_=narrow;
    }

    // Size the physical menu ledger from actual admitted glyph advances.  The
    // actions use the true doubled pixel face, rather than leaving small words
    // floating inside an arbitrarily wide plaque.
    auto& font_engine=admitted_font_engine();
    const auto action_face=admitted_arcade_face(font_engine,static_cast<int>(std::round(24.f*density)));
    const auto title_face=admitted_arcade_face(font_engine,static_cast<int>(std::round(36.f*density)));
    const auto action_width=[&](const char* key) { return font_engine.GetStringWidth(action_face,text(key))/density; };
    float menu_need=font_engine.GetStringWidth(title_face,"SONNHEIDE")/density+38.f;
    for(const char* key:{"new","continue","load","settings","exit"}) menu_need=std::max(menu_need,action_width(key)+116.f);
    const float menu_width=std::min(std::ceil(std::max(304.f,menu_need)/8.f)*8.f,w-32.f);
    property("main-menu","width",menu_width);
    property("main-menu","left",grid((w-menu_width)/2));
    property("main-menu","top",grid(std::max(16.f,(h-432.f)/2)));
    property("main-menu","max-height",std::max(160.f,h-32));
    const bool editing=controls_open_ || !view_.creation.preview_ready;
    show(document_,"creation-topbar",!editing);
    show(document_,"creation-form",editing);
    show(document_,"creation-heading",editing,"flex");
    show(document_,"preview",editing,"inline-block");
    document_->GetElementById("creation-controls")->SetClass("preview-tray",!editing);
    const float tray_width=std::clamp(std::ceil((action_width("create")+100.f)/8.f)*8.f,208.f,480.f);
    const float creation_width=std::min(editing?608.f:tray_width,w-32.f);
    const float half_action_width=(creation_width-32.f)/2.f-12.f;
    document_->SetClass("stacked-actions",narrow || std::max(action_width("preview"),action_width("create"))+56.f>half_action_width);
    const float creation_height=std::min(editing?704.f:96.f,h-32.f);
    property("creation-controls","width",creation_width);
    property("creation-controls","height",creation_height);
    property("creation-controls","left",std::round((w-creation_width)/2));
    property("creation-controls","top",editing?std::round((h-creation_height)/2):h-creation_height-16.f);
    property("scene-viewport","left",0);property("scene-viewport","top",0);
    property("scene-viewport","width",w);property("scene-viewport","height",h);
    property("save-panel","left",grid(std::max(16.f,(w-std::min(800.f,w-32))/2)));
    property("save-panel","width",grid(std::min(800.f,w-32)));
    property("save-panel","top",56);property("save-panel","height",grid(std::max(128.f,h-72)));
    for(const auto* id:{"settings-panel","world-panel","help-panel","error-panel"}) {
        const float width=grid(std::min(536.f,w-24));
        property(id,"left",grid((w-width)/2));property(id,"width",width);
        const float top=grid(std::max(16.f,std::min(64.f,h*.08f)));
        property(id,"top",top);property(id,"max-height",grid(std::max(128.f,h-top-24)));
        property(id,"padding-left",16);property(id,"padding-right",16);
    }
    const float tab_width=std::floor(std::min(72.f,(w-16.f)/8.f));
    if(auto* bar=document_->GetElementById("bottom-bar")) {
        for(int i=0;i<bar->GetNumChildren();++i) if(auto* button=bar->GetChild(i)) {
            button->SetProperty("width",std::to_string(tab_width)+"dp");
        }
    }
    // The six-pixel join faces occupy a genuine gap in the panel top ring.
    // Neither a later panel draw nor alpha overdraw may close the tab necks.
    document_->GetElementById("bottom-bar-frame")->SetProperty("decorator","physical-material(marble #ffffffff 3dp 2dp 3dp 8dp "+std::to_string(tab_width*8.f)+"dp)");
    // Three physical instrument groups share one row where they actually fit.
    // At small widths the flex wrapper uses two real rows rather than reserving
    // a permanently oversized empty toolbar on every screen.
    const float tool_height=section_==BottomSection::World?(w-24.f>=768.f?72.f:136.f):72.f;
    property("section-tools","min-height",tool_height);
    property("section-tools","max-height",std::max(tool_height,std::min(208.f,h*.45f)));
    property("preview-caption","left",16);property("preview-caption","max-width",w-32.f);

}
void Ui::update_creation_scale() {
    const auto metres=[](std::int64_t mm){std::string value=std::to_string(mm/1000);auto fraction=std::to_string(1000+mm%1000).substr(1);while(!fraction.empty()&&fraction.back()=='0')fraction.pop_back();return fraction.empty()?value:value+"."+fraction;};
    const auto distance=[&](std::int64_t mm){return mm>=1000000?metres(mm/1000)+" km":metres(mm)+" m";};
    label(document_,"creation-scale",distance(static_cast<std::int64_t>(view_.creation.width)*view_.creation.cell_mm)+" × "+distance(static_cast<std::int64_t>(view_.creation.height)*view_.creation.cell_mm)+" · "+text("fine-surface"));
}
void Ui::update() {
    if(!context_) return;
    context_->Update();
    // RmlUi has no automatic browser-style title tooltip. Render the real
    // localised full name on actual pointer hover, without emitting an intent.
    Rml::Element* hovered=context_->GetHoverElement();
    if(blocks_world_input()) {
        const char* active=!view_.error_key.empty() || !local_error_.empty()?"error-panel":settings_open_?"settings-panel":world_open_?"world-panel":help_open_?"help-panel":"creation-controls";
        auto* owner=hovered;
        while(owner && owner!=document_ && owner->GetId()!=active) owner=owner->GetParentNode();
        if(!owner || owner==document_) hovered=nullptr;
    }
    while(hovered && hovered!=document_ && hovered->GetTagName()!="button") hovered=hovered->GetParentNode();
    const std::string tooltip_key=hovered && hovered!=document_?hovered->GetAttribute<Rml::String>("data-tooltip-key",""):"";
    const bool tooltip_was_visible=document_->GetElementById("tool-tooltip")->GetComputedValues().display()!=Rml::Style::Display::None;
    show(document_,"tool-tooltip",!tooltip_key.empty());
    if(tooltip_key.empty() && tooltip_was_visible) context_->Update();
    if(!tooltip_key.empty()) {
        auto* tooltip=document_->GetElementById("tool-tooltip");
        label(document_,"tool-tooltip",text(tooltip_key)+(hovered->IsPseudoClassSet("disabled")?" · "+text("not-available"):""));
        const float density=context_->GetDensityIndependentPixelRatio();
        const auto dimensions=context_->GetDimensions();
        const float w=dimensions.x/density,h=dimensions.y/density;
        const auto offset=hovered->GetAbsoluteOffset(Rml::Box::BORDER),size=hovered->GetBox().GetSize(Rml::Box::BORDER);
        auto& font_engine=admitted_font_engine();
        const auto tooltip_face=admitted_arcade_face(font_engine,static_cast<int>(std::round(12.f*density)));
        const auto tooltip_text=text(tooltip_key)+(hovered->IsPseudoClassSet("disabled")?" · "+text("not-available"):"");
        const float content_width=font_engine.GetStringWidth(tooltip_face,tooltip_text)/density;
        const float width=std::min(std::min(272.f,w-24.f),std::max(56.f,std::ceil(content_width+24.f)));
        tooltip->SetProperty("width",std::to_string(width)+"dp");
        tooltip->SetProperty("left",std::to_string(std::clamp((offset.x+size.x/2)/density-width/2,12.f,std::max(12.f,w-width-12)))+"dp");
        context_->Update();
        const float height=tooltip->GetBox().GetSize(Rml::Box::BORDER).y/density;
        tooltip->SetProperty("top",std::to_string(std::clamp(offset.y/density-height-8,8.f,std::max(8.f,h-height-8)))+"dp");
        context_->Update();
    }
    if(!pending_focus_.empty()) {
        if(auto* el=document_->GetElementById(pending_focus_)) {
            el->Focus();
            el->ScrollIntoView(false);
            context_->Update();
        }
        pending_focus_.clear();
    }
}
void Ui::render() { if(context_) context_->Render(); }
Rml::Context& Ui::context() { if(!context_) throw std::logic_error("UI context is not initialized"); return *context_; }
std::vector<Action> Ui::take_actions() { auto actions=std::move(actions_); actions_.clear(); return actions; }
Viewport Ui::viewport() const {
    if(!document_ || !context_) return {};
    const float density=context_->GetDensityIndependentPixelRatio();
    if(view_.screen==Screen::Creation) {
        auto* el=document_->GetElementById("scene-viewport"); const auto offset=el->GetAbsoluteOffset(Rml::Box::BORDER),size=el->GetBox().GetSize(Rml::Box::BORDER);
        return {offset.x/density,offset.y/density,size.x/density,size.y/density};
    }
    const auto size=context_->GetDimensions(); return {0,0,size.x/density,size.y/density};
}
bool Ui::captures_pointer(int x,int y) const {
    if(!context_) return false;
    auto* target=context_->GetElementAtPoint({static_cast<float>(x),static_cast<float>(y)});
    while(target && target!=document_) {
        const auto id=target->GetId();
        if(id=="scene-viewport" || id=="world-screen" || id=="creation-screen" || id=="main-screen") return false;
        if(target->GetTagName()=="button" || target->GetTagName()=="input" || target->GetTagName()=="select" || target->IsClassSet("panel") || target->IsClassSet("overlay") || id=="bottom-bar" || id=="topbar") return true;
        target=target->GetParentNode();
    }
    return false;
}
bool Ui::text_input_focused() const { if(!context_) return false; const auto* focus=context_->GetFocusElement(); return focus && (focus->GetTagName()=="input" || focus->GetTagName()=="textarea"); }
bool Ui::blocks_world_input() const { return (view_.screen==Screen::Creation && (controls_open_ || !view_.creation.preview_ready)) || settings_open_ || world_open_ || help_open_ || !view_.error_key.empty() || !local_error_.empty(); }
void Ui::emit(ActionKind kind,const std::string& value,double a,double b,double c,double d) {
    Action action; action.kind=kind; action.text=value; action.a=a; action.b=b; action.c=c; action.d=d;
    const bool creation=kind==ActionKind::SetBlankBase || kind==ActionKind::SetCreationSize || kind==ActionKind::SetCreationName || kind==ActionKind::SetCreationTheme || kind==ActionKind::RequestPreview || kind==ActionKind::CreateWorld || kind==ActionKind::CancelCreation;
    action.generation=creation?view_.creation.generation:view_.world.session;
    if(kind==ActionKind::CreateWorld) action.token=view_.creation.preview_token;
    if(kind==ActionKind::CommitWorldName) action.token=view_.world.rename_preview_token;
    actions_.push_back(std::move(action));
}
void Ui::activate(const std::string& id) {
    if(!document_) return;
    if(id=="continue" && !view_.can_continue) return;
    if(auto* element=document_->GetElementById(id)) if(element->IsPseudoClassSet("disabled")) return;
    if(id=="new" || id=="new-world") emit(ActionKind::NewWorld);
    else if(id=="continue" || id=="continue-world") emit(ActionKind::ContinueWorld);
    else if(id=="load" || id=="world-load") emit(ActionKind::OpenLoad);
    else if(id=="exit") emit(ActionKind::Exit);
    else if(id=="return-menu") emit(ActionKind::ReturnMenu);
    else if(id=="back-load") emit(ActionKind::CancelLoad);
    else if(id=="settings-main" || id=="settings-open") { settings_open_=true; pending_focus_="locale-zh"; emit(ActionKind::OpenSettings); }
    else if(id=="settings-close" || id=="settings-dismiss") { settings_open_=false; pending_focus_=view_.screen==Screen::MainMenu?"settings-main":"tab-settings"; emit(ActionKind::CloseSettings); }
    else if(id=="cancel-create" || id=="creation-close") emit(ActionKind::CancelCreation);
    else if(id=="creation-options") { controls_open_=!controls_open_;pending_focus_=controls_open_?"creation-name":"create";document_->GetElementById("creation-controls")->SetScrollTop(0); }
    else if(id=="blank-soil") emit(ActionKind::SetBlankBase,"soil",static_cast<double>(BlankBase::Soil));
    else if(id=="blank-ocean") emit(ActionKind::SetBlankBase,"ocean",static_cast<double>(BlankBase::Ocean));
    else if(id.rfind("size-",0)==0) {
        for(int size:{256,512,1024,2048,4096}) if(id=="size-"+std::to_string(size)) emit(ActionKind::SetCreationSize,"",size,size);
    }
    else if(id=="preview") { if(!valid_creation_inputs()) { local_error_="error-input"; pending_focus_="error-dismiss"; } else emit(ActionKind::RequestPreview); }
    else if(id=="create") { if(!valid_creation_inputs()) { local_error_="error-input"; pending_focus_="error-dismiss"; } else emit(ActionKind::CreateWorld); }
    else if(id=="locale-zh" || id=="locale-en" || id=="locale-de") emit(ActionKind::SetLocale,id=="locale-zh"?"zh-CN":id=="locale-en"?"en":"de");
    else if(id=="fullscreen" || id=="windowed") emit(ActionKind::SetFullscreen,"",id=="fullscreen"?1:0);
    else if(id=="motion-normal" || id=="motion-reduced") emit(ActionKind::SetReducedMotion,"",id=="motion-reduced"?1:0);
    else if(id=="volume-down" || id=="volume-up") emit(ActionKind::SetAudioVolume,"",std::clamp(view_.audio_volume+(id=="volume-up"?5:-5),0,100));
    else if(id=="camera-home") emit(ActionKind::CameraHome);
    else if(id=="camera-help") { help_open_=true; pending_focus_="help-close"; emit(ActionKind::OpenHelp); }
    else if(id=="help-close" || id=="help-dismiss") { help_open_=false; pending_focus_="camera-help"; emit(ActionKind::CloseHelp); }
    else if(id=="pause") emit(ActionKind::SetPaused,"",!view_.world.paused);
    else if(id=="speed-one" || id=="speed-four" || id=="speed-sixteen" || id=="speed-sixtyfour") emit(ActionKind::SetSpeed,"",id=="speed-one"?1:id=="speed-four"?4:id=="speed-sixteen"?16:64);
    else if(id=="age-light") emit(ActionKind::SetAgeLight);
    else if(id=="age-darkness") emit(ActionKind::SetAgeDarkness);
    else if(id=="age-auto" || id=="age-manual") emit(ActionKind::SetAgeAutomatic,"",id=="age-auto"?1:0);
    else if(id=="save") emit(ActionKind::SaveWorld);
    else if(id=="world-info") { world_open_=true; pending_focus_="world-name-draft"; emit(ActionKind::OpenWorldInspector); }
    else if(id=="world-close") { world_open_=false; pending_focus_="world-info"; emit(ActionKind::CloseWorldInspector); }
    else if(id=="rename-preview") emit(ActionKind::PreviewWorldName,rename_name_);
    else if(id=="rename-commit") emit(ActionKind::CommitWorldName,rename_name_);
    else if(id=="rename-cancel") { rename_name_=view_.world.name; emit(ActionKind::CancelWorldName); }
    else if(id=="error-details") error_details_open_=!error_details_open_;
    else if(id=="error-dismiss") { local_error_.clear(); error_details_open_=false; focus_underlying_controls(); if(!view_.error_key.empty()) emit(ActionKind::DismissError); }
    else if(id=="load-selected") emit(ActionKind::LoadSelected,view_.selected_save);
    else if(id.rfind("save-entry-",0)==0) {
        try { const auto index=std::stoul(id.substr(11)); if(index<view_.saves.size()) emit(ActionKind::SelectSave,view_.saves[index].id); } catch(const std::exception&) {}
    }
    else if(id.rfind("tab-",0)==0) {
        const std::array<const char*,8> tabs={"tab-observe","tab-terrain","tab-life","tab-civilization","tab-construction","tab-economy","tab-world","tab-settings"};
        for(std::size_t i=0;i<tabs.size();++i) if(id==tabs[i]) { section_=static_cast<BottomSection>(i); emit(ActionKind::SelectBottomSection,"",static_cast<int>(section_)); }
    }
    refresh(); layout();
}
bool Ui::valid_creation_inputs() const {
    double width=0,height=0;
    if(!parse_number(input_text(document_,"creation-width"),width) || !parse_number(input_text(document_,"creation-height"),height) || width<=0 || height<=0 || width!=std::floor(width) || height!=std::floor(height) || width>std::numeric_limits<std::int32_t>::max() || height>std::numeric_limits<std::int32_t>::max()) return false;
    return true;
}
void Ui::input_changed(const std::string& id,const std::string& value) {
    if(id=="creation-name") { creation_name_=value; emit(ActionKind::SetCreationName,value); return; }
    if(id=="world-name-draft") { rename_name_=value; emit(ActionKind::SetWorldNameDraft,value); return; }
    if(id=="creation-theme") { emit(ActionKind::SetCreationTheme,value); return; }
    if(id=="creation-width" || id=="creation-height") {
        double width=0,height=0;
        if(parse_number(input_text(document_,"creation-width"),width) && parse_number(input_text(document_,"creation-height"),height) && width==std::floor(width) && height==std::floor(height) && width>0 && height>0 && width<=std::numeric_limits<std::int32_t>::max() && height<=std::numeric_limits<std::int32_t>::max()) emit(ActionKind::SetCreationSize,"",width,height);
    } else if(id=="audio-volume") { double volume=0; if(parse_number(value,volume) && volume>=0 && volume<=100) emit(ActionKind::SetAudioVolume,"",std::round(volume)); }
}
void Ui::ProcessEvent(Rml::Event& event) {
    auto* target=event.GetTargetElement();
    if(event.GetType()=="change" || event.GetType()=="input") { if(target && !syncing_controls_) input_changed(target->GetId(),input_text(document_,target->GetId().c_str())); return; }
    if(event.GetType()=="keydown") {
        const int key=event.GetParameter<int>("key_identifier",0);
        const char* modal=!view_.error_key.empty() || !local_error_.empty()?"error-panel":settings_open_?"settings-panel":world_open_?"world-panel":help_open_?"help-panel":(view_.screen==Screen::Creation && controls_open_)?"creation-controls":nullptr;
        if(key==Rml::Input::KI_TAB && modal) {
            std::vector<Rml::Element*> controls;
            const auto collect=[&controls](auto&& self,Rml::Element* el)->void {
                if(el->GetComputedValues().display()==Rml::Style::Display::None) return;
                const auto tag=el->GetTagName();
                if((tag=="button" || tag=="input" || tag=="select") && !el->IsPseudoClassSet("disabled")) controls.push_back(el);
                for(int i=0;i<el->GetNumChildren();++i) self(self,el->GetChild(i));
            };
            collect(collect,document_->GetElementById(modal));
            if(!controls.empty()) {
                const auto found=std::find(controls.begin(),controls.end(),context_->GetFocusElement());
                int index=found==controls.end()?-1:static_cast<int>(found-controls.begin());
                index=(index+(event.GetParameter<bool>("shift_key",false)?-1:1)+static_cast<int>(controls.size()))%static_cast<int>(controls.size());
                controls[static_cast<std::size_t>(index)]->Focus(); controls[static_cast<std::size_t>(index)]->ScrollIntoView(false);
            }
            event.StopImmediatePropagation(); return;
        }
        if(key==Rml::Input::KI_ESCAPE) {
            if(!view_.error_key.empty() || !local_error_.empty()) { local_error_.clear(); focus_underlying_controls(); if(!view_.error_key.empty()) emit(ActionKind::DismissError); }
            else if(settings_open_) { settings_open_=false; pending_focus_=view_.screen==Screen::MainMenu?"settings-main":"tab-settings"; emit(ActionKind::CloseSettings); }
            else if(world_open_) { world_open_=false; pending_focus_="world-info"; emit(ActionKind::CloseWorldInspector); }
            else if(help_open_) { help_open_=false; pending_focus_="camera-help"; emit(ActionKind::CloseHelp); }
            else if(view_.screen==Screen::Creation) emit(ActionKind::CancelCreation);
            else if(view_.screen==Screen::Load) emit(ActionKind::CancelLoad);
            refresh(); layout(); event.StopPropagation();
        } else if((key==Rml::Input::KI_RETURN || key==Rml::Input::KI_SPACE) && target && target->GetTagName()=="button") { activate(target->GetId()); event.StopPropagation(); }
        return;
    }
    while(target && target!=document_) { if(target->GetTagName()=="button") { activate(target->GetId()); event.StopPropagation(); return; } target=target->GetParentNode(); }
}
}
