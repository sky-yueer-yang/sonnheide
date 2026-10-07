#pragma once
#include <compare>
#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <thread>
#include <variant>
#include <vector>

namespace sonnheide::interaction {
using Id = std::uint64_t;
constexpr std::uint64_t daysPerYear = 360;
constexpr std::uint64_t adultAgeDays = 18 * daysPerYear;
enum class Kind {
  Person, Household, Settlement, City, State, Empire, Alliance, Culture, Language,
  Religion, Denomination, Church, Enterprise, Building, Port, Army, Vehicle,
  Garment, StockBatch, Contract, Project, Innovation, Document, Plant,
  MineralDeposit, Road, World, MapMarker, Account, Reservation, Shipment,
  ProductionLine, Facility, KnowledgeLicense, ResearchProject, War, Treaty,
  GenesisNode, NameRecord, HistoryEvent
};
std::string_view kind_name(Kind kind);
struct EntityRef {
  Id world{}; Kind kind{}; Id id{};
  auto operator<=>(const EntityRef&) const = default;
};
struct Position { std::int64_t x{}, z{}; auto operator<=>(const Position&) const = default; };
enum class Surface { Land, Water };
struct Cell {
  Surface surface{Surface::Land}; std::int64_t heightMillimetres{}; std::uint32_t personCapacity{32};
  auto operator<=>(const Cell&) const = default;
};
struct BodySpec {
  std::string rig{"adult_v1"}, clothingFit{"adult_v1"}; std::uint32_t scalePermille{1000};
  auto operator<=>(const BodySpec&) const = default;
};
enum class Sex { Male, Female };
enum class Origin { None, PlayerPlacement, Birth };
enum class PlantClass { Tree, Other };
enum class Relation { Nationality, Household, Home, Employer, Language, Culture, Religion,
                      Partner, Parent, Author, Owner, Controller, Custodian, Location, AccessRoad, Related };
struct Link { Relation relation{}; EntityRef target{}; auto operator<=>(const Link&) const = default; };
struct Record {
  EntityRef ref{}; std::string name; std::vector<std::string> previousNames; bool active{true}; std::optional<Position> position;
  Origin origin{Origin::None}; Sex sex{Sex::Male}; std::uint64_t initialAgeDays{}, ageDays{}, createdDay{};
  BodySpec body{}; PlantClass plantClass{PlantClass::Other};
  std::vector<Link> links; std::uint64_t inventoryUnits{};
  std::vector<Position> protectedAccess;
  auto operator<=>(const Record&) const = default;
};
enum class Rule { Reproduction, Aging, HungerConsequences, AiNewWars };
struct Rules {
  bool reproduction{true}, aging{true}, hungerConsequences{true}, aiNewWars{true};
  auto operator<=>(const Rules&) const = default;
};
enum class Authority { Player, Scheduler };
struct PlacePerson { std::string name; Position position{}; Sex sex{}; auto operator<=>(const PlacePerson&) const = default; };
struct Birth { std::string name; EntityRef firstParent{}, secondParent{}; Sex sex{}; auto operator<=>(const Birth&) const = default; };
struct Rename { EntityRef target{}; std::string name; auto operator<=>(const Rename&) const = default; };
struct SetRule { Rule rule{}; bool enabled{}; auto operator<=>(const SetRule&) const = default; };
struct FormPartners { EntityRef first{}, second{}; auto operator<=>(const FormPartners&) const = default; };
struct SetRelation { EntityRef source{}; Relation relation{}; EntityRef target{}; auto operator<=>(const SetRelation&) const = default; };
enum class ClearCategory { Trees, Plants, Minerals, Buildings, Roads, People };
struct Clear { ClearCategory category{}; std::vector<EntityRef> targets; auto operator<=>(const Clear&) const = default; };
struct Advance { std::uint64_t days{}; auto operator<=>(const Advance&) const = default; };
using Payload = std::variant<PlacePerson, Birth, Rename, SetRule, FormPartners, SetRelation, Clear, Advance>;
struct Command { Id id{}; std::uint64_t expectedRevision{}; Payload payload; auto operator<=>(const Command&) const = default; };
enum class Status { Applied, Invalid, Unauthorized, StaleRevision, CommandConflict, NotFound,
                    Archived, Capacity, Dependency, WrongWriter, Limit, AllocationFailure };
struct Result {
  Status status{Status::Invalid}; std::uint64_t revision{}; std::optional<EntityRef> created;
  std::size_t affected{}; auto operator<=>(const Result&) const = default;
};
struct HistoryEntry {
  Id command{}; std::uint64_t day{}; std::vector<EntityRef> subjects; std::string action;
  auto operator<=>(const HistoryEntry&) const = default;
};
struct Snapshot {
  Id world{}; std::uint64_t revision{}, day{}; double worldScale{}; Rules rules;
  std::map<Position, Cell> cells; std::map<EntityRef, Record> records; std::vector<HistoryEntry> history;
  auto operator<=>(const Snapshot&) const = default;
};
struct Search {
  std::string nameContains; std::optional<Kind> kind; bool includeArchived{};
  std::optional<Link> relation;
};
struct Statistics {
  EntityRef scope{}; std::uint64_t day{}, revision{}, livePeople{}, archivedPeople{}, placedPeople{}, bornPeople{}, adults{};
  std::uint64_t physicalItems{}, itemUnits{};
  auto operator<=>(const Statistics&) const = default;
};

// Finite headless interaction oracle, not the production economy/navigation/save system.
// Import fixtures may contain existing non-person objects; all people enter by commands.
// Authority is trusted dispatch context, never a user-supplied command field.
// One writer thread also owns reads; query results are detached value snapshots.
class World {
public:
  World(Id world, std::map<Position, Cell> cells, std::vector<Record> imported = {}, double worldScale = 1.0);
  ~World();
  World(const World&) = delete;
  World& operator=(const World&) = delete;
  Result apply(const Command& command, Authority authority);
  Snapshot snapshot() const;
  std::optional<Record> inspect(EntityRef ref) const;
  std::vector<Record> search(const Search& query) const;
  std::vector<Record> follow_links(EntityRef ref) const;
  std::vector<Statistics> compare(const std::vector<EntityRef>& scopes) const;
  bool validate() const;
private:
  struct Storage;
  void require_owner_thread() const;
  std::unique_ptr<Storage> state_;
  std::thread::id writer_;
};

struct Marker { EntityRef ref{}; std::string name; Position position{}; auto operator<=>(const Marker&) const = default; };
// Local UI state is deliberately separate: no simulation revision/RNG or inventory change.
class PlayerView {
public:
  explicit PlayerView(Id world) : world_(world) {}
  bool favorite(const World& world, EntityRef ref, bool enabled);
  std::optional<EntityRef> add_marker(const World& world, std::string name, Position position);
  bool remove_marker(EntityRef ref);
  std::optional<Marker> inspect_marker(EntityRef ref) const;
  std::vector<EntityRef> favorites() const;
  std::uint64_t revision() const { return revision_; }
private:
  Id world_{}, nextMarker_{1}; std::uint64_t revision_{};
  std::set<EntityRef> favorites_; std::map<EntityRef, Marker> markers_;
};
} // namespace sonnheide::interaction
