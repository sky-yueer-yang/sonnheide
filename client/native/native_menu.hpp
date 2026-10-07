#pragma once
#include "menu_model.hpp"
#include <RmlUi/Core/EventListener.h>
#include <filesystem>
#include <string>
namespace Rml { class Context; class ElementDocument; class Event; }
namespace sonnheide::client {
// A native RmlUi document. Background geometry is rendered by bgfx from presentation().
class NativeMenu final : public Rml::EventListener {
public:
    NativeMenu(Rml::Context& context, std::filesystem::path asset_root, Preferences preferences = {});
    ~NativeMenu() override;
    bool initialize(std::string& error);
    void update(double seconds, bool visible);
    void ProcessEvent(Rml::Event& event) override;
    void close_panel();
    bool consume_quit_request();
    bool consume_preferences_changed();
    const Preferences& preferences() const { return model_.preferences(); }
    PaintingPose presentation() const { return model_.presentation(); }
    void set_painting_available(int index, bool available) { model_.set_painting_available(index,available); }
    void set_preferences_error(bool failed);
    // Useful for an automated native event scenario; performs the same handler as a real click.
    void activate(const std::string& action);
    Panel panel() const { return model_.panel(); }
private:
    void refresh();
    void layout();
    void focus_first();
    Rml::Context& context_;
    std::filesystem::path asset_root_;
    Rml::ElementDocument* document_ = nullptr;
    MenuModel model_;
    bool quit_ = false;
    bool changed_ = false;
    bool preference_failure_ = false;
    int last_width_ = -1;
    int last_height_ = -1;
    float last_density_ = -1;
};
}
