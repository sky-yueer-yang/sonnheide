#include "menu_model.hpp"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <system_error>

namespace sonnheide::client {
namespace {
double smooth(double t) { t = std::clamp(t, 0., 1.); return t*t*t*(t*(t*6.-15.)+10.); }
struct Message { const char* key; const char* zh; const char* en; const char* de; };
constexpr Message messages[] = {
 {"new","创建世界","New world","Neue Welt"}, {"continue","继续","Continue","Fortsetzen"},
 {"load","载入世界","Load world","Welt laden"}, {"settings","设置","Settings","Einstellungen"},
 {"quit","退出","Quit","Beenden"}, {"back","返回","Back","Zurück"},
 {"language","语言","Language","Sprache"}, {"motion","油画动画","Painting animation","Gemäldeanimation"},
 {"reduce","减弱动画","Reduce motion","Bewegung reduzieren"}, {"full","全屏","Fullscreen","Vollbild"},
 {"window","窗口","Windowed","Fenster"}, {"play","播放","Play","Abspielen"}, {"pause","暂停","Pause","Pause"},
 {"previous","上一幅","Previous","Zurück"}, {"next","下一幅","Next","Weiter"},
 {"normal","完整动画","Full motion","Volle Bewegung"},
 {"newbody","缺少有效的真实地形区域包，暂时无法创建世界。","A valid real-terrain region package is required to create a world.","Zum Erstellen einer Welt wird ein gültiges Paket mit realem Gelände benötigt."},
 {"newdetail","安装有效区域包后，可以选择区域并创建世界。","Install a valid region package to select a region and create a world.","Installiere ein gültiges Gebietspaket, um ein Gebiet auszuwählen und eine Welt zu erstellen."},
 {"loadbody","暂无可载入的世界存档。","No saved world is available.","Es ist kein Weltspielstand vorhanden."},
 {"loaddetail","保存世界后，可以从这里继续游玩。","After saving a world, return here to continue playing.","Nach dem Speichern einer Welt kannst du hier weiterspielen."},
 {"preferror","偏好保存失败；设置在本次运行中仍有效。","Preferences could not be saved; settings still apply during this run.","Die Einstellungen konnten nicht gespeichert werden; sie gelten weiterhin für diesen Programmstart."}
};
}
MenuModel::MenuModel(Preferences preferences, std::uint64_t seed) : preferences_(preferences), rng_(seed ? seed : 1) { next_motion(); }
double MenuModel::random() { rng_ ^= rng_ >> 12; rng_ ^= rng_ << 25; rng_ ^= rng_ >> 27; return static_cast<double>((rng_ * UINT64_C(2685821657736338717)) >> 11) / 9007199254740992.; }
void MenuModel::next_motion() {
    constexpr int dx[] = {-1, 1, 1, -1}; constexpr int dy[] = {1, -1, 1, -1};
    std::array<int,4> candidates{}; int count = 0;
    for (int i=0; i<4; ++i) if (i != previous_direction_ && .0055-dx[i]*from_x_ >= .00075 && .004-dy[i]*from_y_ >= .00055) candidates[count++] = i;
    const int direction = candidates[std::min(count-1, static_cast<int>(random()*count))];
    previous_direction_ = direction;
    target_x_ = from_x_ + dx[direction]*(.0055-dx[direction]*from_x_)*(.35+random()*.4);
    target_y_ = from_y_ + dy[direction]*(.004-dy[direction]*from_y_)*(.35+random()*.4);
    motion_duration_ = 140+random()*60;
}
int MenuModel::next_available(int from, int direction) const {
    for (int step=1; step<=5; ++step) { const int index = (from+direction*step+50)%5; if (available_[index]) return index; }
    return -1;
}
void MenuModel::set_painting_available(int index, bool available) {
    if (index < 0 || index >= 5) return;
    available_[index] = available;
    if (!available && back_ == index) {
        back_=-1; fade_=0; hold_=0;
        const int queued=queued_manual_direction_; queued_manual_direction_=0;
        if (queued != 0) step_painting(queued);
    }
    if (front_ < 0 || (!available && front_ == index)) { front_ = next_available(index, 1); front_age_=0; hold_=0; back_=-1; fade_=0; queued_manual_direction_=0; }
}
void MenuModel::update(double seconds, bool visible) {
    if (!visible || preferences_.reduced_motion || preferences_.painting_paused || !std::isfinite(seconds) || seconds <= 0) return;
    // Event stalls cannot cause a jump. The application's visibility boundary resets its clock.
    seconds=std::min(seconds, .25);
    motion_age_ += seconds;
    while (motion_age_ >= motion_duration_) { motion_age_-=motion_duration_; from_x_=target_x_; from_y_=target_y_; next_motion(); }
    front_age_ = std::min(240., front_age_+seconds);
    if (back_ >= 0) {
        back_age_ = std::min(240., back_age_+seconds); fade_ += seconds;
        if (fade_ + 1e-9 >= fade_duration_) {
            front_=back_; front_age_=back_age_; back_=-1; fade_=0; hold_=0;
            const int queued=queued_manual_direction_; queued_manual_direction_=0;
            if (queued != 0) step_painting(queued);
        }
    } else if (front_ >= 0) {
        hold_ += seconds;
        if (hold_ >= 68) { const int candidate=next_available(front_,1); if (candidate >= 0 && candidate != front_) { back_=candidate; back_age_=0; fade_=0; fade_duration_=12; } else hold_=0; }
    }
}
PaintingPose MenuModel::presentation() const {
    const double fraction=smooth(motion_age_/motion_duration_);
    return {front_,back_,back_ >= 0 ? smooth(fade_/fade_duration_) : 0, from_x_+(target_x_-from_x_)*fraction,from_y_+(target_y_-from_y_)*fraction,1.018+.018*smooth(front_age_/240),1.018+.018*smooth(back_age_/240)};
}
void MenuModel::step_painting(int direction) {
    direction=direction < 0 ? -1 : 1;
    const bool static_change=preferences_.reduced_motion || preferences_.painting_paused;
    if (back_ >= 0 && !static_change) {
        // Two decoded layers cannot replace a mixed image with a third image without
        // a jump. Finish the current fade; retain only the latest requested direction.
        queued_manual_direction_=direction;
        return;
    }
    if (static_change && back_ >= 0) {
        if (fade_/fade_duration_ >= .5) { front_=back_; front_age_=back_age_; }
        back_=-1; fade_=0;
    }
    queued_manual_direction_=0;
    const int candidate=next_available(front_ < 0 ? 2 : front_,direction);
    if (candidate < 0 || candidate == front_) return;
    hold_=0;
    if (static_change || front_ < 0) { front_=candidate; front_age_=0; back_=-1; fade_=0; }
    else { back_=candidate; back_age_=0; fade_=0; fade_duration_=.9; }
}
std::string_view MenuModel::text(std::string_view key) const { for (const auto& entry:messages) if (entry.key == key) return preferences_.locale == Locale::English ? entry.en : preferences_.locale == Locale::German ? entry.de : entry.zh; return key; }
const char* locale_code(Locale locale) { return locale == Locale::English ? "en" : locale == Locale::German ? "de" : "zh"; }
Preferences load_preferences(const std::filesystem::path& path) {
    Preferences p; std::ifstream input(path); std::string line; int count=0;
    while (count++ < 12 && std::getline(input,line)) {
        if (line == "locale=en") p.locale=Locale::English; else if (line == "locale=de") p.locale=Locale::German; else if (line == "locale=zh") p.locale=Locale::Chinese;
        else if (line == "reduced_motion=1") p.reduced_motion=true; else if (line == "fullscreen=1") p.fullscreen=true; else if (line == "painting_paused=1") p.painting_paused=true;
    }
    return p;
}
bool save_preferences(const std::filesystem::path& path,const Preferences& p,std::string& error) {
    std::error_code ec; std::filesystem::create_directories(path.parent_path(),ec); if (ec) { error=ec.message(); return false; }
    auto temporary=path; temporary += ".tmp";
    { std::ofstream output(temporary,std::ios::trunc); if (!output) { error="Cannot open preference temporary file"; return false; }
      output << "schema=1\nlocale=" << locale_code(p.locale) << "\nreduced_motion=" << p.reduced_motion << "\nfullscreen=" << p.fullscreen << "\npainting_paused=" << p.painting_paused << '\n';
      output.flush(); if (!output) { error="Cannot write preferences"; output.close(); std::filesystem::remove(temporary,ec); return false; } }
    std::filesystem::rename(temporary,path,ec); if (ec) { error=ec.message(); std::filesystem::remove(temporary,ec); return false; }
    error.clear(); return true;
}
}
