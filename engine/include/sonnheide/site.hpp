#pragma once
// Historical ADR 0003 slope oracle only. ADR 0010 uses flat inland ground and
// optional coastal support. This interface is not part of the current kernel.
#include "sonnheide/types.hpp"
#include <limits>
#include <map>
#include <optional>
#include <vector>

namespace sonnheide::site {
using Height = std::int64_t; // Absolute millimetres in the frozen world datum.
inline constexpr Height missingHeight = std::numeric_limits<Height>::min();
struct Rect {
  int x0{}, z0{}, x1{}, z1{}; // Grid-aligned, half-open cell rectangle.
  auto operator<=>(const Rect&) const = default;
};
bool overlaps(Rect a, Rect b);
bool contains(Rect outer, Rect inner);

// Immutable reference surface: each cell is two planar triangles, NW-SE diagonal.
// Source DEM resolution/uncertainty remains a separate provenance fact.
// Queries safely reject an unavailable (for example, moved-from) grid.
class TerrainGrid {
public:
  TerrainGrid(int width, int depth, Height cellSizeMm,
              std::vector<Surface> natural, std::vector<Height> vertices);
  int width() const { return width_; }
  int depth() const { return depth_; }
  Height cell_size_mm() const { return cellSize_; }
  bool valid(Rect rectangle) const;
  bool all_land(Rect rectangle) const;
  Height vertex(int x, int z) const;
  std::optional<std::pair<Height,Height>> extrema(Rect rectangle) const;
  bool ground_walkable(Rect rectangle, Height maxRiseNumerator, Height runDenominator) const;
  std::uint64_t fingerprint() const; // Regression fingerprint, not a source SHA-256.
private:
  int width_, depth_;
  Height cellSize_;
  std::vector<Surface> natural_;
  std::vector<Height> vertices_;
  bool storage_valid() const noexcept;
};

enum class Mode { Pedestrian, Vehicle };
enum class Facing { North, East, South, West };
struct Entrance {
  Mode mode;
  Height xMm, zMm; // Absolute boundary position; floor Y is the foundation top.
  Facing outward;
};
struct MotionProfile {
  Mode mode;
  Height widthMm, clearanceMm;
  Height maxGradeNumerator, maxGradeDenominator;
  Height wheelbaseMm{}, allowableSagMm{};
  auto operator<=>(const MotionProfile&) const = default;
};
MotionProfile pedestrian_profile();
MotionProfile vehicle_profile();
struct RoadPortal {
  EntityId id;
  Rect area;
  Height heightMm;
  EntityId connectedNetworkId; // Trusted, previously certified network input.
};
struct Ramp {
  std::size_t entranceIndex;
  EntityId portalId;
  Rect area;
  MotionProfile profile;
};
struct SiteRequest {
  EntityId id, actor;
  std::uint64_t expectedRevision;
  Rect footprint, constructionEnvelope, deliveryApron, deliveryRoute;
  EntityId deliveryPortalId;
  std::vector<Entrance> entrances;
  std::vector<Ramp> approaches;
  Height floorClearanceMm{150}, embedMm{300}, maxFoundationExposureMm{6000};
  std::optional<Height> requestedFloorMm;
  bool requiresVehicleEntrance{false}; // From trusted building definition, not the renderer.
  Facing deliveryFront{Facing::North}; // From the building definition; rotates with it.
  // World mm along the front edge: X for North/South, Z for East/West.
  // Defaults to the footprint's front-edge midpoint. The socket is on the
  // constructionEnvelope front edge, on existing ground outside the enclosure.
  std::optional<Height> deliverySocketAcrossMm;
};
struct SitePlan {
  SiteRequest request;
  Height bottomMm, floorMm;
  std::uint64_t terrainFingerprint;
  std::vector<Rect> protectedAreas; // Conservative 2.5D: no over/under building.
};
struct PlanResult {
  std::string reason; // Empty means a valid reference plan.
  std::optional<SitePlan> plan;
  explicit operator bool() const { return plan.has_value(); }
};

// Geometry/rights oracle, not a production building command, ledger or save format.
// Successful reservations preserve every previously registered approach and portal.
// A portal/network ID is a trusted input; the global network is not proved here.
class SiteRegistry {
public:
  SiteRegistry(TerrainGrid terrain, std::vector<EntityId> landOwners,
               std::vector<RoadPortal> connectedPortals);
  PlanResult preview(const SiteRequest& request) const;
  PlanResult reserve(const SiteRequest& request);
  const TerrainGrid& terrain() const { return terrain_; }
  const std::map<EntityId,SitePlan>& sites() const { return sites_; }
  std::uint64_t revision() const { return revision_; }
private:
  TerrainGrid terrain_;
  std::vector<EntityId> owners_;
  std::map<EntityId,RoadPortal> portals_;
  std::map<EntityId,SitePlan> sites_;
  std::uint64_t revision_{0};
  bool owned(Rect area, EntityId actor) const;
};
} // namespace sonnheide::site
