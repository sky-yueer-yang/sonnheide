#pragma once
#include "earth_camera.hpp"
#include "menu_model.hpp"
#include <RmlUi/Core/EventListener.h>
#include <SDL3/SDL_events.h>
#include <filesystem>
#include <memory>
#include <string>
namespace Rml { class Context; class Event; }
namespace sonnheide::client {
class NativeEarthPage final : public Rml::EventListener {
public:
 NativeEarthPage(Rml::Context&,std::filesystem::path resources,std::filesystem::path saves);
 ~NativeEarthPage() override;
 bool initialize(std::string& error);
 void open(Locale locale,bool continue_saved=false);
 void update();
 bool handle_event(const SDL_Event&,int width,int height);
 void draw(int pixel_width,int pixel_height);
 void activate(const std::string& action);
 void ProcessEvent(Rml::Event&) override;
 bool visible() const;
 bool consume_return();
 bool can_continue() const;
 bool in_world() const;
 bool preview_ready() const;
 bool busy() const;
 bool resources_ready() const;
 std::string last_error() const;
 std::string world_id() const;
 native::EarthCamera camera_state() const;
 std::string resource_recipe_hash() const;
 void set_ground_diagnostic(native::GroundDiagnostic);
private:
 struct Impl;std::unique_ptr<Impl> impl_;
};
}
