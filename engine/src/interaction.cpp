#include "sonnheide/interaction.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <new>
#include <stdexcept>
#include <type_traits>

namespace sonnheide::interaction {
namespace {
constexpr std::uint64_t maxQuantity = 1000000000000ULL;
constexpr std::uint64_t maxDays = 360000000ULL;
constexpr std::size_t maxRecords = 10000, maxCommands = 100000;
bool valid_kind(Kind k) { return k >= Kind::Person && k <= Kind::HistoryEvent; }
bool valid_name(const std::string& name) {
  return !name.empty() && name.size() <= 128 && std::none_of(name.begin(), name.end(), [](unsigned char c) { return c < 32 || c == 127; });
}
bool valid_position(Position p) { return p.x >= -1000000000 && p.x <= 1000000000 && p.z >= -1000000000 && p.z <= 1000000000; }
bool building(Kind k) { return k == Kind::Building || k == Kind::Port || k == Kind::Facility; }
bool physical(Kind k) { return k == Kind::Garment || k == Kind::StockBatch; }
bool rename_allowed(Kind k) {
  switch (k) {
  case Kind::Person: case Kind::Household: case Kind::Settlement: case Kind::City:
  case Kind::State: case Kind::Empire: case Kind::Alliance: case Kind::Culture:
  case Kind::Language: case Kind::Religion: case Kind::Denomination: case Kind::Church:
  case Kind::Enterprise: case Kind::Building: case Kind::Port: case Kind::Army:
  case Kind::Vehicle: case Kind::Project: case Kind::ProductionLine: case Kind::Facility:
  case Kind::Plant: case Kind::MineralDeposit: case Kind::Road: case Kind::World:
    return true;
  default: return false;
  }
}
bool has(const Record& r, Relation relation, EntityRef target) {
  return std::find(r.links.begin(), r.links.end(), Link{relation, target}) != r.links.end();
}
bool role_valid(Kind source, Relation role, Kind target) {
  switch (role) {
  case Relation::Nationality: return source == Kind::Person && target == Kind::State;
  case Relation::Household: return source == Kind::Person && target == Kind::Household;
  case Relation::Home: return source == Kind::Person && building(target);
  case Relation::Employer: return source == Kind::Person && target == Kind::Enterprise;
  case Relation::Language: return source == Kind::Person && target == Kind::Language;
  case Relation::Culture: return source == Kind::Person && target == Kind::Culture;
  case Relation::Religion: return source == Kind::Person && target == Kind::Religion;
  case Relation::Partner: case Relation::Parent: return source == Kind::Person && target == Kind::Person;
  case Relation::Author: return (source == Kind::Document || source == Kind::Innovation || source == Kind::NameRecord) && target == Kind::Person;
  case Relation::Owner: case Relation::Controller: case Relation::Custodian:
    return target == Kind::Person || target == Kind::State || target == Kind::Enterprise || target == Kind::Household || target == Kind::Church;
  case Relation::Location: return physical(source) && (building(target) || target == Kind::Person || target == Kind::Vehicle);
  case Relation::AccessRoad: return building(source) && target == Kind::Road;
  case Relation::Related: return valid_kind(source) && valid_kind(target) && target != Kind::MapMarker;
  }
  return false;
}
bool valid_state(const Snapshot& s) {
  if (!s.world || s.cells.empty() || s.cells.size() > maxRecords || s.records.size() > maxRecords || s.day > maxDays || !std::isfinite(s.worldScale) || s.worldScale <= 0 || s.worldScale > 1000000) return false;
  for (const auto& [position, cell] : s.cells) {
    if (!valid_position(position) || (cell.surface != Surface::Land && cell.surface != Surface::Water) || cell.heightMillimetres < -1000000000 || cell.heightMillimetres > 1000000000 || cell.personCapacity > 1000000) return false;
  }
  std::map<Position, std::size_t> occupancy;
  std::uint64_t quantity = 0;
  for (const auto& [key, r] : s.records) {
    if (key != r.ref || key.world != s.world || !key.id || !valid_kind(key.kind) || key.kind == Kind::MapMarker || !valid_name(r.name) || r.inventoryUnits > maxQuantity || r.createdDay > s.day) return false;
    if (r.previousNames.size() > maxCommands || (!rename_allowed(key.kind) && !r.previousNames.empty()) || std::any_of(r.previousNames.begin(), r.previousNames.end(), [](const auto& name) { return !valid_name(name); })) return false;
    if (r.position && (!valid_position(*r.position) || !s.cells.contains(*r.position))) return false;
    if (r.plantClass != PlantClass::Tree && r.plantClass != PlantClass::Other) return false;
    if (key.kind == Kind::Person) {
      if (!r.position || r.body != BodySpec{} || (r.sex != Sex::Male && r.sex != Sex::Female) || r.ageDays > maxDays || r.inventoryUnits || !r.protectedAccess.empty()) return false;
      if (r.origin != Origin::PlayerPlacement && r.origin != Origin::Birth) return false;
      if (r.initialAgeDays != (r.origin == Origin::PlayerPlacement ? adultAgeDays : 0) || r.ageDays < r.initialAgeDays) return false;
      const auto parents = std::count_if(r.links.begin(), r.links.end(), [](const auto& link) { return link.relation == Relation::Parent; });
      if (parents != (r.origin == Origin::Birth ? 2 : 0)) return false;
      if (r.active) {
        const auto& cell = s.cells.at(*r.position);
        if (cell.surface != Surface::Land || ++occupancy[*r.position] > cell.personCapacity) return false;
      }
    } else if (r.origin != Origin::None || r.ageDays || r.initialAgeDays) return false;
    if (physical(key.kind) && r.active) {
      if (!r.inventoryUnits || quantity > maxQuantity - r.inventoryUnits) return false;
      quantity += r.inventoryUnits;
    }
    for (const auto& position : r.protectedAccess) if (!building(key.kind) || !s.cells.contains(position) || s.cells.at(position).surface != Surface::Land) return false;
    std::set<Link> seen;
    for (const auto& link : r.links) {
      const auto target = s.records.find(link.target);
      if (target == s.records.end() || !role_valid(key.kind, link.relation, link.target.kind) || !seen.insert(link).second) return false;
      if ((link.relation == Relation::Parent || link.relation == Relation::Partner) && key == link.target) return false;
      if (link.relation == Relation::Partner && !has(target->second, Relation::Partner, key)) return false;
      if (r.active && !target->second.active && (link.relation == Relation::Home || link.relation == Relation::AccessRoad)) return false;
      if (r.active && physical(key.kind) && link.relation == Relation::Location && building(link.target.kind) && !target->second.active) return false;
    }
  }
  for (const auto& entry : s.history) for (const auto ref : entry.subjects) if (!s.records.contains(ref)) return false;
  return s.records.contains({s.world, Kind::World, 1});
}
Status person_space(const Snapshot& s, Position position) {
  const auto cell = s.cells.find(position);
  if (cell == s.cells.end() || cell->second.surface != Surface::Land) return Status::Invalid;
  const auto count = std::count_if(s.records.begin(), s.records.end(), [position](const auto& entry) { return entry.first.kind == Kind::Person && entry.second.active && entry.second.position == position; });
  return static_cast<std::uint64_t>(count) >= cell->second.personCapacity ? Status::Capacity : Status::Applied;
}
bool category_matches(ClearCategory category, const Record& r) {
  switch (category) {
  case ClearCategory::Trees: return r.ref.kind == Kind::Plant && r.plantClass == PlantClass::Tree;
  case ClearCategory::Plants: return r.ref.kind == Kind::Plant && r.plantClass == PlantClass::Other;
  case ClearCategory::Minerals: return r.ref.kind == Kind::MineralDeposit;
  case ClearCategory::Buildings: return building(r.ref.kind);
  case ClearCategory::Roads: return r.ref.kind == Kind::Road;
  case ClearCategory::People: return r.ref.kind == Kind::Person;
  }
  return false;
}
bool belongs(const Record& r, EntityRef scope) {
  if (scope.kind == Kind::World) return true;
  return std::any_of(r.links.begin(), r.links.end(), [scope](const auto& link) {
    return link.target == scope && (link.relation == Relation::Nationality || link.relation == Relation::Household || link.relation == Relation::Employer || link.relation == Relation::Language || link.relation == Relation::Culture || link.relation == Relation::Religion);
  }) || r.ref == scope;
}
}
std::string_view kind_name(Kind kind) {
  static constexpr std::array<std::string_view, 40> names{
    "person", "household", "settlement", "city", "state", "empire", "alliance", "culture", "language", "religion", "denomination", "church", "enterprise", "building", "port", "army", "vehicle", "garment", "stock_batch", "contract", "project", "innovation", "document", "plant", "mineral_deposit", "road", "world", "map_marker", "account", "reservation", "shipment", "production_line", "facility", "knowledge_license", "research_project", "war", "treaty", "genesis_node", "name_record", "history_event"};
  return valid_kind(kind) ? names[static_cast<std::size_t>(kind)] : std::string_view{};
}
struct World::Storage {
  struct Receipt { Command command; Authority authority; Result result; };
  Snapshot snapshot; Id nextId{2}; std::map<Id, Receipt> receipts;
};
World::World(Id world, std::map<Position, Cell> cells, std::vector<Record> imported, double scale)
  : state_(std::make_unique<Storage>()), writer_(std::this_thread::get_id()) {
  auto& s = state_->snapshot; s.world = world; s.worldScale = scale; s.cells = std::move(cells);
  Record worldRecord; worldRecord.ref = {world, Kind::World, 1}; worldRecord.name = "World";
  s.records.emplace(worldRecord.ref, worldRecord);
  for (auto& record : imported) {
    if (record.ref.kind == Kind::Person || record.ref.kind == Kind::World || record.ref.id == std::numeric_limits<Id>::max() || !s.records.emplace(record.ref, record).second) throw std::invalid_argument("invalid non-person import fixture");
    state_->nextId = std::max(state_->nextId, record.ref.id + 1);
  }
  if (!valid_state(s)) throw std::invalid_argument("invalid interaction world fixture");
}
World::~World() = default;
Result World::apply(const Command& command, Authority authority) {
  if (std::this_thread::get_id() != writer_) return {Status::WrongWriter, 0, {}, 0};
  const auto revision = state_->snapshot.revision;
  if (!command.id) return {Status::Invalid, revision, {}, 0};
  const auto receipt = state_->receipts.find(command.id);
  if (receipt != state_->receipts.end()) {
    if (receipt->second.command == command && receipt->second.authority == authority) return receipt->second.result;
    return {Status::CommandConflict, revision, {}, 0};
  }
  if (state_->receipts.size() >= maxCommands) return {Status::Limit, revision, {}, 0};
  try {
    auto next = std::make_unique<Storage>(*state_);
    auto& s = next->snapshot;
    Result result{Status::Applied, revision, {}, 0};
    std::vector<EntityRef> subjects;
    std::string action;
    if (command.expectedRevision != revision) result.status = Status::StaleRevision;
    else if (authority != Authority::Player && authority != Authority::Scheduler) result.status = Status::Unauthorized;
    else if (revision == std::numeric_limits<std::uint64_t>::max()) result.status = Status::Limit;
    else result.status = std::visit([&](const auto& payload) -> Status {
      using T = std::decay_t<decltype(payload)>;
      if constexpr (std::is_same_v<T, PlacePerson> || std::is_same_v<T, Birth>) {
        if constexpr (std::is_same_v<T, PlacePerson>) { if (authority != Authority::Player) return Status::Unauthorized; }
        else { if (authority != Authority::Scheduler) return Status::Unauthorized; if (!s.rules.reproduction) return Status::Dependency; }
        if (!valid_name(payload.name) || (payload.sex != Sex::Male && payload.sex != Sex::Female)) return Status::Invalid;
        if (s.records.size() >= maxRecords || next->nextId == std::numeric_limits<Id>::max()) return Status::Limit;
        Record person; person.ref = {s.world, Kind::Person, next->nextId}; person.name = payload.name; person.sex = payload.sex; person.createdDay = s.day;
        if constexpr (std::is_same_v<T, PlacePerson>) {
          person.position = payload.position; person.origin = Origin::PlayerPlacement; person.initialAgeDays = adultAgeDays; person.ageDays = adultAgeDays;
        } else {
          const auto first = s.records.find(payload.firstParent), second = s.records.find(payload.secondParent);
          if (first == s.records.end() || second == s.records.end()) return Status::NotFound;
          if (payload.firstParent.kind != Kind::Person || payload.secondParent.kind != Kind::Person || payload.firstParent == payload.secondParent) return Status::Invalid;
          const auto& a = first->second; const auto& b = second->second;
          if (!a.active || !b.active || a.ageDays < adultAgeDays || b.ageDays < adultAgeDays || a.sex == b.sex || a.position != b.position || !has(a, Relation::Partner, b.ref) || !has(b, Relation::Partner, a.ref)) return Status::Dependency;
          person.position = a.position; person.origin = Origin::Birth;
          person.links = {{Relation::Parent, a.ref}, {Relation::Parent, b.ref}};
        }
        const auto space = person_space(s, *person.position); if (space != Status::Applied) return space;
        s.records.emplace(person.ref, person); ++next->nextId; result.created = person.ref;
        subjects.push_back(person.ref); result.affected = 1; action = std::is_same_v<T, PlacePerson> ? "player_placement" : "birth";
        return Status::Applied;
      } else if constexpr (std::is_same_v<T, Rename>) {
        if (authority != Authority::Player) return Status::Unauthorized;
        auto it = s.records.find(payload.target); if (it == s.records.end()) return Status::NotFound;
        if (!it->second.active) return Status::Archived;
        if (!rename_allowed(payload.target.kind)) return Status::Invalid;
        if (!valid_name(payload.name)) return Status::Invalid;
        it->second.previousNames.push_back(it->second.name); it->second.name = payload.name;
        subjects.push_back(payload.target); result.affected = 1; action = "rename"; return Status::Applied;
      } else if constexpr (std::is_same_v<T, SetRule>) {
        if (authority != Authority::Player) return Status::Unauthorized;
        switch (payload.rule) {
        case Rule::Reproduction: s.rules.reproduction = payload.enabled; break;
        case Rule::Aging: s.rules.aging = payload.enabled; break;
        case Rule::HungerConsequences: s.rules.hungerConsequences = payload.enabled; break;
        case Rule::AiNewWars: s.rules.aiNewWars = payload.enabled; break;
        default: return Status::Invalid;
        }
        subjects.push_back({s.world, Kind::World, 1}); result.affected = 1; action = "world_rule"; return Status::Applied;
      } else if constexpr (std::is_same_v<T, FormPartners>) {
        if (authority != Authority::Scheduler) return Status::Unauthorized;
        auto a = s.records.find(payload.first), b = s.records.find(payload.second);
        if (a == s.records.end() || b == s.records.end()) return Status::NotFound;
        if (payload.first.kind != Kind::Person || payload.second.kind != Kind::Person || payload.first == payload.second) return Status::Invalid;
        if (!a->second.active || !b->second.active || a->second.ageDays < adultAgeDays || b->second.ageDays < adultAgeDays) return Status::Dependency;
        if (std::any_of(a->second.links.begin(), a->second.links.end(), [](const auto& link) { return link.relation == Relation::Partner; }) || std::any_of(b->second.links.begin(), b->second.links.end(), [](const auto& link) { return link.relation == Relation::Partner; })) return Status::Dependency;
        a->second.links.push_back({Relation::Partner, b->first}); b->second.links.push_back({Relation::Partner, a->first});
        subjects = {a->first, b->first}; result.affected = 2; action = "partners"; return Status::Applied;
      } else if constexpr (std::is_same_v<T, SetRelation>) {
        if (authority != Authority::Scheduler) return Status::Unauthorized;
        auto source = s.records.find(payload.source), target = s.records.find(payload.target);
        if (source == s.records.end() || target == s.records.end()) return Status::NotFound;
        if (!source->second.active || !target->second.active) return Status::Archived;
        if (payload.relation == Relation::Parent || payload.relation == Relation::Partner || !role_valid(payload.source.kind, payload.relation, payload.target.kind)) return Status::Invalid;
        auto& links = source->second.links;
        links.erase(std::remove_if(links.begin(), links.end(), [&](const auto& link) { return link.relation == payload.relation; }), links.end());
        links.push_back({payload.relation, payload.target}); subjects = {payload.source, payload.target}; result.affected = 1; action = "relation"; return Status::Applied;
      } else if constexpr (std::is_same_v<T, Clear>) {
        if (authority != Authority::Player) return Status::Unauthorized;
        if (payload.targets.empty() || payload.targets.size() > 256) return Status::Invalid;
        const std::set<EntityRef> targets(payload.targets.begin(), payload.targets.end());
        for (const auto ref : targets) {
          auto it = s.records.find(ref); if (it == s.records.end()) return Status::NotFound;
          if (!it->second.active) return Status::Archived;
          if (!category_matches(payload.category, it->second)) return Status::Invalid;
          it->second.active = false;
        }
        for (const auto ref : targets) {
          const auto& removed = s.records.at(ref);
          if (building(ref.kind) && removed.inventoryUnits) return Status::Dependency;
          for (const auto& [key, remaining] : s.records) {
            if (!remaining.active) continue;
            if (building(ref.kind) && (has(remaining, Relation::Home, ref) || (physical(key.kind) && has(remaining, Relation::Location, ref)))) return Status::Dependency;
            if (ref.kind == Kind::Road && building(key.kind) && (has(remaining, Relation::AccessRoad, ref) || (removed.position && std::find(remaining.protectedAccess.begin(), remaining.protectedAccess.end(), *removed.position) != remaining.protectedAccess.end()))) return Status::Dependency;
          }
        }
        subjects.assign(targets.begin(), targets.end()); result.affected = subjects.size(); action = payload.category == ClearCategory::People ? "player_clear_death" : "player_clear";
        return Status::Applied;
      } else {
        if (authority != Authority::Scheduler) return Status::Unauthorized;
        if (!payload.days || payload.days > maxDays - s.day) return Status::Invalid;
        if (s.rules.aging) for (auto& [key, record] : s.records) {
          if (key.kind != Kind::Person || !record.active) continue;
          if (record.ageDays > maxDays - payload.days) return Status::Limit;
          record.ageDays += payload.days;
        }
        s.day += payload.days; subjects.push_back({s.world, Kind::World, 1}); result.affected = 1; action = "advance"; return Status::Applied;
      }
    }, command.payload);
    if (result.status == Status::Applied) {
      ++s.revision; result.revision = s.revision;
      s.history.push_back({command.id, s.day, subjects, action});
      if (!valid_state(s)) result.status = Status::Dependency;
    }
    if (result.status != Status::Applied) {
      next = std::make_unique<Storage>(*state_); result.revision = revision; result.created.reset(); result.affected = 0;
    }
    next->receipts.emplace(command.id, Storage::Receipt{command, authority, result});
    state_.swap(next); return result;
  } catch (const std::bad_alloc&) { return {Status::AllocationFailure, revision, {}, 0}; }
}
void World::require_owner_thread() const {
  if (std::this_thread::get_id() != writer_) throw std::logic_error("World reads belong to its writer thread; pass detached snapshots to UI threads");
}
Snapshot World::snapshot() const { require_owner_thread(); return state_->snapshot; }
std::optional<Record> World::inspect(EntityRef ref) const {
  require_owner_thread();
  const auto it = state_->snapshot.records.find(ref); return it == state_->snapshot.records.end() ? std::nullopt : std::optional<Record>{it->second};
}
std::vector<Record> World::search(const Search& query) const {
  require_owner_thread();
  std::vector<Record> result;
  for (const auto& [ref, record] : state_->snapshot.records) {
    if (!query.includeArchived && !record.active) continue;
    if (query.kind && ref.kind != *query.kind) continue;
    if (record.name.find(query.nameContains) == std::string::npos) continue;
    if (query.relation && !has(record, query.relation->relation, query.relation->target)) continue;
    result.push_back(record);
  }
  return result;
}
std::vector<Record> World::follow_links(EntityRef ref) const {
  require_owner_thread();
  std::vector<Record> result; const auto origin = inspect(ref); if (!origin) return result;
  std::set<EntityRef> seen;
  for (const auto& link : origin->links) if (seen.insert(link.target).second) result.push_back(*inspect(link.target));
  return result;
}
std::vector<Statistics> World::compare(const std::vector<EntityRef>& scopes) const {
  require_owner_thread();
  std::vector<Statistics> result; std::set<EntityRef> seen;
  const auto& records = state_->snapshot.records;
  for (const auto scope : scopes) {
    if (!records.contains(scope)) throw std::invalid_argument("statistics scope does not resolve");
    if (scope.kind != scopes.front().kind) throw std::invalid_argument("comparison requires matching entity kinds and metric profiles");
    if (scope.kind != Kind::World && scope.kind != Kind::Person && scope.kind != Kind::State && scope.kind != Kind::Household && scope.kind != Kind::Enterprise && scope.kind != Kind::Language && scope.kind != Kind::Culture && scope.kind != Kind::Religion) throw std::invalid_argument("statistics profile not implemented for this entity kind");
    if (!seen.insert(scope).second) continue;
    Statistics row; row.scope = scope; row.day = state_->snapshot.day; row.revision = state_->snapshot.revision;
    std::set<EntityRef> members;
    for (const auto& [ref, record] : records) if (ref.kind == Kind::Person && belongs(record, scope)) {
      members.insert(ref);
      if (!record.active) { ++row.archivedPeople; continue; }
      ++row.livePeople;
      if (record.origin == Origin::PlayerPlacement) ++row.placedPeople; else ++row.bornPeople;
      if (record.ageDays >= adultAgeDays) ++row.adults;
    }
    for (const auto& [ref, record] : records) if (physical(ref.kind) && record.active) {
      const bool owned = std::any_of(record.links.begin(), record.links.end(), [&](const auto& link) { return link.relation == Relation::Owner && (link.target == scope || members.contains(link.target)); });
      if (scope.kind == Kind::World || owned || ref == scope) { ++row.physicalItems; row.itemUnits += record.inventoryUnits; }
    }
    result.push_back(row);
  }
  return result;
}
bool World::validate() const { require_owner_thread(); return valid_state(state_->snapshot); }
bool PlayerView::favorite(const World& world, EntityRef ref, bool enabled) {
  if (ref.world != world_ || world.snapshot().world != world_) return false;
  if (ref.kind == Kind::MapMarker ? !markers_.contains(ref) : !world.inspect(ref).has_value()) return false;
  const bool existing = favorites_.contains(ref);
  if (existing == enabled) return true;
  if (revision_ == std::numeric_limits<std::uint64_t>::max()) return false;
  if (enabled) favorites_.insert(ref); else favorites_.erase(ref);
  ++revision_; return true;
}
std::optional<EntityRef> PlayerView::add_marker(const World& world, std::string name, Position position) {
  const auto s = world.snapshot();
  if (s.world != world_ || !valid_name(name) || !s.cells.contains(position) || nextMarker_ == std::numeric_limits<Id>::max() || revision_ == std::numeric_limits<std::uint64_t>::max() || markers_.size() >= maxRecords) return std::nullopt;
  const EntityRef ref{world_, Kind::MapMarker, nextMarker_}; markers_.emplace(ref, Marker{ref, std::move(name), position}); ++nextMarker_; ++revision_; return ref;
}
bool PlayerView::remove_marker(EntityRef ref) {
  if (ref.world != world_ || !markers_.contains(ref) || revision_ == std::numeric_limits<std::uint64_t>::max()) return false;
  markers_.erase(ref); favorites_.erase(ref); ++revision_; return true;
}
std::optional<Marker> PlayerView::inspect_marker(EntityRef ref) const {
  const auto it = markers_.find(ref); return it == markers_.end() ? std::nullopt : std::optional<Marker>{it->second};
}
std::vector<EntityRef> PlayerView::favorites() const { return {favorites_.begin(), favorites_.end()}; }
} // namespace sonnheide::interaction
