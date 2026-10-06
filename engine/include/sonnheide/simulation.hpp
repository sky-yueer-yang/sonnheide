#pragma once
#include "sonnheide/world.hpp"
#include <istream>
#include <ostream>

namespace sonnheide {
struct Stock { Quantity available{}; Quantity reserved{}; auto operator<=>(const Stock&) const = default; };
struct ReclamationJob { EntityId id; Cell cell; EntityId owner; Quantity material; Quantity work; JobPhase phase; };
struct KernelPort { EntityId id; KernelPlacePort geometry; };
struct Event { std::uint64_t sequence; std::string kind; EntityId entity; };
class Simulation {
public:
  static constexpr Quantity materialsPerCell = 10, workPerCell = 5;
  Simulation(FrozenWorld world, std::map<EntityId, Stock> initialStock);
  Result submit(const Command& command, AuthorityContext authority);
  void rebuild_navigation() { world_.rebuild_navigation(); }
  const FrozenWorld& world() const { return world_; }
  const std::map<EntityId, Stock>& stock() const { return stock_; }
  const std::map<EntityId, ReclamationJob>& jobs() const { return jobs_; }
  const std::vector<Event>& events() const { return events_; }
  std::uint64_t revision() const { return revision_; }
  std::uint64_t state_fingerprint() const;
  void validate() const;
  void save(std::ostream& out) const;
  static Simulation load(std::istream& in);
private:
  struct Receipt { std::string canonical; Principal principal; EntityId actor; Result result; };
  FrozenWorld world_;
  std::map<EntityId, Stock> stock_;
  std::map<EntityId, ReclamationJob> jobs_;
  std::map<EntityId, KernelPort> ports_;
  std::map<std::string, Receipt> receipts_;
  std::vector<Event> events_;
  std::uint64_t revision_{};
  EntityId nextId_{1};
  Result apply(const Payload& payload, AuthorityContext authority);
  void emit(const std::string& kind, EntityId entity);
};
void save_atomic(const Simulation& simulation, const std::string& path);
} // namespace sonnheide
