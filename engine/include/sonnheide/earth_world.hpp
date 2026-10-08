#pragma once
#include <cstdint>
#include <filesystem>
#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace sonnheide::earth {
// Independent byte admission shared by geography and presentation resource loaders.
std::string sha256_file(const std::filesystem::path& path);
struct Point { double x{}, y{}; };
struct LonLat { double longitude{}, latitude{}; };
struct Bounds { double min_x{}, min_y{}, max_x{}, max_y{}; };
// One spherical AEQD map, metres in both horizontal axes. Inverse and forward
// reject the antipodal singularity instead of returning deceptively finite maps.
class Projection {
public:
 explicit Projection(LonLat origin);
 Point forward(LonLat geographic) const;
 LonLat inverse(Point metres) const;
 LonLat origin() const { return origin_; }
private: LonLat origin_; double phi_{}, lambda_{};
};
struct Ring {
 std::int32_t id{}, container{-1}; std::uint8_t level{};
 double west{}, east{}, south{}, north{};
 std::vector<LonLat> vertices;
};
enum class Detail { Full, High, Intermediate, Low, Coarse };
class GeoAtlas {
public:
 // Full geometry is always used by surface(). LOD is only for global navigation.
 // Directory must be admitted by prepare_geography.py --fetch first.
 static std::shared_ptr<GeoAtlas> load(const std::filesystem::path& directory);
 const std::vector<Ring>& rings(Detail level) const;
 bool land(LonLat position) const;
 const std::string& source_hash() const { return source_hash_; }
private:
 struct Edge { double x1,y1,x2,y2; std::int32_t ring; std::uint8_t level; };
 std::vector<std::vector<Edge>> latitude_edges_;
 std::vector<Ring> full_, high_, intermediate_, low_, coarse_;
 std::string source_hash_;
};
struct WorldConfig {
 LonLat centre{8.55,47.37};
 double width_m{4096}, height_m{4096};
 double world_scale{1}; // Both axes, physical texture coordinates and displayed metres.
 double closure_band_m{128}, sea_collar_m{512}, sea_guard_m{64};
 double coast_width_m{24}, land_height_m{2}, water_depth_m{4};
 std::uint32_t grid_cells_x{128}, grid_cells_y{128};
 std::uint64_t seed{1};
};
struct SurfaceSample {
 bool source_land{}, land{}, guard{};
 double height_m{}, coast_distance_m{};
 std::uint32_t water_region{}; // 0 land, positive connected water; outside is presentation-only.
 bool authoritative{};
};
struct Cell { bool source_land{}, land{}, guard{}; double height_m{}; std::uint32_t water_region{}; };
class EarthWorldDefinition {
public:
 static std::shared_ptr<const EarthWorldDefinition> create(std::shared_ptr<const GeoAtlas> atlas,
                                                          const WorldConfig& config,
                                                          const std::function<bool()>& cancelled = {});
 // Highest preview and game terrain use this same frozen full-source sampler.
 // Outside the domain has sea appearance but is never an authoritative cell.
 SurfaceSample sample(Point game_metres) const;
 LonLat geographic(Point game_metres) const;
 Bounds selection_bounds() const { return selection_; }
 Bounds domain_bounds() const { return domain_; }
 const WorldConfig& config() const { return config_; }
 const std::vector<Cell>& cells() const { return cells_; }
 std::uint32_t columns() const { return columns_; }
 std::uint32_t rows() const { return rows_; }
 double cell_width_m() const { return dx_; }
 double cell_height_m() const { return dy_; }
 const std::string& source_hash() const;
 const std::string& recipe_hash() const { return recipe_hash_; }
 std::uint32_t water_region_count() const { return water_regions_; }
private:
 EarthWorldDefinition(std::shared_ptr<const GeoAtlas> atlas, WorldConfig config,
                      const std::function<bool()>& cancelled);
 bool closed_land(Point p, bool source) const;
 double height_at(Point p, bool land) const;
 double coast_distance(Point p) const;
 std::shared_ptr<const GeoAtlas> atlas_; WorldConfig config_; Projection projection_;
 struct ShoreSegment { Point a,b; };
 struct WaterRegion { std::uint32_t id{}; std::uint8_t level{}; std::vector<ShoreSegment> edges; };
  bool source_land_at(Point p) const;
 Bounds selection_, domain_; std::vector<Cell> cells_; std::vector<ShoreSegment> shore_segments_;
 std::vector<std::vector<std::uint32_t>> segment_bins_;
 std::vector<WaterRegion> closed_water_;
 Point source_reference_{}; bool source_reference_land_{};
 std::uint32_t columns_{}, rows_{}, water_regions_{}; double dx_{},dy_{};
 std::string recipe_hash_;
};
}
