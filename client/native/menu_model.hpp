#pragma once

#include <array>
#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>

namespace sonnheide::client {
enum class Locale { Chinese, English, German };
enum class Panel { None, NewWorld, LoadWorld, Settings };
struct Preferences {
    Locale locale = Locale::Chinese;
    bool reduced_motion = false;
    bool fullscreen = false;
    bool painting_paused = false;
    friend bool operator==(const Preferences&, const Preferences&) = default;
};
struct PaintingPose {
    int front = -1;
    int back = -1;
    double blend = 0;
    double x = 0; // fractions of viewport, bounded by .0055
    double y = 0; // fractions of viewport, bounded by .0040
    double front_scale = 1.018;
    double back_scale = 1.018;
};
// Presentation-only clock and RNG. Never receives World or simulation state.
class MenuModel {
public:
    explicit MenuModel(Preferences preferences = {}, std::uint64_t presentation_seed = 1);
    void set_painting_available(int index, bool available);
    void update(double seconds, bool visible);
    void open(Panel panel) { panel_ = panel; }
    void close() { panel_ = Panel::None; }
    Panel panel() const { return panel_; }
    void set_preferences(Preferences preferences) { preferences_ = preferences; }
    const Preferences& preferences() const { return preferences_; }
    PaintingPose presentation() const;
    void step_painting(int direction);
    std::string_view text(std::string_view key) const;
private:
    double random();
    void next_motion();
    int next_available(int from, int direction) const;
    std::array<bool, 5> available_{};
    Preferences preferences_;
    Panel panel_ = Panel::None;
    std::uint64_t rng_;
    int front_ = -1;
    int back_ = -1;
    double hold_ = 0;
    double fade_ = 0;
    double fade_duration_ = 12;
    int queued_manual_direction_ = 0;
    double front_age_ = 0;
    double back_age_ = 0;
    double motion_age_ = 0;
    double motion_duration_ = 140;
    double from_x_ = 0;
    double from_y_ = 0;
    double target_x_ = 0;
    double target_y_ = 0;
    int previous_direction_ = -1;
};
const char* locale_code(Locale locale);
Preferences load_preferences(const std::filesystem::path& path);
// Writes a sibling temporary file then renames; failure preserves the previous file.
bool save_preferences(const std::filesystem::path& path, const Preferences& preferences, std::string& error);
}
