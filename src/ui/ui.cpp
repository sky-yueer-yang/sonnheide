#include "ui.hpp"
#include "pixel_decorator.hpp"
#include <RmlUi/Core.h>
#include <RmlUi/Core/Context.h>
#include <RmlUi/Core/Element.h>
#include <RmlUi/Core/ElementDocument.h>
#include <RmlUi/Core/Event.h>
#include <RmlUi/Core/Input.h>
#include <RmlUi/Core/Elements/ElementFormControl.h>
#include <RmlUi/Core/Elements/ElementFormControlSelect.h>
#include <algorithm>
#include <cmath>
#include <fstream>
#include <iomanip>
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
std::string number(double value) { std::ostringstream out; out.imbue(std::locale::classic()); out<<std::fixed<<std::setprecision(5)<<value; return out.str(); }
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
    if(view.screen==Screen::Creation && !view_.creation.preview_ready && view.creation.preview_ready && context_ && context_->GetDimensions().x/context_->GetDensityIndependentPixelRatio()<820) controls_open_=false;
    view_=view;
    if(creation_generation_!=view.creation.generation) { creation_generation_=view.creation.generation; creation_name_=view.creation.name; }
    if(rename_session_!=view.world.session) { rename_session_=view.world.session; rename_name_=view.world.name; world_open_=false; }
    if(changed_screen) { settings_open_=false; world_open_=false; help_open_=false; if(view.screen==Screen::Creation) controls_open_=true; }
    if(changed_session) section_=BottomSection::Observe;
    if(changed_screen) pending_focus_=view.screen==Screen::MainMenu?"new-world":view.screen==Screen::Creation?"creation-name":view.screen==Screen::Load?"back-load":"tab-observe";
    if(opened_error) pending_focus_="error-dismiss";
    if(closed_error) pending_focus_=view.screen==Screen::Creation?"creation-width":view.screen==Screen::World?"tab-observe":"new-world";
    refresh(); layout();
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
    const bool earth=view_.creation.mode==CreationMode::Earth;
    show(document_,"blank-controls",!earth); show(document_,"earth-controls",earth);
    const bool themes=view_.creation.can_select_theme && (earth || view_.creation.base==BlankBase::Soil);
    show(document_,"creation-theme-label",themes);
    show(document_,"creation-theme",themes);
    show(document_,"map-controls",earth && !view_.creation.preview_ready);
    show(document_,"preview-caption",view_.creation.preview_ready);
    choice(document_,"blank-mode",!earth); choice(document_,"earth-mode",earth);
    choice(document_,"blank-soil",view_.creation.base==BlankBase::Soil); choice(document_,"blank-ocean",view_.creation.base==BlankBase::Ocean);
    enable(document_,"earth-mode",view_.creation.earth_available && !view_.creation.busy);
    enable(document_,"blank-mode",!view_.creation.busy);
    enable(document_,"preview",!view_.creation.busy && (!earth || view_.creation.earth_available));
    enable(document_,"create",view_.creation.preview_ready && !view_.creation.busy);
    label(document_,"earth-package",view_.creation.package_status);
    input_value(context_,document_,"creation-name",creation_name_);
    input_value(context_,document_,"creation-width",std::to_string(view_.creation.width));
    input_value(context_,document_,"creation-height",std::to_string(view_.creation.height));
    enable(document_,"creation-height",!earth && !view_.creation.busy);
    const auto metres=[](std::int64_t mm){std::string value=std::to_string(mm/1000);auto fraction=std::to_string(1000+mm%1000).substr(1);while(!fraction.empty()&&fraction.back()=='0')fraction.pop_back();return fraction.empty()?value:value+"."+fraction;};
    label(document_,"creation-scale",metres(view_.creation.cell_mm)+text("metres-per-cell")+" · "+metres(static_cast<std::int64_t>(view_.creation.width)*view_.creation.cell_mm)+" × "+metres(static_cast<std::int64_t>(view_.creation.height)*view_.creation.cell_mm)+" m");
    input_value(context_,document_,"earth-west",number(view_.creation.west)); input_value(context_,document_,"earth-east",number(view_.creation.east));
    input_value(context_,document_,"earth-south",number(view_.creation.south)); input_value(context_,document_,"earth-north",number(view_.creation.north));
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
    for(int i=0;i<8;++i) { choice(document_,tabs[i],i==static_cast<int>(section_)); enable(document_,tabs[i],i==0 || i==6 || i==7); }
    show(document_,"observe-tools",section_==BottomSection::Observe,"flex"); show(document_,"world-tools",section_==BottomSection::World); show(document_,"age-tools",section_==BottomSection::World,"flex");
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
        else if(error_key.find("PACKAGE")!=std::string::npos || error_key.find("GSHHG")!=std::string::npos || error_key.find("GEOGRAPHY")!=std::string::npos) error_key="error-package";
        else if(error_key.find("NAME")!=std::string::npos || error_key=="INVALID_UTF8" || error_key=="NON_NFC") error_key="error-name";
        else if(error_key=="CREATION_DIMENSIONS") error_key="error-dimensions";
        else if(error_key.find("BUDGET")!=std::string::npos || error_key.find("SIZE")!=std::string::npos) error_key="error-size";
        else if(error_key=="EARTH_SELECTION" || error_key=="EARTH_BOUNDS" || error_key.find("REGION")!=std::string::npos || error_key.find("RECT")!=std::string::npos) error_key="error-region";
        else if(error_key.find("CHECKPOINT")!=std::string::npos || error_key.find("CONTENT")!=std::string::npos || error_key.find("FORMAT")!=std::string::npos) error_key="error-checkpoint";
        else error_key="error-generic";
    }
    label(document_,"error-message",text(error_key));
    label(document_,"error-detail",view_.error_key+(view_.error_detail.empty()?"":" · "+view_.error_detail));
    label(document_,"error-details",text(error_details_open_?"hide-details":"technical-details"));
    show(document_,"error-details",!view_.error_detail.empty(),"inline-block");
    show(document_,"error-detail",error_details_open_ && !view_.error_detail.empty());
    const auto icons=[this](auto&& self,Rml::Element* element)->void {
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
    const float menu_width=grid(std::min(264.f,w-32));
    property("main-menu","width",menu_width);
    property("main-menu","left",narrow?grid((w-menu_width)/2):grid(std::max(24.f,w*.12f)));
    property("main-menu","top",grid(std::max(16.f,(h-320.f)/2)));
    property("main-menu","max-height",std::max(160.f,h-32));
    show(document_,"creation-options",narrow,"inline-block");
    show(document_,"creation-controls",!narrow || controls_open_);
    property("creation-controls","left",narrow?16:24);
    property("creation-controls","width",grid(std::max(240.f,std::min(narrow?w-32:352.f,352.f))));
    property("creation-controls","height",grid(std::max(160.f,h-88)));
    property("scene-viewport","left",narrow?16:400);
    property("scene-viewport","top",68);
    property("scene-viewport","width",grid(std::max(8.f,w-(narrow?32:424))));
    property("scene-viewport","height",grid(std::max(8.f,h-88)));
    property("save-panel","left",grid(std::max(16.f,(w-std::min(800.f,w-32))/2)));
    property("save-panel","width",grid(std::min(800.f,w-32)));
    property("save-panel","top",68);property("save-panel","height",grid(std::max(128.f,h-88)));
    for(const auto* id:{"settings-panel","world-panel","help-panel","error-panel"}) {
        const float width=grid(std::min(704.f,w-32));
        property(id,"left",grid((w-width)/2));property(id,"width",width);
        const float top=grid(std::max(16.f,std::min(64.f,h*.08f)));
        property(id,"top",top);property(id,"max-height",grid(std::max(128.f,h-top-24)));
        property(id,"padding-left",20);property(id,"padding-right",20);
    }
    if(auto* bar=document_->GetElementById("bottom-bar")) {
        for(int i=0;i<bar->GetNumChildren();++i) if(auto* button=bar->GetChild(i)) {
            button->SetProperty("width",w<760?"25%":"12.5%");
            button->SetProperty("font-size","12dp");
            button->SetProperty("min-height","96dp");
            button->SetProperty("line-height","20dp");
        }
    }
    property("section-tools","bottom",w<760?208:112);
    property("section-tools","right",16);
    property("section-tools","width",grid(std::min(section_==BottomSection::World?320.f:224.f,w-32)));
    property("section-tools","max-height",grid(std::max(128.f,h-(w<760?336:240))));
    property("preview-caption","right",24);
    property("preview-caption","max-width",grid(std::max(128.f,w-(narrow?48:432))));
    if(auto* map=document_->GetElementById("map-controls")) {
        map->SetProperty("left",narrow?"16dp":"auto");map->SetProperty("right","24dp");
        map->SetProperty("max-width",std::to_string(grid(std::max(128.f,w-(narrow?40:424))))+"dp");
    }
}
void Ui::update() {
    if(!context_) return;
    context_->Update();
    if(view_.screen==Screen::World) {
        const float density=context_->GetDensityIndependentPixelRatio();
        const float bar_height=document_->GetElementById("bottom-bar")->GetBox().GetSize(Rml::Box::BORDER).y/density;
        const float h=context_->GetDimensions().y/density;
        auto* tools=document_->GetElementById("section-tools");
        const float bottom=std::ceil(bar_height/8)*8+8;
        const float max_height=std::max(128.f,std::floor((h-bar_height-136)/8)*8);
        if(std::abs(tools->ResolveNumericProperty("bottom")/density-bottom)>.1f || std::abs(tools->ResolveNumericProperty("max-height")/density-max_height)>.1f) {
            tools->SetProperty("bottom",std::to_string(bottom)+"dp");tools->SetProperty("max-height",std::to_string(max_height)+"dp");context_->Update();
        }
    }
    // RmlUi has no automatic browser-style title tooltip. Render the real
    // localised full name on actual pointer hover, without emitting an intent.
    Rml::Element* hovered=view_.screen==Screen::World && !blocks_world_input()?context_->GetHoverElement():nullptr;
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
        const float width=std::min(272.f,w-24);
        tooltip->SetProperty("width",std::to_string(width)+"dp");
        tooltip->SetProperty("left",std::to_string(std::clamp((offset.x+size.x/2)/density-width/2,12.f,std::max(12.f,w-width-12)))+"dp");
        context_->Update();
        const float height=tooltip->GetBox().GetSize(Rml::Box::BORDER).y/density;
        tooltip->SetProperty("top",std::to_string(std::clamp(offset.y/density-height-8,8.f,std::max(8.f,h-height-8)))+"dp");
        context_->Update();
    }
    if(!pending_focus_.empty()) {
        if(auto* el=document_->GetElementById(pending_focus_)) el->Focus();
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
        return {offset.x/density,offset.y/density,size.x/density,size.y/density,view_.creation.mode==CreationMode::Earth && !view_.creation.preview_ready};
    }
    const auto size=context_->GetDimensions(); return {0,0,size.x/density,size.y/density,false};
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
bool Ui::blocks_world_input() const { return settings_open_ || world_open_ || help_open_ || !view_.error_key.empty() || !local_error_.empty(); }
void Ui::emit(ActionKind kind,const std::string& value,double a,double b,double c,double d) {
    Action action; action.kind=kind; action.text=value; action.a=a; action.b=b; action.c=c; action.d=d;
    const bool creation=kind==ActionKind::SetCreationMode || kind==ActionKind::SetBlankBase || kind==ActionKind::SetCreationSize || kind==ActionKind::SetCreationName || kind==ActionKind::SetCreationTheme || kind==ActionKind::SetEarthBounds || kind==ActionKind::MapZoom || kind==ActionKind::MapPan || kind==ActionKind::RequestPreview || kind==ActionKind::CreateWorld || kind==ActionKind::CancelCreation;
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
    else if(id=="settings-main" || id=="tab-settings") { settings_open_=true; pending_focus_="locale-zh"; emit(ActionKind::OpenSettings); }
    else if(id=="settings-close") { settings_open_=false; pending_focus_=view_.screen==Screen::MainMenu?"settings-main":"tab-settings"; emit(ActionKind::CloseSettings); }
    else if(id=="cancel-create") emit(ActionKind::CancelCreation);
    else if(id=="creation-options") controls_open_=!controls_open_;
    else if(id=="blank-mode") emit(ActionKind::SetCreationMode,"blank",static_cast<double>(CreationMode::Blank));
    else if(id=="earth-mode") emit(ActionKind::SetCreationMode,"earth",static_cast<double>(CreationMode::Earth));
    else if(id=="blank-soil") emit(ActionKind::SetBlankBase,"soil",static_cast<double>(BlankBase::Soil));
    else if(id=="blank-ocean") emit(ActionKind::SetBlankBase,"ocean",static_cast<double>(BlankBase::Ocean));
    else if(id=="preview") { if(!valid_creation_inputs()) { local_error_="error-input"; pending_focus_="error-dismiss"; } else emit(ActionKind::RequestPreview); }
    else if(id=="create") { if(!valid_creation_inputs()) { local_error_="error-input"; pending_focus_="error-dismiss"; } else emit(ActionKind::CreateWorld); }
    else if(id=="map-in" || id=="map-out") emit(ActionKind::MapZoom,"",id=="map-in"?1:-1);
    else if(id=="map-left" || id=="map-right" || id=="map-up" || id=="map-down") emit(ActionKind::MapPan,"",id=="map-left"?-1:id=="map-right"?1:0,id=="map-up"?1:id=="map-down"?-1:0);
    else if(id=="locale-zh" || id=="locale-en" || id=="locale-de") emit(ActionKind::SetLocale,id=="locale-zh"?"zh-CN":id=="locale-en"?"en":"de");
    else if(id=="fullscreen" || id=="windowed") emit(ActionKind::SetFullscreen,"",id=="fullscreen"?1:0);
    else if(id=="motion-normal" || id=="motion-reduced") emit(ActionKind::SetReducedMotion,"",id=="motion-reduced"?1:0);
    else if(id=="volume-down" || id=="volume-up") emit(ActionKind::SetAudioVolume,"",std::clamp(view_.audio_volume+(id=="volume-up"?5:-5),0,100));
    else if(id=="camera-home") emit(ActionKind::CameraHome);
    else if(id=="camera-help") { help_open_=true; pending_focus_="help-close"; emit(ActionKind::OpenHelp); }
    else if(id=="help-close") { help_open_=false; pending_focus_="camera-help"; emit(ActionKind::CloseHelp); }
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
    else if(id=="error-dismiss") { local_error_.clear(); error_details_open_=false; pending_focus_=view_.screen==Screen::Creation?"creation-width":view_.screen==Screen::World?"tab-observe":"new-world"; if(!view_.error_key.empty()) emit(ActionKind::DismissError); }
    else if(id=="load-selected") emit(ActionKind::LoadSelected,view_.selected_save);
    else if(id.rfind("save-entry-",0)==0) {
        try { const auto index=std::stoul(id.substr(11)); if(index<view_.saves.size()) emit(ActionKind::SelectSave,view_.saves[index].id); } catch(const std::exception&) {}
    }
    else if(id=="tab-observe" || id=="tab-world") { section_=id=="tab-observe"?BottomSection::Observe:BottomSection::World; emit(ActionKind::SelectBottomSection,"",static_cast<int>(section_)); }
    refresh(); layout();
}
bool Ui::valid_creation_inputs() const {
    double width=0,height=0;
    if(!parse_number(input_text(document_,"creation-width"),width) || !parse_number(input_text(document_,"creation-height"),height) || width<=0 || height<=0 || width!=std::floor(width) || height!=std::floor(height) || width>std::numeric_limits<std::int32_t>::max() || height>std::numeric_limits<std::int32_t>::max()) return false;
    if(view_.creation.mode==CreationMode::Earth) {
        double west=0,south=0,east=0,north=0;
        if(!parse_number(input_text(document_,"earth-west"),west) || !parse_number(input_text(document_,"earth-south"),south) || !parse_number(input_text(document_,"earth-east"),east) || !parse_number(input_text(document_,"earth-north"),north)) return false;
        if(west < -180 || west>180 || east<=west || east-west>360 || south < -90 || north>90 || south>=north) return false;
    }
    return true;
}
void Ui::input_changed(const std::string& id,const std::string& value) {
    if(id=="creation-name") { creation_name_=value; emit(ActionKind::SetCreationName,value); return; }
    if(id=="world-name-draft") { rename_name_=value; emit(ActionKind::SetWorldNameDraft,value); return; }
    if(id=="creation-theme") { emit(ActionKind::SetCreationTheme,value); return; }
    if(id=="creation-width" || id=="creation-height") {
        double width=0,height=0;
        if(parse_number(input_text(document_,"creation-width"),width) && parse_number(input_text(document_,"creation-height"),height) && width==std::floor(width) && height==std::floor(height) && width>0 && height>0 && width<=std::numeric_limits<std::int32_t>::max() && height<=std::numeric_limits<std::int32_t>::max()) emit(ActionKind::SetCreationSize,"",width,height);
    } else if(id.rfind("earth-",0)==0) {
        double west=0,south=0,east=0,north=0;
        if(parse_number(input_text(document_,"earth-west"),west) && parse_number(input_text(document_,"earth-south"),south) && parse_number(input_text(document_,"earth-east"),east) && parse_number(input_text(document_,"earth-north"),north)) emit(ActionKind::SetEarthBounds,"",west,south,east,north);
    } else if(id=="audio-volume") { double volume=0; if(parse_number(value,volume) && volume>=0 && volume<=100) emit(ActionKind::SetAudioVolume,"",std::round(volume)); }
}
void Ui::ProcessEvent(Rml::Event& event) {
    auto* target=event.GetTargetElement();
    if(event.GetType()=="change" || event.GetType()=="input") { if(target && !syncing_controls_) input_changed(target->GetId(),input_text(document_,target->GetId().c_str())); return; }
    if(event.GetType()=="keydown") {
        const int key=event.GetParameter<int>("key_identifier",0);
        const char* modal=!view_.error_key.empty() || !local_error_.empty()?"error-panel":settings_open_?"settings-panel":world_open_?"world-panel":help_open_?"help-panel":nullptr;
        if(key==Rml::Input::KI_TAB && modal) {
            std::vector<Rml::Element*> controls;
            const auto collect=[&controls](auto&& self,Rml::Element* el)->void {
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
            if(!view_.error_key.empty() || !local_error_.empty()) { local_error_.clear(); pending_focus_=view_.screen==Screen::Creation?"creation-width":view_.screen==Screen::World?"tab-observe":"new-world"; if(!view_.error_key.empty()) emit(ActionKind::DismissError); }
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
