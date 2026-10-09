#include "sonnheide/native_platform.hpp"
#include <SDL3/SDL_main.h>
#include "bgfx_render_interface.hpp"
#include "native_menu.hpp"
#include "native_earth_page.hpp"
#include "earth_renderer.hpp"
#include <RmlUi/Core.h>
#include <RmlUi/Core/Elements/ElementFormControlInput.h>
#include <bgfx/bgfx.h>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
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
sonnheide::native::EarthCamera check_entry_camera(const sonnheide::client::NativeEarthPage& earth,
                                                const char* entry) {
    const auto camera=earth.camera_state();
    if (camera.orthographic || !std::isfinite(camera.height_m) || camera.height_m<=0 || camera.height_m>40 ||
        !std::isfinite(camera.pitch_deg) || camera.pitch_deg<=0 || camera.pitch_deg>25 ||
        !std::isfinite(camera.yaw_deg) || !std::isfinite(camera.target_x_m) || !std::isfinite(camera.target_z_m))
        throw std::runtime_error(std::string("Phase1 ")+entry+" did not enter a finite perspective close view (height <=40m, pitch <=25 degrees)");
    std::cout<<"Phase1 camera "<<entry<<": perspective, height="<<camera.height_m<<", pitch="<<camera.pitch_deg
             <<", yaw="<<camera.yaw_deg<<", target="<<camera.target_x_m<<','<<camera.target_z_m<<'\n';
    return camera;
}
bool same_camera(const sonnheide::native::EarthCamera& a,const sonnheide::native::EarthCamera& b) {
    return a.target_x_m==b.target_x_m && a.target_z_m==b.target_z_m && a.height_m==b.height_m &&
           a.pitch_deg==b.pitch_deg && a.yaw_deg==b.yaw_deg && a.orthographic==b.orthographic;
}
void write_ground_evidence(const std::filesystem::path& folder,
                           const sonnheide::client::NativeEarthPage& earth,
                           const sonnheide::native::EarthCamera& camera,
                           const char* backend,int width,int height) {
    std::ofstream file(folder/"ground-camera.json",std::ios::binary);
    if(!file)throw std::runtime_error("Cannot write native ground camera evidence");
    // WorldId and recipe are canonical hexadecimal identities, never user text.
    file<<std::setprecision(17)<<"{\n  \"schema_version\": 1,\n  \"entry\": \"formal-world\",\n"
        <<"  \"backend\": \""<<backend<<"\",\n  \"world_id\": \""<<earth.world_id()<<"\",\n"
        <<"  \"resource_recipe_hash\": \""<<earth.resource_recipe_hash()<<"\",\n"
        <<"  \"width\": "<<width<<", \"height\": "<<height<<",\n"
        <<"  \"camera\": {\"orthographic\": false, \"height_m\": "<<camera.height_m
        <<", \"pitch_deg\": "<<camera.pitch_deg<<", \"yaw_deg\": "<<camera.yaw_deg
        <<", \"target_x_m\": "<<camera.target_x_m<<", \"target_z_m\": "<<camera.target_z_m<<"},\n"
        <<"  \"comparison\": \"same World, camera, UI and lighting; only one ground texture binding changes\",\n"
        <<"  \"frames\": [\"ground-full\", \"ground-mean-base\", \"ground-flat-normal\", \"ground-mean-arm\", \"ground-restored\"],\n"
        <<"  \"frame_file_sha256\": {";
    bool first=true;
    for(const char* name:{"ground-full","ground-mean-base","ground-flat-normal","ground-mean-arm","ground-restored"}) {
        const auto path=folder/(std::string(name)+".tga");
        if(!std::filesystem::is_regular_file(path))throw std::runtime_error("Formal ground screenshot callback did not produce "+std::string(name));
        file<<(first?"":",")<<"\n    \""<<name<<"\": \""<<sonnheide::earth::sha256_file(path)<<"\"";first=false;
    }
    file<<"\n  }\n}\n";
    if(!file)throw std::runtime_error("Cannot finish native ground camera evidence");
}
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
        bool smoke = false,phase1_smoke=false;
        std::filesystem::path saves,captures;
        std::filesystem::path root;
        for (int i=1; i<argc; ++i) {
            const std::string arg = argv[i];
            if (arg == "--smoke-test") smoke = true;
            else if (arg == "--phase1-smoke") phase1_smoke = true;
            else if (arg == "--capture-dir" && i+1 < argc) captures = std::filesystem::u8path(argv[++i]);
            else if (arg == "--saves" && i+1 < argc) saves = std::filesystem::u8path(argv[++i]);
            else if (arg == "--resources" && i+1 < argc) root = std::filesystem::u8path(argv[++i]);
            else throw std::runtime_error("Usage: Sonnheide [--resources PATH] [--saves PATH] [--smoke-test | --phase1-smoke] [--capture-dir PATH]");
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
        if (smoke||phase1_smoke) preference_path = root / "../../smoke-preferences.conf";
        else {
            char* path = SDL_GetPrefPath("Sonnreich", "Sonnheide");
            if (!path) throw std::runtime_error("Cannot locate user preferences directory");
            preference_path = std::filesystem::u8path(path) / "preferences.conf";
            SDL_free(path);
        }
        if(saves.empty())saves=(smoke||phase1_smoke)?root/"../../phase1-smoke-saves":preference_path.parent_path()/"saves";
        saves=std::filesystem::absolute(saves).lexically_normal();
        if(!captures.empty()) {
            if(!phase1_smoke)throw std::runtime_error("--capture-dir requires --phase1-smoke");
            captures=std::filesystem::absolute(captures);std::filesystem::create_directories(captures);
            // Missing/failed screenshots must never inherit a previous QA run.
            for(const char* name:{"ground-full.tga","ground-mean-base.tga","ground-flat-normal.tga","ground-mean-arm.tga","ground-restored.tga","ground-camera.json"})
                std::filesystem::remove(captures/name);
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
            sonnheide::client::NativeEarthPage earth(*context,root,saves);
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
            if (!earth.initialize(error)) throw std::runtime_error(error);
            menu.enable_world_navigation();
            SDL_StartTextInput(window.window());
            std::cout << "Sonnheide native renderer: " << renderer.renderer_name() << '\n';
            std::cout << "Logical " << width << 'x' << height << ", pixels " << pixels_w << 'x' << pixels_h << '\n';
            auto previous = std::chrono::steady_clock::now();
            bool running = true, previous_visible = false;
            unsigned frame=0,phase=0,phase_frames=0,ready_frames=0,stable_frames=0;std::string smoke_world;
            sonnheide::native::EarthCamera ground_camera;int ground_width=0,ground_height=0;
            const auto session_start=std::chrono::steady_clock::now();bool checked_continue=false;
            while (running) {
                SDL_Event event;
                while (SDL_PollEvent(&event)) {
                    if (event.type == SDL_EVENT_QUIT || event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED) running=false;
                    else if(!earth.handle_event(event,width,height))sonnheide::native::process_event(event,*context);
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
                menu.update(delta,visible&&!earth.visible());earth.update();
                if(!checked_continue&&earth.resources_ready()){menu.set_continue_available(earth.can_continue());checked_continue=true;}
                if(earth.consume_return()){menu.show(true);menu.set_continue_available(earth.can_continue());}
                if(auto navigation=menu.consume_navigation_request();!navigation.empty()){menu.show(false);earth.open(menu.preferences().locale,navigation!="new");}
                if (smoke) {
                    if (frame==10) menu.activate("settings");
                    if (frame==20) menu.activate("en");
                    if (frame==30) menu.activate("de");
                    if (frame==40) menu.activate("zh");
                    if (frame==50) menu.activate("back");
                    if (frame==60) menu.activate("new");
                    if (frame==70) earth.activate("back");
                    if (frame==80) menu.activate("load");
                    if (frame==90) earth.activate("back");
                    if (frame==120) menu.activate("quit");
                }
                if(phase1_smoke){
                    if(std::chrono::duration<double>(now-session_start).count()>180)throw std::runtime_error("Phase1 native smoke timed out at step "+std::to_string(phase));
                    if(!earth.last_error().empty())throw std::runtime_error("Phase1 native smoke: "+earth.last_error());
                    ++phase_frames;
                    auto next=[&]{++phase;phase_frames=0;stable_frames=0;};
                    const bool stable=(phase==2&&earth.preview_ready())||((phase==5||phase==7||phase==12)&&earth.in_world());
                    stable_frames=stable?stable_frames+1:0;
                    if(stable_frames==6) {
                        check_entry_camera(earth,phase==2?"Preview":phase==5?"World":phase==7?"Continue":"Load");
                        if(phase==5){ground_camera=earth.camera_state();ground_width=pixels_w;ground_height=pixels_h;}
                    }
                    if(phase==5&&stable_frames>=6&&!captures.empty()) {
                        if(!same_camera(ground_camera,earth.camera_state())||pixels_w!=ground_width||pixels_h!=ground_height)
                            throw std::runtime_error("Formal ground comparison camera or viewport changed during capture");
                        using Diagnostic=sonnheide::native::GroundDiagnostic;
                        if(stable_frames==12)earth.set_ground_diagnostic(Diagnostic::MeanBase);
                        else if(stable_frames==24)earth.set_ground_diagnostic(Diagnostic::FlatNormal);
                        else if(stable_frames==36)earth.set_ground_diagnostic(Diagnostic::MeanArm);
                        else if(stable_frames==48)earth.set_ground_diagnostic(Diagnostic::Full);
                    }
                    if(phase==1&&earth.resources_ready())++ready_frames;
                    if(phase==0&&frame>8){menu.activate("new");next();}
                    else if(phase==1&&earth.visible()&&earth.resources_ready()&&ready_frames>15){earth.activate("preview");next();}
                    else if(phase==2&&earth.preview_ready()&&stable_frames>10){if(auto* doc=context->GetDocument(1))if(auto* input=dynamic_cast<Rml::ElementFormControlInput*>(doc->GetElementById("earth-world-name")))input->SetValue("世界 · Sonnheide");earth.activate("dark");next();}
                    else if(phase==3&&phase_frames>8){earth.activate("light");next();}
                    else if(phase==4&&phase_frames>8){earth.activate("create");next();}
                    else if(phase==5&&earth.in_world()&&stable_frames>(captures.empty()?10U:60U)){smoke_world=earth.world_id();if(smoke_world.empty()||!earth.can_continue())throw std::runtime_error("Created World is not durably resumable");if(!captures.empty())write_ground_evidence(captures,earth,ground_camera,renderer.renderer_name(),ground_width,ground_height);earth.activate("back");next();}
                    else if(phase==6&&!earth.visible()&&phase_frames>8){menu.activate("continue");next();}
                    else if(phase==7&&earth.in_world()&&stable_frames>10){if(earth.world_id()!=smoke_world)throw std::runtime_error("Continue changed stable WorldId");earth.activate("back");next();}
                    else if(phase==8&&!earth.visible()&&phase_frames>8){menu.activate("settings");menu.activate("de");menu.activate("back");menu.activate("new");next();}
                    else if(phase==9&&earth.visible()&&earth.resources_ready()){earth.activate("preview");earth.activate("cancel");next();}
                    else if(phase==10&&!earth.busy()&&phase_frames>8){earth.activate("back");next();}
                    else if(phase==11&&!earth.visible()&&phase_frames>8){menu.activate("settings");menu.activate("en");menu.activate("back");menu.activate("load");next();}
                    else if(phase==12&&earth.in_world()&&stable_frames>10){if(earth.world_id()!=smoke_world)throw std::runtime_error("Cancelled candidate altered Continue");earth.activate("back");next();}
                    else if(phase==13&&!earth.visible()){std::cout<<"Phase1 native Metal/D3D11 scenario passed: perspective close-view Preview/World/Continue/Load, Light/Dark, durable create, cancelled candidate, three locales; WorldId="<<smoke_world<<'\n';if(!captures.empty())std::cout<<"Formal ground texture captures require tools/validate_ground_frames.py before claiming visible PBR contribution\n";break;}
                }
                if (menu.consume_preferences_changed()) {
                    window.fullscreen(menu.preferences().fullscreen);
                    menu.set_preferences_error(!sonnheide::client::save_preferences(preference_path,menu.preferences(),error));
                    if (!error.empty()) std::cerr << error << '\n';
                }
                if (menu.consume_quit_request()) break;
                if (!visible) { SDL_Delay(20); continue; }
                renderer.begin_frame(width,height,pixels_w,pixels_h);
                if(earth.visible())earth.draw(pixels_w,pixels_h);
                else {
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
                }
                context->Update(); renderer.begin_ui_frame(); context->Render();
                if(phase1_smoke&&!captures.empty()&&((phase==1&&ready_frames==10)||(phase==3&&phase_frames==6)||stable_frames==6)){const auto filename=path_utf8(captures/("step-"+std::to_string(phase)));bgfx::requestScreenShot(BGFX_INVALID_HANDLE,filename.c_str());}
                if(phase1_smoke&&!captures.empty()&&phase==5) {
                    // bgfx accepts only one screenshot per framebuffer each
                    // frame; step-5 already captures frame 6, so Full uses 7.
                    const char* name=stable_frames==7?"ground-full":stable_frames==18?"ground-mean-base":
                                     stable_frames==30?"ground-flat-normal":stable_frames==42?"ground-mean-arm":
                                     stable_frames==54?"ground-restored":nullptr;
                    if(name){const auto filename=path_utf8(captures/name);bgfx::requestScreenShot(BGFX_INVALID_HANDLE,filename.c_str());}
                }
                renderer.end_frame();
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
