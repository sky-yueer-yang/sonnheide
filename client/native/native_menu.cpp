#include "native_menu.hpp"
#include <RmlUi/Core/Context.h>
#include <RmlUi/Core/Element.h>
#include <RmlUi/Core/ElementDocument.h>
#include <RmlUi/Core/Event.h>
#include <RmlUi/Core/Input.h>
#include <algorithm>
#include <chrono>
#include <stdexcept>

namespace sonnheide::client {
namespace {
// RmlUi 5.1 has no letter-spacing property. Native inline boxes provide the
// requested .01em only for the six main-page labels, without modifying fonts.
std::string tracked_label(std::string_view text) {
    std::string result; bool first=true;
    for (std::size_t i=0;i<text.size();) {
        const auto c=static_cast<unsigned char>(text[i]);
        const std::size_t count=c<0x80 ? 1 : c<0xe0 ? 2 : c<0xf0 ? 3 : 4;
        result += first ? "<span>" : "<span class=\"tracked\">";
        result.append(text.substr(i,count)); result += "</span>"; first=false; i+=count;
    }
    return result;
}
}

NativeMenu::NativeMenu(Rml::Context& context,std::filesystem::path root,Preferences preferences)
 : context_(context),asset_root_(std::move(root)),model_(preferences,static_cast<std::uint64_t>(std::chrono::steady_clock::now().time_since_epoch().count())) {}
NativeMenu::~NativeMenu() { if (document_) { document_->RemoveEventListener("click",this); document_->RemoveEventListener("keydown",this); document_->Close(); } }
bool NativeMenu::initialize(std::string& error) {
    const auto path=(asset_root_/"ui/application/main-menu.rml").u8string();
    document_=context_.LoadDocument(std::string(reinterpret_cast<const char*>(path.data()),path.size()));
    if (!document_) { error="Could not load native main-menu.rml"; return false; }
    document_->AddEventListener("click",this); document_->AddEventListener("keydown",this);
    if (auto* unavailable=document_->GetElementById("action-continue")) { unavailable->SetPseudoClass("disabled",true); unavailable->SetProperty("focus","none"); unavailable->SetProperty("tab-index","none"); }
    refresh(); layout(); document_->Show(); focus_first(); error.clear(); return true;
}
void NativeMenu::update(double seconds,bool visible) { model_.update(seconds,visible); layout(); }
void NativeMenu::layout() {
    if (!document_) return;
    const auto dimensions=context_.GetDimensions();
    const float density=std::max(.1f,context_.GetDensityIndependentPixelRatio());
    if (last_width_ == dimensions.x && last_height_ == dimensions.y && last_density_ == density) return;
    last_width_=dimensions.x; last_height_=dimensions.y; last_density_=density;
    const float h=static_cast<float>(dimensions.y)/density,w=static_cast<float>(dimensions.x)/density;
    float bottom=.08f*h+20, anchor_step=36,anchor_gap=36,right=.07f*w;
    if (w >= 1800) { anchor_step=39; anchor_gap=43; }
    if (w <= 900) bottom=.07f*h+20;
    if (w <= 560) { anchor_step=40; anchor_gap=24; right=.08f*w; }
    if (h <= 580) { bottom=.05f*h+12; anchor_step=34; anchor_gap=w > 560 ? 25 : 21; }
    // RmlUi 5.1's glow effect reads radius parameters as raw integers, unlike
    // normal dp properties. Scale these raster radii explicitly for Retina.
    const std::string glow="glow("+std::to_string(2*density)+"px "+std::to_string(3*density)+"px #fff6df38)";
    for (const auto* id:{"main-title","action-new","action-continue","action-load","action-settings","action-quit"})
        if (auto* element=document_->GetElementById(id)) element->SetProperty("font-effect",glow);
    const float top=std::max(16.f,h-bottom-5*anchor_step-anchor_gap-18.88f);
    if (auto* menu=document_->GetElementById("menu-group")) { menu->SetProperty("top",std::to_string(top)+"dp"); menu->SetProperty("right",std::to_string(right)+"dp"); }
    if (auto* panel=document_->GetElementById("dialog")) { panel->SetProperty("width",std::to_string(std::max(260.f,std::min(640.f,w-48)))+"dp"); panel->SetProperty("left",std::to_string(std::max(12.f,(w-std::min(640.f,w-48))/2))+"dp"); panel->SetProperty("top",std::to_string(std::max(16.f,std::min(h*.16f,90.f)))+"dp"); }
}
void NativeMenu::refresh() {
    if (!document_) return;
    document_->SetProperty("font-family",model_.preferences().locale == Locale::Chinese ? "Noto Serif CJK SC" : "Cinzel");
    const char* translated[]={"new","continue","load","settings","quit","back","language","motion","reduce","full","window","play","pause","previous","next","normal","newbody","newdetail","loadbody","loaddetail","preferror"};
    for (const auto* key:translated) if (auto* element=document_->GetElementById(std::string("text-")+key)) {
        const std::string_view name(key);
        const bool main_label=name == "new" || name == "continue" || name == "load" || name == "settings" || name == "quit";
        element->SetInnerRML(main_label ? tracked_label(model_.text(key)) : std::string(model_.text(key)));
    }
    document_->GetElementById("main-title")->SetInnerRML(tracked_label("SONNHEIDE"));
    const bool shown=model_.panel() != Panel::None;
    document_->GetElementById("overlay")->SetProperty("display",shown ? "block" : "none");
    document_->GetElementById("menu-group")->SetProperty("display",shown ? "none" : "block");
    for (auto pair : {std::pair{"new-panel",Panel::NewWorld},std::pair{"load-panel",Panel::LoadWorld},std::pair{"settings-panel",Panel::Settings}})
        document_->GetElementById(pair.first)->SetProperty("display",model_.panel() == pair.second ? "block" : "none");
    const auto& p=model_.preferences();
    for (const auto* action:{"zh","en","de","reduced","normal","full","window","pause","play"}) {
        bool active=(std::string(action) == locale_code(p.locale)) || (std::string(action) == (p.reduced_motion ? "reduced" : "normal")) || (std::string(action) == (p.fullscreen ? "full" : "window")) || (std::string(action) == (p.painting_paused ? "pause" : "play"));
        if (auto* element=document_->GetElementById(std::string("action-")+action)) element->SetClass("selected",active);
    }
    for (const auto* key:{"new","load","settings"}) document_->GetElementById(std::string("title-")+key)->SetInnerRML(std::string(model_.text(key)));
    document_->GetElementById("text-preferror")->SetProperty("display",preference_failure_ ? "block" : "none");
}
void NativeMenu::focus_first() { if (!document_) return; const char* id=model_.panel() == Panel::Settings ? "action-zh" : model_.panel() == Panel::None ? "action-new" : "action-back"; if (auto* element=document_->GetElementById(id)) element->Focus(); }
void NativeMenu::close_panel() { model_.close(); refresh(); focus_first(); }
void NativeMenu::activate(const std::string& action) {
    const Panel previous_panel=model_.panel();
    if (action == "new") model_.open(Panel::NewWorld);
    else if (action == "load") model_.open(Panel::LoadWorld);
    else if (action == "settings") model_.open(Panel::Settings);
    else if (action == "back") model_.close();
    else if (action == "quit") quit_=true;
    else if (action == "continue") return; // No valid save pointer exists in stage 0.
    else if (action == "previous" || action == "next") { model_.step_painting(action == "previous" ? -1 : 1); return; }
    else {
        auto p=model_.preferences();
        if (action == "zh") p.locale=Locale::Chinese; else if (action == "en") p.locale=Locale::English; else if (action == "de") p.locale=Locale::German;
        else if (action == "reduced") p.reduced_motion=true; else if (action == "normal") p.reduced_motion=false;
        else if (action == "full") p.fullscreen=true; else if (action == "window") p.fullscreen=false;
        else if (action == "pause") p.painting_paused=true; else if (action == "play") p.painting_paused=false; else return;
        if (p != model_.preferences()) { model_.set_preferences(p); changed_=true; }
    }
    refresh(); if (model_.panel() != previous_panel) focus_first();
}
void NativeMenu::ProcessEvent(Rml::Event& event) {
    if (event.GetType() == "keydown") {
        const int key=event.GetParameter<int>("key_identifier",0);
        if (key == Rml::Input::KI_ESCAPE && model_.panel() != Panel::None) { close_panel(); event.StopPropagation(); }
        else if (key == Rml::Input::KI_RETURN || key == Rml::Input::KI_SPACE) {
            auto* element=event.GetTargetElement();
            if (element && element->GetId().find("action-") == 0) { activate(element->GetId().substr(7)); event.StopPropagation(); }
        }
        return;
    }
    auto* target=event.GetTargetElement();
    while (target && target != document_) {
        const auto& id=target->GetId(); if (id.find("action-") == 0) { activate(id.substr(7)); event.StopPropagation(); return; } target=target->GetParentNode();
    }
}
bool NativeMenu::consume_quit_request() { const bool result=quit_; quit_=false; return result; }
bool NativeMenu::consume_preferences_changed() { const bool result=changed_; changed_=false; return result; }
void NativeMenu::set_preferences_error(bool failed) { preference_failure_=failed; refresh(); }
}
