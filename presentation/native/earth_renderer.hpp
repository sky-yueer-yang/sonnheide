#pragma once
#include <sonnheide/earth_world.hpp>
#include "earth_camera.hpp"
#include <filesystem>
#include <memory>
#include <vector>
namespace sonnheide::native {
// bgfx must already be initialized by BgfxRenderInterface; destroy before it.
class EarthRenderer {
public:
 explicit EarthRenderer(const std::filesystem::path& resource_root);
 ~EarthRenderer();
 EarthRenderer(const EarthRenderer&)=delete;
 EarthRenderer& operator=(const EarthRenderer&)=delete;
 void set_world(std::shared_ptr<const earth::EarthWorldDefinition> world);
 void draw(const EarthCamera& camera,bool darkness,int pixel_width,int pixel_height,int material_override=-1,GroundDiagnostic diagnostic=GroundDiagnostic::Full);
 void draw_atlas(const earth::GeoAtlas& atlas,double longitude,double latitude,double longitude_span,int pixel_width,int pixel_height,earth::LonLat selected_centre,double selection_width_real_m,double selection_height_real_m);
 void clear_world();
 bool ready() const;
 std::string resource_recipe_hash() const;
 std::vector<std::string> compatible_presentation_recipes() const;
 const char* material_name(int index) const;
 static constexpr int material_count=8;
private:
 struct Impl; std::unique_ptr<Impl> impl_;
};
}
