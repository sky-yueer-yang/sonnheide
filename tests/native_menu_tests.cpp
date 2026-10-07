#include "menu_model.hpp"
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>

using namespace sonnheide::client;
namespace {
void require(bool condition,const char* why) { if (!condition) throw std::runtime_error(why); }
void advance(MenuModel& menu,double seconds) { for (int i=0;i<static_cast<int>(seconds*4);++i) menu.update(.25,true); }
void check_pose(const PaintingPose& p) { require(std::abs(p.x)<=.0055 && std::abs(p.y)<=.004,"Motion remains inside exact crop envelope"); require(p.front_scale>=1.018 && p.front_scale<=1.036 && p.back_scale>=1.018 && p.back_scale<=1.036,"Zoom remains in bounded envelope"); }
}
int main() {
 try {
    MenuModel manual({},71);
    for (int index=0;index<5;++index) manual.set_painting_available(index,true);
    manual.step_painting(1);
    require(manual.presentation().front == 0 && manual.presentation().back == 1 && manual.presentation().blend == 0,"Manual change starts with old decoded front intact");
    manual.update(.25,true); manual.update(.20,true);
    require(std::abs(manual.presentation().blend-.5)<1e-12,"Manual transition reaches half blend at 450ms");
    const auto mixed=manual.presentation(); manual.step_painting(1); manual.step_painting(-1);
    require(manual.presentation().front == mixed.front && manual.presentation().back == mixed.back && manual.presentation().blend == mixed.blend,"Repeated manual requests retain current blended pixels");
    manual.update(.25,true); manual.update(.20,true);
    require(manual.presentation().front == 1 && manual.presentation().back == 0 && manual.presentation().blend == 0,"At 900ms last queued direction starts from completed old transition");
    manual.update(.25,true); manual.update(.25,true); manual.update(.25,true); manual.update(.149,true);
    require(manual.presentation().front == 1 && manual.presentation().back == 0,"Manual transition remains pending before 900ms boundary");
    manual.update(.002,true);
    require(manual.presentation().front == 0 && manual.presentation().back == -1,"Manual transition completes after exact 900ms duration");
    advance(manual,67.75); require(manual.presentation().back == -1,"Manual completion restarts full automatic hold");
    manual.update(.25,true); require(manual.presentation().back == 1,"Automatic change still starts after 68-second hold");
    advance(manual,6); require(std::abs(manual.presentation().blend-.5)<1e-12,"Automatic transition retains its independent 12-second duration");
    manual.step_painting(1); manual.set_painting_available(1,false);
    require(manual.presentation().front == 0 && manual.presentation().back == 2 && manual.presentation().blend == 0,"Failed mixed candidate preserves old front and queued request skips unavailable image");
    manual.set_painting_available(2,false);
    require(manual.presentation().front == 0 && manual.presentation().back == -1,"Failed manual candidate leaves valid old front untouched");
    auto static_preferences=manual.preferences(); static_preferences.reduced_motion=true; manual.set_preferences(static_preferences); manual.step_painting(1);
    require(manual.presentation().front == 3 && manual.presentation().back == -1,"Reduced-motion manual changes are static and skip failed candidates");
    MenuModel one_image; one_image.set_painting_available(2,true); one_image.step_painting(1);
    require(one_image.presentation().front == 2 && one_image.presentation().back == -1,"Single decoded image does not start a meaningless fade");
    MenuModel model({},731); require(model.presentation().front == -1,"No image is falsely available before decoding");
    model.set_painting_available(2,true); model.set_painting_available(3,true); model.set_painting_available(4,true);
    require(model.presentation().front == 2,"Decoded first image retained");
    advance(model,67.75); require(model.presentation().back == -1,"Hold stays exactly 68 seconds");
    model.update(.25,true); require(model.presentation().back == 3 && model.presentation().blend == 0,"Decoded next image starts fade at boundary");
    advance(model,6); require(model.presentation().front == 2 && std::abs(model.presentation().blend-.5)<1e-12,"Half fade still retains old image");
    model.set_painting_available(3,false); require(model.presentation().front == 2 && model.presentation().back == -1,"Failed candidate never destroys previous painting");
    advance(model,68); require(model.presentation().back == 4,"Failed candidate skipped on next transition");
    advance(model,12); require(model.presentation().front == 4 && model.presentation().back == -1,"Old front switches only after completed 12 second fade");
    const auto held=model.presentation(); advance(model,5); const auto before_hidden=model.presentation();
    for (int i=0;i<1000;++i) model.update(.25,false);
    require(model.presentation().x == before_hidden.x && model.presentation().front_scale == before_hidden.front_scale,"Hidden window freezes both motion and zoom");
    auto prefs=model.preferences(); prefs.reduced_motion=true; model.set_preferences(prefs); advance(model,1000);
    require(model.presentation().x == before_hidden.x && model.presentation().front_scale == before_hidden.front_scale,"Reduced motion freezes all autonomous painting state");
    model.step_painting(-1); require(model.presentation().front == 2,"Manual image changes still work under reduced motion");
    prefs.reduced_motion=false; prefs.painting_paused=true; model.set_preferences(prefs); const auto paused=model.presentation(); advance(model,100);
    require(model.presentation().x == paused.x,"Manual pause freezes path");
    prefs.painting_paused=false; model.set_preferences(prefs);
    for (int i=0;i<40000;++i) { model.update(.25,true); check_pose(model.presentation()); }
    require(held.front_scale > 1.018,"Actual slow zoom changes image scale");
    model.set_painting_available(2,false); model.set_painting_available(4,false); require(model.presentation().front == -1,"All-image failure reaches honest fallback");
    model.set_painting_available(1,true); require(model.presentation().front == 1,"Recovery from all-image failure works");
    for (const auto locale:{Locale::Chinese,Locale::English,Locale::German}) { prefs.locale=locale; model.set_preferences(prefs); require(model.text("new") != "new" && !model.text("newbody").empty(),"All locales include both actions and required business explanations"); }
    model.open(Panel::NewWorld); require(model.panel() == Panel::NewWorld,"New-world opens actual application panel"); model.close(); require(model.panel() == Panel::None,"Cancel returns without inventing a world");
    const auto directory=std::filesystem::temp_directory_path()/"sonnheide-native-menu-test";
    std::filesystem::remove_all(directory); const auto file=directory/"preferences.cfg"; std::string error;
    require(save_preferences(file,prefs,error),"Preferences persist successfully"); require(load_preferences(file) == prefs,"Three-language settings survive restart");
    const auto old=load_preferences(file); std::filesystem::create_directory(directory/"preferences.cfg.tmp");
    prefs.locale=Locale::Chinese;
    require(!save_preferences(file,prefs,error),"Failure is surfaced instead of falsely reporting persistence"); require(load_preferences(file) == old,"Failed replacement preserves previous preference file");
    std::filesystem::remove_all(directory);
    std::cout << "Native menu presentation, failure-recovery, state and preference invariants passed\n";
 } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
