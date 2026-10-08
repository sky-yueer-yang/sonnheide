#pragma once
#include "sonnheide/earth_world.hpp"
#include <filesystem>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <thread>

namespace sonnheide::creation {
using DraftRevision = std::uint64_t;
struct CreationTicket { std::uint64_t request{}, generation{}; DraftRevision draft_revision{}; };
struct ResourceReadiness {
 std::string terrain_recipe_hash;
 // Hash of the compiled, admitted material and sky recipe. Provided by the native resource loader.
 std::string presentation_recipe_hash;
 bool terrain_ready{}, materials_ready{}, light_sky_ready{}, dark_sky_ready{};
};
enum class EnvironmentAge { Light, Darkness };
struct EmptyEntityCounts { std::uint64_t people{}, buildings{}, countries{}, tasks{}; };
struct WorldSession {
 std::string world_id;
 std::string display_name;
 std::uint64_t session_generation{}, tick{};
 EnvironmentAge age{EnvironmentAge::Light};
 bool age_automatic{};
 EmptyEntityCounts entities;
 std::shared_ptr<const earth::EarthWorldDefinition> definition;
 std::string presentation_recipe_hash;
};
enum class CreateStatus { Preparing, Published, Cancelled, Stale, Failed };
struct CreationResult {
 CreateStatus status{CreateStatus::Failed};
 std::string error;
 std::shared_ptr<const WorldSession> session;
 explicit operator bool() const { return status == CreateStatus::Published; }
};
enum class PersistenceStep {
 CheckpointOpen, CheckpointWrite, CheckpointFlush, CheckpointFileSync,
 CheckpointRename, CheckpointDirectorySync, CheckpointReadback,
 PointerOpen, PointerWrite, PointerFlush, PointerFileSync,
 PointerRename, PointerDirectorySync
};
// Native UI calls only from the constructing thread. Workers may prepare geometry
// but never publish sessions, allocate WorldIds, or mutate the continue pointer.
class CreateCoordinator {
public:
 CreateCoordinator(std::filesystem::path saves, std::shared_ptr<const earth::GeoAtlas> atlas,
                   std::string admitted_presentation_recipe_hash);
 ~CreateCoordinator();
 CreationTicket begin(DraftRevision draft_revision, std::string display_name = "Sonnheide");
 void cancel(CreationTicket ticket);
 void invalidate_draft(DraftRevision revision);
 CreationResult start(CreationTicket ticket,
                       std::shared_ptr<const earth::EarthWorldDefinition> candidate,
                       const ResourceReadiness& readiness);
 std::optional<CreationResult> poll();
 bool busy() const;
 CreationResult create(CreationTicket ticket,
                       std::shared_ptr<const earth::EarthWorldDefinition> candidate,
                       const ResourceReadiness& readiness);
 // Read-only preparation is safe on a worker. It never publishes or allocates an identity.
 std::shared_ptr<const WorldSession> prepare_continue(const std::function<bool()>& cancelled = {}) const;
 CreationResult publish_continue(std::shared_ptr<const WorldSession> prepared, const ResourceReadiness& readiness, CreationTicket ticket = {});
 CreationResult load_continue(const ResourceReadiness& readiness);
 std::shared_ptr<const WorldSession> active() const { return active_; }
 bool can_continue() const;
 const std::filesystem::path& save_directory() const { return saves_; }
 // Tests throw from this hook before the named operation. Never a gameplay API.
 void set_fault_injector(std::function<void(PersistenceStep)> injector);
private:
 struct Pending;
 std::unique_ptr<Pending> pending_;
 void require_writer() const;
 void fault(PersistenceStep step) const;
 std::string validate_readiness(const earth::EarthWorldDefinition&, const ResourceReadiness&) const;
 bool current(CreationTicket) const;
 std::filesystem::path saves_;
 std::shared_ptr<const earth::GeoAtlas> atlas_;
 std::string presentation_hash_;
 std::thread::id writer_;
 std::uint64_t next_request_{}, next_generation_{};
 CreationTicket current_{};
 std::string draft_name_;
 bool cancelled_{};
 std::shared_ptr<const WorldSession> active_;
 std::function<void(PersistenceStep)> injector_;
};
}
