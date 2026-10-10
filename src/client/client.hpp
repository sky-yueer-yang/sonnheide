#pragma once
#include <SDL3/SDL_events.h>
#include "../core/sonnheide.hpp"
#include <RmlUi/Core/RenderInterface.h>
#include <RmlUi/Core/SystemInterface.h>
#include <RmlUi/Core/FileInterface.h>
#include <array>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <memory>
#include <optional>
#include <string>
namespace Rml { class Context; }
namespace sonnheide::client {
struct Vec3 { double x{},y{},z{}; };
struct Ray { Vec3 origin,direction; };
struct SurfaceCell { float height_m{}; std::uint8_t kind{},theme{}; bool guard{},wet{}; };
struct SurfaceHit { Vec3 point,normal; int x{},z{}; };
// Immutable adapters must read the same core World/CreationPreview surface as
// support/navigation. This view never owns or changes simulation geometry.
struct SurfaceView {
    sonn::Uuid identity{};
    std::uint64_t revision{};
    int width{},height{};
    double origin_x{},origin_z{},cell_m{2};
    std::uint32_t profile{2};
    std::array<float,8> height_levels{};
    using FineVisitor=std::function<void(int,int)>;
    // Enumerate only a requested management rectangle; construction is O(1).
    std::function<void(int,int,int,int,const FineVisitor&)> enumerate_fine;
    std::function<SurfaceCell(int,int)> cell;
    std::function<sonn::Mesh(int,int,int,int)> mesh,coarse_mesh;
    std::function<SurfaceCell(double,double)> point;
    std::function<std::vector<SurfaceCell>(int,int)> fine;
    std::function<std::array<std::uint16_t,64>(int,int)> coverage_counts;
    std::function<std::optional<SurfaceHit>(const Ray&)> raycast;
};
SurfaceView surface_view(const sonn::World&);
enum class Age { Light,Darkness };
enum class MaterialDiagnostic { Full,MeanBase };
struct Camera {
    Vec3 target{0,2,0};
    double distance_m{48},yaw_deg{32},pitch_deg{32};
};
struct Dimensions { int logical_w{},logical_h{},pixel_w{},pixel_h{}; float density{1}; };
struct ClientConfig { std::filesystem::path resources; int width{1280},height{800}; bool fullscreen{}; };
class Client final {
public:
    explicit Client(const ClientConfig&);
    ~Client();
    Client(const Client&)=delete;Client& operator=(const Client&)=delete;
    Dimensions dimensions() const;
    bool poll_event(SDL_Event&);
    bool visible() const;
    bool focused() const;
    void fullscreen(bool);
    // First let UI consume input; only unconsumed world input reaches camera.
    bool process_ui_event(const SDL_Event&,Rml::Context&);
    bool process_camera_event(const SDL_Event&);
    void sync_text_input(Rml::Context&);
    void cancel_input();
    Camera& camera();
    const Camera& camera() const;
    void frame_surface(const SurfaceView&);
    Ray screen_ray(double logical_x,double logical_y) const;
    std::optional<SurfaceHit> pick(const SurfaceView&,double logical_x,double logical_y) const;
    void begin_frame(const SurfaceView* surface,Age age,double presentation_seconds=0);
    void begin_ui();
    void end_frame();
    void request_screenshot(const std::filesystem::path& basename);
    void material_diagnostic(MaterialDiagnostic);
    void set_map_rgba(int width,int height,std::span<const std::uint8_t> rgba,std::uint64_t revision);
    // Sets the scene viewport in SDL logical coordinates, for both 3D and map.
    void map_overlay(int logical_x,int logical_y,int logical_width,int logical_height,bool visible);
    std::string renderer_name() const;
    std::string gpu_parameters_json() const;
    Rml::RenderInterface* render_interface();
    Rml::SystemInterface* system_interface();
    Rml::FileInterface* file_interface();
    void audio_volume(float);
    bool audio_available() const;
    std::string audio_status() const;
private:
    struct Impl;std::unique_ptr<Impl> impl_;
};
}
