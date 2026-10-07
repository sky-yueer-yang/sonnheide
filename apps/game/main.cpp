#include "sonnheide/native_platform.hpp"
#include <SDL3/SDL_main.h>
#include "bgfx_render_interface.hpp"
#include "native_menu.hpp"
#include <RmlUi/Core.h>
#include <algorithm>
#include <chrono>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {
std::string path_utf8(const std::filesystem::path& path) {
    const auto value=path.u8string();
    return {reinterpret_cast<const char*>(value.data()),value.size()};
}
struct RmlLifetime {
    RmlLifetime() { if (!Rml::Initialise()) throw std::runtime_error("RmlUi initialization failed"); }
    ~RmlLifetime() { Rml::Shutdown(); }
};
void draw_painting(sonnheide::native::BgfxRenderInterface& renderer,
                  const std::filesystem::path& root, int index,
                  const sonnheide::native::ImagePixels& image,
                  const sonnheide::client::PaintingPose& pose,
                  double scale, float opacity, int width, int height) {
    if (index < 0 || image.width <= 0 || image.height <= 0) return;
    const float cover = std::max(float(width) / image.width, float(height) / image.height);
    const float w = image.width * cover * 1.06f * float(scale);
    const float h = image.height * cover * 1.06f * float(scale);
    const float x = (width - w) * .55f + width * float(pose.x);
    const float y = (height - h) * .40f + height * float(pose.y);
    renderer.draw_image(root / ("assets/source/ui/paintings/painting-0" + std::to_string(index+1) + ".jpg"), x,y,w,h,opacity);
}
}

int main(int argc, char** argv) {
    try {
        bool smoke = false;
        std::filesystem::path root;
        for (int i=1; i<argc; ++i) {
            const std::string arg = argv[i];
            if (arg == "--smoke-test") smoke = true;
            else if (arg == "--resources" && i+1 < argc) root = std::filesystem::u8path(argv[++i]);
            else throw std::runtime_error("Usage: Sonnheide [--resources PATH] [--smoke-test]");
        }
        if (root.empty()) {
            const char* base = SDL_GetBasePath();
            if (!base) throw std::runtime_error("Cannot locate application resources");
            root = std::filesystem::u8path(base);
            if (!std::filesystem::exists(root / "ui/application/main-menu.rml")) {
                if (std::filesystem::exists(root / "Resources/ui/application/main-menu.rml")) root /= "Resources";
                else root /= "../Resources";
            }
        }
        root = std::filesystem::absolute(root).lexically_normal();
        std::filesystem::path preference_path;
        if (smoke) preference_path = root / "../../smoke-preferences.conf";
        else {
            char* path = SDL_GetPrefPath("Sonnreich", "Sonnheide");
            if (!path) throw std::runtime_error("Cannot locate user preferences directory");
            preference_path = std::filesystem::u8path(path) / "preferences.conf";
            SDL_free(path);
        }
        const auto initial_preferences = sonnheide::client::load_preferences(preference_path);
        sonnheide::native::PlatformWindow window;
        window.fullscreen(initial_preferences.fullscreen);
        int width=0,height=0,pixels_w=0,pixels_h=0;
        window.dimensions(width,height,pixels_w,pixels_h);
        sonnheide::native::SdlSystemInterface system;
        sonnheide::native::UnicodeFileInterface files;
        sonnheide::native::BgfxRenderInterface renderer(window.native_handle(),root,pixels_w,pixels_h);
        Rml::SetSystemInterface(&system);
        Rml::SetFileInterface(&files);
        Rml::SetRenderInterface(&renderer);
        RmlLifetime rml;
        for (const auto& font : {root/"assets/source/ui/fonts/Cinzel.ttf", root/"assets/source/ui/fonts/noto-serif-cjk-sc/NotoSerifCJKsc-Regular.otf"}) {
            if (!Rml::LoadFontFace(path_utf8(font),true)) throw std::runtime_error("Cannot load packaged font: " + path_utf8(font));
        }
        auto* context = Rml::CreateContext("main-menu",{pixels_w,pixels_h});
        if (!context) throw std::runtime_error("Cannot create native UI context");
        context->SetDensityIndependentPixelRatio(float(pixels_w)/width);
        {
            sonnheide::client::NativeMenu menu(*context,root,initial_preferences);
            sonnheide::native::ImagePixels paintings[5];
            for (int index : {2,0,1,3,4}) {
                const auto painting_path=root/("assets/source/ui/paintings/painting-0"+std::to_string(index+1)+".jpg");
                try {
                    paintings[index] = sonnheide::native::load_image(painting_path);
                    paintings[index].rgba.clear(); paintings[index].rgba.shrink_to_fit();
                } catch (const std::exception& exception) { std::cerr << exception.what() << '\n'; }
                menu.set_painting_available(index, paintings[index].width > 0 && renderer.prepare_image(painting_path));
            }
            std::string error;
            if (!menu.initialize(error)) throw std::runtime_error(error);
            SDL_StartTextInput(window.window());
            std::cout << "Sonnheide native renderer: " << renderer.renderer_name() << '\n';
            std::cout << "Logical " << width << 'x' << height << ", pixels " << pixels_w << 'x' << pixels_h << '\n';
            auto previous = std::chrono::steady_clock::now();
            bool running = true, previous_visible = false;
            unsigned frame=0;
            while (running) {
                SDL_Event event;
                while (SDL_PollEvent(&event)) {
                    if (event.type == SDL_EVENT_QUIT || event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED) running=false;
                    else sonnheide::native::process_event(event,*context);
                }
                if (!running) break;
                window.dimensions(width,height,pixels_w,pixels_h);
                if (width<=0 || height<=0 || pixels_w<=0 || pixels_h<=0) { SDL_Delay(20); continue; }
                context->SetDimensions({pixels_w,pixels_h});
                context->SetDensityIndependentPixelRatio(float(pixels_w)/width);
                const auto now=std::chrono::steady_clock::now();
                const bool visible=window.visible();
                const double delta=visible && previous_visible ? std::chrono::duration<double>(now-previous).count() : 0;
                previous=now; previous_visible=visible;
                menu.update(delta,visible);
                if (smoke) {
                    if (frame==10) menu.activate("settings");
                    if (frame==20) menu.activate("en");
                    if (frame==30) menu.activate("de");
                    if (frame==40) menu.activate("zh");
                    if (frame==50) menu.activate("back");
                    if (frame==60) menu.activate("new");
                    if (frame==70) menu.activate("back");
                    if (frame==80) menu.activate("load");
                    if (frame==90) menu.activate("back");
                    if (frame==120) menu.activate("quit");
                }
                if (menu.consume_preferences_changed()) {
                    window.fullscreen(menu.preferences().fullscreen);
                    menu.set_preferences_error(!sonnheide::client::save_preferences(preference_path,menu.preferences(),error));
                    if (!error.empty()) std::cerr << error << '\n';
                }
                if (menu.consume_quit_request()) break;
                if (!visible) { SDL_Delay(20); continue; }
                renderer.begin_frame(width,height,pixels_w,pixels_h);
                const auto pose=menu.presentation();
                if (pose.front>=0) draw_painting(renderer,root,pose.front,paintings[pose.front],pose,pose.front_scale,1,width,height);
                if (pose.back>=0) draw_painting(renderer,root,pose.back,paintings[pose.back],pose,pose.back_scale,float(pose.blend),width,height);
                renderer.draw_menu_shade(float(width),float(height));
                float size=std::clamp(width*.84f,760.f,1400.f),logo=std::clamp(width*.14f,112.f,190.f);
                float cx=std::clamp(width*.175f,160.f,280.f),cy=std::clamp(height*.23f,160.f,225.f);
                if (width<=900) { cx=std::clamp(width*.20f,150.f,185.f); cy=std::clamp(height*.24f,150.f,185.f); }
                if (width<=560) { size=720;logo=100;cx=116;cy=150; }
                if (height<=580) { size=std::clamp(width*.76f,640.f,1000.f);logo=104;cx=148;cy=130; if (width<=560) {size=620;logo=88;cx=100;cy=112;} }
                renderer.draw_compass(cx,cy,size,logo);
                context->Update(); renderer.begin_ui_frame(); context->Render(); renderer.end_frame();
                ++frame;
            }
            SDL_StopTextInput(window.window());
            std::cout << "Native session shut down cleanly; frames=" << frame << '\n';
        }
        Rml::RemoveContext("main-menu");
        return 0;
    } catch (const std::exception& exception) {
        std::cerr << "Sonnheide: " << exception.what() << '\n';
        return 1;
    }
}
