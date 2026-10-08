#include "native_menu.hpp"
#include "native_earth_page.hpp"
#include <sonnheide/native_platform.hpp>
#include <SDL3/SDL_main.h>
#include <RmlUi/Core.h>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <stdexcept>

namespace {
std::string path_utf8(const std::filesystem::path& path) {
    const auto value=path.u8string();
    return {reinterpret_cast<const char*>(value.data()),value.size()};
}
void require(bool value,const char* message) { if (!value) throw std::runtime_error(message); }
struct TestSystem final : Rml::SystemInterface {
    double GetElapsedTime() override { return 0; }
    bool LogMessage(Rml::Log::Type type,const Rml::String& message) override {
        if (type <= Rml::Log::LT_WARNING) std::cerr << message << '\n';
        return true;
    }
};
// The real RmlUi parser, fonts, layout engine, DOM and event system run here.
// This renderer merely accepts their geometry; no OS window or GPU is needed.
struct NullRenderer final : Rml::RenderInterface {
    void RenderGeometry(Rml::Vertex*,int,int*,int,Rml::TextureHandle,const Rml::Vector2f&) override {}
    void EnableScissorRegion(bool) override {}
    void SetScissorRegion(int,int,int,int) override {}
    bool GenerateTexture(Rml::TextureHandle& handle,const Rml::byte*,const Rml::Vector2i&) override { handle=++next_; return true; }
    Rml::TextureHandle next_=0;
};
struct Rectangle { float x,y,w,h; };
Rectangle bounds(Rml::ElementDocument* document,const char* id,float density) {
    auto* element=document->GetElementById(id); require(element != nullptr,"Native document required element is missing");
    const auto offset=element->GetAbsoluteOffset(Rml::Box::BORDER);
    const auto size=element->GetBox().GetSize(Rml::Box::BORDER);
    return {offset.x/density,offset.y/density,size.x/density,size.y/density};
}
void click(Rml::Context& context,const Rectangle& box,float density) {
    context.ProcessMouseMove(static_cast<int>((box.x+box.w/2)*density),static_cast<int>((box.y+box.h/2)*density),0);
    context.ProcessMouseButtonDown(0,0); context.ProcessMouseButtonUp(0,0);
}
void update(sonnheide::client::NativeMenu& menu,Rml::Context& context) { menu.update(0,true); context.Update(); context.Render(); }
void exercise_earth(const std::filesystem::path& root,int width,int height,float density) {
 auto* context=Rml::CreateContext("earth-layout",{int(width*density),int(height*density)});require(context,"Cannot create Earth UI context");context->SetDensityIndependentPixelRatio(density);
 {
  sonnheide::client::NativeEarthPage page(*context,root,std::filesystem::temp_directory_path()/"sonnheide-ui-layout-unused");std::string error;
  require(page.initialize(error),"Cannot parse native creation document");
  for(auto locale:{sonnheide::client::Locale::Chinese,sonnheide::client::Locale::English,sonnheide::client::Locale::German}){
   page.open(locale);context->Update();context->Render();auto* doc=context->GetDocument(0);
   const auto bar=bounds(doc,"earth-toolbar",density);require(bar.y>=0&&bar.y+bar.h<=height+.5,"Earth toolbar clipped vertically");
   for(const auto* id:{"earth-action-back","earth-action-select","earth-action-scale","earth-action-preview"}){const auto r=bounds(doc,id,density);require(r.w>0&&r.x>=0&&r.x+r.w<=width+.5&&r.y>=bar.y&&r.y+r.h<=height+.5,"Earth action escaped its bottom toolbar");}
   click(*context,bounds(doc,"earth-action-sources",density),density);context->Update();
   const auto source=bounds(doc,"earth-sources-panel",density);if(source.x<0||source.y<0||source.x+source.w>width+.5||source.y+source.h>bar.y+.5)std::cerr<<"source rect="<<source.x<<","<<source.y<<","<<source.w<<","<<source.h<<" toolbar_y="<<bar.y<<" window="<<width<<","<<height<<'\n';require(source.x>=0&&source.y>=0&&source.x+source.w<=width+.5&&source.y+source.h<=bar.y+.5,"Source panel covered the global toolbar or viewport");
   context->ProcessKeyDown(Rml::Input::KI_ESCAPE,0);context->Update();require(page.visible(),"Escape from Sources discarded the world draft");
   auto* back=doc->GetElementById("earth-action-back");back->Focus();context->ProcessKeyDown(Rml::Input::KI_RETURN,0);context->Update();require(!page.visible()&&page.consume_return(),"Earth keyboard Back did not return to main menu");
   page.open(locale);context->Update();click(*context,bounds(doc,"earth-action-back",density),density);context->Update();require(!page.visible()&&page.consume_return(),"Earth pointer Back did not return to menu");
  }
  std::cout<<"Native creation layout "<<width<<'x'<<height<<" density="<<density<<": three locales, bottom controls, keyboard and pointer return passed\n";
 }
 Rml::RemoveContext("earth-layout");
}
void exercise(const std::filesystem::path& root,int width,int height,float density) {
    auto* context=Rml::CreateContext("layout-test",{static_cast<int>(width*density),static_cast<int>(height*density)});
    require(context != nullptr,"Cannot create native layout-test context");
    context->SetDensityIndependentPixelRatio(density);
    {
        sonnheide::client::NativeMenu menu(*context,root); std::string error;
        require(menu.initialize(error),"Cannot parse packaged native RML"); update(menu,*context);
        auto* document=context->GetDocument(0);
        const auto title=bounds(document,"main-title",density);
        const auto first=bounds(document,"action-new",density), second=bounds(document,"action-continue",density), last=bounds(document,"action-quit",density);
        require(std::abs(first.y-title.y-title.h-12)<.1,"Native menu title/first action gap drifted");
        require(std::abs(second.y-first.y-24)<.1 && std::abs(last.y-first.y-96)<.1,"Native menu compact rows no longer form 24dp hit regions");
        require(first.x>=0 && first.x+first.w<=width && last.y+last.h<=height,"Menu controls are clipped from the window");
        // Real pointer events, not direct action calls, must open the settings panel.
        click(*context,bounds(document,"action-settings",density),density); update(menu,*context);
        require(menu.panel() == sonnheide::client::Panel::Settings,"Native settings hit target did not open actual panel");
        for (const auto* language:{"zh","en","de"}) {
            menu.activate(language); update(menu,*context);
            float bottom=0;
            for (const auto* id:{"title-settings","text-language","action-zh","text-motion","action-normal","action-previous","action-full","action-back"}) {
                const auto rect=bounds(document,id,density);
                require(rect.h>0 && rect.y>=bottom-.1,"Native settings paragraphs/rows overlap or remain inline");
                bottom=rect.y+rect.h;
            }
            const auto dialog=bounds(document,"dialog",density);
            require(dialog.x>=0 && dialog.x+dialog.w<=width+.1 && dialog.y>=0 && dialog.y+dialog.h<=height+.1,"Settings panel viewport clipping is broken");
        }
        click(*context,bounds(document,"action-en",density),density); update(menu,*context);
        require(menu.preferences().locale == sonnheide::client::Locale::English,"Native pointer language selection failed");
        context->ProcessKeyDown(Rml::Input::KI_ESCAPE,0); update(menu,*context);
        require(menu.panel() == sonnheide::client::Panel::None,"Native Escape did not return to main menu");
        // A monitor-density change without a pixel-size change must still refresh
        // absolute dp anchors and font rasterization; the physical title stays visible.
        if (width==1280 && density==2) {
            context->SetDensityIndependentPixelRatio(1); update(menu,*context);
            const auto changed=bounds(document,"main-title",1);
            require(changed.y>title.y*2,"Same-size density change left stale menu anchor cache");
        }
        std::cout << "Native Rml layout " << width << 'x' << height << " density=" << density << ": three locales and real pointer/Escape passed\n";
    }
    Rml::RemoveContext("layout-test");
}
}
int main(int argc,char** argv) {
    TestSystem system; NullRenderer renderer; sonnheide::native::UnicodeFileInterface files;
    Rml::SetSystemInterface(&system); Rml::SetRenderInterface(&renderer); Rml::SetFileInterface(&files);
    if (!Rml::Initialise()) { std::cerr << "RmlUi initialization failed\n"; return 1; }
    int result=0;
    try {
        const auto root=argc>1 ? std::filesystem::u8path(argv[1]) : std::filesystem::current_path();
        require(Rml::LoadFontFace(path_utf8(root/"assets/source/ui/fonts/Cinzel.ttf"),true),"Packaged Roman font failed");
        require(Rml::LoadFontFace(path_utf8(root/"assets/source/ui/fonts/noto-serif-cjk-sc/NotoSerifCJKsc-Regular.otf"),true),"Packaged Chinese font failed");
        for (float density:{1.f,2.f}) { exercise(root,1280,800,density); exercise(root,400,600,density);exercise_earth(root,1280,800,density);exercise_earth(root,400,600,density); }
    } catch (const std::exception& exception) { std::cerr << exception.what() << '\n'; result=1; }
    Rml::Shutdown(); return result;
}
