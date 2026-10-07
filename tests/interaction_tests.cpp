#include "sonnheide/interaction.hpp"
#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <new>
#include <stdexcept>
#include <thread>

namespace {
thread_local long failAllocationAfter = -1;
}
void* operator new(std::size_t size) {
  if (failAllocationAfter == 0) { failAllocationAfter = -1; throw std::bad_alloc{}; }
  if (failAllocationAfter > 0) --failAllocationAfter;
  if (auto* result = std::malloc(size ? size : 1)) return result;
  throw std::bad_alloc{};
}
void operator delete(void* p) noexcept { std::free(p); }
void operator delete(void* p, std::size_t) noexcept { std::free(p); }
void* operator new[](std::size_t size) { return ::operator new(size); }
void operator delete[](void* p) noexcept { ::operator delete(p); }
void operator delete[](void* p, std::size_t) noexcept { ::operator delete(p); }

using namespace sonnheide::interaction;
namespace {
constexpr Id worldId = 71;
EntityRef ref(Kind kind, Id id) { return {worldId, kind, id}; }
void check(bool ok, const char* message) { if (!ok) throw std::runtime_error(message); }
template<class F> void invalid(F run, const char* message) {
  bool rejected = false; try { run(); } catch (const std::invalid_argument&) { rejected = true; }
  check(rejected, message);
}
Record record(Kind kind, Id id, std::string name, std::optional<Position> position = {}) {
  Record r; r.ref = ref(kind, id); r.name = std::move(name); r.position = position; return r;
}
World fixture() {
  std::map<Position, Cell> cells{{{0, 0}, {Surface::Land, 900, 12}}, {{1, 0}, {Surface::Land, 2000, 1}}, {{2, 0}, {Surface::Water, -400, 0}}, {{3, 0}, {Surface::Land, 4000, 12}}, {{4, 0}, {Surface::Land, 5000, 12}}, {{5, 0}, {Surface::Land, 6000, 12}}};
  auto house = record(Kind::Building, 6, "House", Position{3, 0}); house.protectedAccess = {{4, 0}}; house.links = {{Relation::AccessRoad, ref(Kind::Road, 5)}};
  auto store = record(Kind::Building, 7, "Warehouse", Position{5, 0}); store.inventoryUnits = 7;
  auto tree = record(Kind::Plant, 8, "Tree", Position{3, 0}); tree.plantClass = PlantClass::Tree;
  auto mineral = record(Kind::MineralDeposit, 10, "Deposit", Position{5, 0}); mineral.inventoryUnits = 77;
  auto garment = record(Kind::Garment, 11, "Tee", Position{5, 0}); garment.inventoryUnits = 1; garment.links = {{Relation::Location, ref(Kind::Building, 7)}};
  auto stock = record(Kind::StockBatch, 12, "Fabric", Position{5, 0}); stock.inventoryUnits = 40; stock.links = {{Relation::Owner, ref(Kind::Enterprise, 4)}, {Relation::Location, ref(Kind::Building, 7)}};
  return World(worldId, std::move(cells), {
    record(Kind::State, 2, "Sonne"), record(Kind::Language, 3, "Sonnisch"), record(Kind::Enterprise, 4, "Textiles"),
    record(Kind::Road, 5, "Old access", Position{4, 0}), house, store, tree, record(Kind::Plant, 9, "Grass", Position{3, 0}), mineral, garment, stock, record(Kind::Document, 13, "Book"), record(Kind::State, 14, "Other state")}, 3.5);
}
Result send(World& world, Id id, Payload payload, Authority authority = Authority::Player) {
  return world.apply({id, world.snapshot().revision, std::move(payload)}, authority);
}
EntityRef place(World& world, Id id, std::string name = "Person", Sex sex = Sex::Male, Position p = {0, 0}) {
  const auto result = send(world, id, PlacePerson{std::move(name), p, sex});
  check(result.status == Status::Applied && result.created.has_value(), "player placement failed"); return *result.created;
}
void relate(World& world, Id id, EntityRef source, Relation relation, EntityRef target) {
  check(send(world, id, SetRelation{source, relation, target}, Authority::Scheduler).status == Status::Applied, "trusted existing relationship failed");
}
std::pair<EntityRef, EntityRef> parents(World& world) {
  const auto first = place(world, 1, "Father"); const auto second = place(world, 2, "Mother", Sex::Female);
  check(send(world, 3, FormPartners{first, second}, Authority::Scheduler).status == Status::Applied, "adult partnership failed"); return {first, second};
}
}
int main() {
  int passed = 0;
  const auto test = [&](const char* name, auto run) { run(); ++passed; std::cout << "PASS " << name << '\n'; };
  try {
    test("all forty protocol kinds have stable lowercase names", [] {
      for (int i = 0; i < 40; ++i) check(!kind_name(static_cast<Kind>(i)).empty(), "protocol kind missing");
      check(kind_name(Kind::MineralDeposit) == "mineral_deposit" && kind_name(Kind::HistoryEvent) == "history_event" && kind_name(static_cast<Kind>(400)).empty(), "protocol kind naming invalid");
    });
    test("player placement is eighteen with fixed body and no money or goods", [] {
      auto world = fixture(); const auto before = world.compare({ref(Kind::World, 1)}).front(); const auto person = place(world, 1);
      const auto row = *world.inspect(person); check(row.initialAgeDays == adultAgeDays && row.ageDays == adultAgeDays && row.origin == Origin::PlayerPlacement && row.body == BodySpec{} && row.inventoryUnits == 0 && row.links.empty(), "placement inferred property, biography or identity");
      const auto after = world.compare({ref(Kind::World, 1)}).front(); check(after.livePeople == 1 && after.itemUnits == before.itemUnits && after.physicalItems == before.physicalItems && world.validate(), "placement spawned resources");
      const auto copy = world.snapshot(); check(copy.worldScale == 3.5 && copy.cells.at({0, 0}).heightMillimetres == 900, "placement flattened authentic terrain");
    });
    test("no implicit person imports scheduler placement or invalid land placement", [] {
      auto world = fixture(); const auto before = world.snapshot();
      check(send(world, 1, PlacePerson{"Worker", {0, 0}, Sex::Male}, Authority::Scheduler).status == Status::Unauthorized, "scheduler spawned free workers");
      check(send(world, 2, PlacePerson{"Water", {2, 0}, Sex::Male}).status == Status::Invalid, "water placement permitted");
      check(send(world, 3, PlacePerson{"Missing", {9, 9}, Sex::Male}).status == Status::Invalid, "outside placement permitted");
      check(send(world, 4, PlacePerson{"Invalid", {0, 0}, static_cast<Sex>(900)}).status == Status::Invalid, "invalid sex admitted");
      check(world.snapshot() == before, "failed spawn mutated world");
      auto p = record(Kind::Person, 88, "Imported person", Position{0, 0}); p.ageDays = adultAgeDays; p.origin = Origin::PlayerPlacement;
      invalid([&] { World imported(worldId, {{{0, 0}, {}}}, {p}); }, "fixture silently imported population");
    });
    test("placement respects cell capacity during continued simulation", [] {
      auto world = fixture(); (void)place(world, 1, "First", Sex::Male, {1, 0}); const auto before = world.snapshot();
      check(send(world, 2, PlacePerson{"Second", {1, 0}, Sex::Female}).status == Status::Capacity && world.snapshot() == before, "overfull cell committed partly");
      check(send(world, 3, Advance{10}, Authority::Scheduler).status == Status::Applied, "advance failed");
      const auto later = place(world, 4); check(world.inspect(later)->ageDays == adultAgeDays && world.inspect(later)->createdDay == 10, "later placement inherited world age");
    });
    test("birth is scheduler only with existing adult partnered parents and zero age", [] {
      auto world = fixture(); const auto [father, mother] = parents(world); const auto before = world.snapshot();
      check(send(world, 4, Birth{"Child", father, mother, Sex::Female}).status == Status::Unauthorized && world.snapshot() == before, "player injected birth");
      check(send(world, 5, Birth{"Clone", father, father, Sex::Male}, Authority::Scheduler).status == Status::Invalid, "single parent clone allowed");
      const auto birth = send(world, 6, Birth{"Child", father, mother, Sex::Female}, Authority::Scheduler);
      check(birth.status == Status::Applied && birth.created, "legitimate birth failed");
      const auto child = *world.inspect(*birth.created); check(child.origin == Origin::Birth && child.initialAgeDays == 0 && child.ageDays == 0 && child.body == world.inspect(father)->body && child.body == world.inspect(mother)->body && child.inventoryUnits == 0 && child.links.size() == 2, "birth altered body/fit or created resources");
      const auto third = place(world, 7, "Third", Sex::Female);
      check(send(world, 8, Birth{"Unrelated", father, third, Sex::Male}, Authority::Scheduler).status == Status::Dependency, "relationship-free reproduction admitted");
      check(send(world, 9, FormPartners{*birth.created, third}, Authority::Scheduler).status == Status::Dependency, "adult-sized newborn treated as adult");
      check(send(world, 10, Birth{"Underage", *birth.created, third, Sex::Male}, Authority::Scheduler).status == Status::Dependency && world.validate(), "visual size bypassed biological adulthood");
    });
    test("reproduction rule is rechecked at commit and never blocks player placement", [] {
      auto world = fixture(); const auto [father, mother] = parents(world);
      const Command queued{4, world.snapshot().revision, Birth{"Queued", father, mother, Sex::Male}};
      check(send(world, 5, SetRule{Rule::Reproduction, false}).status == Status::Applied, "player could not set global rule");
      check(world.apply(queued, Authority::Scheduler).status == Status::StaleRevision, "queued birth bypassed changed rule revision");
      check(send(world, 6, Birth{"Retry", father, mother, Sex::Male}, Authority::Scheduler).status == Status::Dependency, "fresh birth bypassed reproduction disable");
      (void)place(world, 7, "Manual placement");
      check(send(world, 8, SetRule{Rule::Reproduction, true}, Authority::Scheduler).status == Status::Unauthorized, "scheduler changed global law");
      check(send(world, 9, SetRule{Rule::Reproduction, true}).status == Status::Applied, "rule could not reenable");
      check(send(world, 10, Birth{"Resumed", father, mother, Sex::Female}, Authority::Scheduler).status == Status::Applied, "birth did not resume");
    });
    test("aging may freeze biological age without changing calendar origin or adult fit", [] {
      auto world = fixture(); const auto [father, mother] = parents(world); const auto birth = send(world, 4, Birth{"Child", father, mother, Sex::Male}, Authority::Scheduler); const auto child = *birth.created;
      check(send(world, 5, SetRule{Rule::Aging, false}).status == Status::Applied && send(world, 6, Advance{3600}, Authority::Scheduler).status == Status::Applied, "age freeze failed");
      check(world.snapshot().day == 3600 && world.inspect(child)->ageDays == 0 && world.inspect(father)->ageDays == adultAgeDays && world.inspect(child)->createdDay == 0, "calendar falsely changed biological age/source");
      check(send(world, 7, SetRule{Rule::Aging, true}).status == Status::Applied && send(world, 8, Advance{adultAgeDays}, Authority::Scheduler).status == Status::Applied, "aging resume failed");
      check(world.inspect(child)->ageDays == adultAgeDays && world.inspect(child)->body == BodySpec{} && world.inspect(father)->body == BodySpec{} && world.validate(), "aging rescaled body or clothes fit");
    });
    test("rule toggles do not fabricate food or erase existing war records", [] {
      auto war = record(Kind::War, 2, "Existing war"); World world(worldId, {{{0, 0}, {}}}, {war});
      check(send(world, 1, SetRule{Rule::HungerConsequences, false}).status == Status::Applied && send(world, 2, SetRule{Rule::AiNewWars, false}).status == Status::Applied, "rules failed");
      check(!world.snapshot().rules.hungerConsequences && !world.snapshot().rules.aiNewWars && world.inspect(war.ref)->active && world.compare({ref(Kind::World, 1)}).front().itemUnits == 0, "snapshot rule fabricated supply or deleted old war");
    });
    test("expected revision command deduplication and bounded typed edits", [] {
      auto world = fixture(); const Command command{1, 0, PlacePerson{"Person", {0, 0}, Sex::Male}};
      const auto result = world.apply(command, Authority::Player); const auto once = world.snapshot();
      check(world.apply(command, Authority::Player) == result && world.snapshot() == once, "successful retry duplicated person or history");
      auto conflict = command; std::get<PlacePerson>(conflict.payload).name = "Other";
      check(world.apply(conflict, Authority::Player).status == Status::CommandConflict && world.apply(command, Authority::Scheduler).status == Status::CommandConflict, "command id reused with changed payload/authority");
      const Command stale{2, 0, Rename{*result.created, "Wrong"}};
      check(world.apply(stale, Authority::Player).status == Status::StaleRevision && world.apply(stale, Authority::Player).status == Status::StaleRevision && world.snapshot() == once, "stale revision mutated state or failed receipt changed");
      check(send(world, 3, Rename{*result.created, std::string(129, 'x')}).status == Status::Invalid, "unbounded name accepted");
      check(send(world, 4, Rename{*result.created, "Renamed"}).status == Status::Applied && world.inspect(*result.created)->ageDays == adultAgeDays && world.inspect(*result.created)->body == BodySpec{}, "allowed name edit changed restricted body or age");
      check(world.inspect(*result.created)->previousNames == std::vector<std::string>{"Person"}, "rename rewrote previous historical name");
    });
    test("fact records cannot be renamed through the free display name command", [] {
      auto event = record(Kind::HistoryEvent, 2, "Historical event"); auto name = record(Kind::NameRecord, 3, "Original name fact"); auto account = record(Kind::Account, 4, "Account identity");
      World world(worldId, {{{0, 0}, {}}}, {event, name, account}); const auto before = world.snapshot();
      check(send(world, 1, Rename{event.ref, "Rewrite"}).status == Status::Invalid && send(world, 2, Rename{name.ref, "Rewrite"}).status == Status::Invalid && send(world, 3, Rename{account.ref, "Rewrite"}).status == Status::Invalid && world.snapshot() == before, "free rename rewrote immutable historical/account facts");
    });
    test("map inspection typed link navigation and filters share stable references", [] {
      auto world = fixture(); const auto person = place(world, 1, "Qian"); relate(world, 2, person, Relation::Nationality, ref(Kind::State, 2)); relate(world, 3, person, Relation::Language, ref(Kind::Language, 3));
      const auto links = world.follow_links(person); check(links.size() == 2 && world.inspect(links.front().ref).has_value(), "person nationality/language pages fail to resolve");
      check(world.search({"Q", Kind::Person, false, Link{Relation::Nationality, ref(Kind::State, 2)}}).size() == 1, "name and typed relation query disagree");
      check(!world.inspect({999, Kind::Person, person.id}) && !world.inspect({worldId, Kind::State, person.id}), "wrong world/kind guessed person");
      const auto before = world.snapshot(); check(send(world, 4, SetRelation{person, Relation::Nationality, ref(Kind::Language, 3)}, Authority::Scheduler).status == Status::Invalid && world.snapshot() == before, "wrong typed nationality partly committed");
      auto copy = *world.inspect(person); copy.name = "Local only"; copy.ageDays = 0; copy.body.scalePermille = 1;
      auto snapshot = world.snapshot(); snapshot.records.clear(); check(world.inspect(person)->name == "Qian" && world.inspect(person)->ageDays == adultAgeDays && world.validate(), "query exposed mutable world storage");
    });
    test("favorites and map markers do not change simulation revision or data", [] {
      auto world = fixture(); const auto person = place(world, 1); PlayerView view(worldId); const auto before = world.snapshot();
      check(view.favorite(world, person, true) && view.favorite(world, ref(Kind::Garment, 11), true), "person/physical item favorite failed");
      const auto marker = view.add_marker(world, "Future harbor", {2, 0}); check(marker && view.favorite(world, *marker, true) && !world.inspect(*marker) && view.inspect_marker(*marker)->position == Position{2, 0}, "local map marker resolver failed");
      const auto revision = view.revision(); check(view.favorite(world, person, true) && view.revision() == revision && world.snapshot() == before, "favorite repeat or camera state changed world");
      check(!view.favorite(world, {999, Kind::Person, person.id}, true) && !view.add_marker(world, "Outside", {999, 999}), "view accepted cross-world or invalid marker");
      check(view.remove_marker(*marker) && !view.inspect_marker(*marker) && view.favorites().size() == 2 && world.snapshot() == before, "removed marker left dangling favorite or changed simulation");
    });
    test("death clear preserves family authors ownership custody favorites and history", [] {
      auto world = fixture(); const auto [father, mother] = parents(world); const auto birth = send(world, 4, Birth{"Child", father, mother, Sex::Male}, Authority::Scheduler);
      relate(world, 5, father, Relation::Nationality, ref(Kind::State, 2)); relate(world, 6, mother, Relation::Nationality, ref(Kind::State, 2)); relate(world, 7, *birth.created, Relation::Nationality, ref(Kind::State, 2));
      relate(world, 8, ref(Kind::Document, 13), Relation::Author, father); relate(world, 9, ref(Kind::Garment, 11), Relation::Owner, father); relate(world, 10, ref(Kind::Garment, 11), Relation::Custodian, mother);
      PlayerView view(worldId); check(view.favorite(world, father, true), "father favorite failed");
      const auto before = world.compare({ref(Kind::State, 2)}).front(); const auto stockBefore = world.compare({ref(Kind::World, 1)}).front();
      check(send(world, 11, Clear{ClearCategory::People, {father}}).status == Status::Applied, "player death clear failed");
      const auto after = world.compare({ref(Kind::State, 2)}).front();
      check(before.livePeople == 3 && after.livePeople == 2 && after.archivedPeople == 1 && after.itemUnits == before.itemUnits, "death lost national statistics or estate goods");
      check(!world.inspect(father)->active && world.inspect(father)->origin == Origin::PlayerPlacement && view.favorites() == std::vector<EntityRef>{father}, "death erased reference/source/favorite");
      const auto childLinks = world.follow_links(*birth.created); check(childLinks.size() == 3 && world.follow_links(ref(Kind::Document, 13)).front().ref == father, "death broke lineage/author links");
      const auto garment = *world.inspect(ref(Kind::Garment, 11)); check(garment.active && garment.inventoryUnits == 1 && garment.links.size() == 3 && world.compare({ref(Kind::World, 1)}).front().itemUnits == stockBefore.itemUnits, "person clear destroyed owned/custodied inventory");
      const auto death = world.snapshot().history.back(); check(death.action == "player_clear_death" && world.inspect(death.subjects.front()).has_value() && world.search({"Father", Kind::Person, true, {}}).size() == 1 && world.search({"Father", Kind::Person, false, {}}).empty() && world.validate(), "archived history search failed");
      const auto afterDeath = world.snapshot(); check(send(world, 12, Birth{"After death", father, mother, Sex::Male}, Authority::Scheduler).status == Status::Dependency && world.snapshot() == afterDeath, "archived parent reproduced");
    });
    test("comparison deduplicates persons objects and ownership instead of adding memberships", [] {
      auto world = fixture(); const auto person = place(world, 1); relate(world, 2, person, Relation::Nationality, ref(Kind::State, 2)); relate(world, 3, person, Relation::Language, ref(Kind::Language, 3)); relate(world, 4, person, Relation::Employer, ref(Kind::Enterprise, 4));
      relate(world, 5, ref(Kind::StockBatch, 12), Relation::Owner, person); relate(world, 6, ref(Kind::StockBatch, 12), Relation::Custodian, person);
      const auto rows = world.compare({ref(Kind::State, 2), ref(Kind::State, 14), ref(Kind::State, 2)}); check(rows.size() == 2 && rows[0].livePeople == 1 && rows[0].itemUnits == 40 && rows[1].livePeople == 0 && rows[0].revision == world.snapshot().revision, "state comparison double-counted rows/people/custody");
      const auto global = world.compare({ref(Kind::World, 1)}).front(); check(global.livePeople == 1 && global.physicalItems == 2 && global.itemUnits == 41 && global.day == world.snapshot().day, "world counts summed overlapping groups");
      invalid([&] { (void)world.compare({ref(Kind::State, 2), ref(Kind::Language, 3)}); }, "different metric profiles compared");
      invalid([&] { (void)world.compare({ref(Kind::Building, 6)}); }, "unimplemented profile silently returned fake zero");
    });
    test("clear road cannot break existing protected access and refusal is atomic", [] {
      auto extra = record(Kind::Road, 99, "Unprotected road", Position{1, 0}); auto world = fixture();
      const auto before = world.snapshot();
      check(send(world, 1, Clear{ClearCategory::Roads, {ref(Kind::Road, 5)}}).status == Status::Dependency && world.snapshot() == before, "protected road cleared");
      World other(worldId, {{{0, 0}, {}}, {{1, 0}, {}}}, {extra}); check(send(other, 1, Clear{ClearCategory::Roads, {extra.ref, extra.ref}}).status == Status::Applied && !other.inspect(extra.ref)->active, "unprotected road cannot clear or duplicated clear counted twice");
      auto freeRoad = record(Kind::Road, 2, "Free", Position{0, 0}); auto blockedRoad = record(Kind::Road, 3, "Blocked", Position{1, 0}); auto house = record(Kind::Building, 4, "House", Position{0, 0}); house.protectedAccess = {{1, 0}};
      World batch(worldId, {{{0, 0}, {}}, {{1, 0}, {}}}, {freeRoad, blockedRoad, house}); const auto batchBefore = batch.snapshot();
      check(send(batch, 1, Clear{ClearCategory::Roads, {freeRoad.ref, blockedRoad.ref}}).status == Status::Dependency && batch.snapshot() == batchBefore, "batch erased unprotected road before protected failure");
    });
    test("occupied or stocked buildings require real relocation before clear", [] {
      auto world = fixture(); const auto person = place(world, 1); relate(world, 2, person, Relation::Home, ref(Kind::Building, 6)); const auto before = world.snapshot();
      check(send(world, 3, Clear{ClearCategory::Buildings, {ref(Kind::Building, 6)}}).status == Status::Dependency && world.snapshot() == before, "occupied building cleared");
      check(send(world, 4, Clear{ClearCategory::Buildings, {ref(Kind::Building, 7)}}).status == Status::Dependency && world.snapshot() == before, "warehouse clear destroyed unrelated inventory");
      check(send(world, 5, Clear{ClearCategory::People, {person}}).status == Status::Applied && send(world, 6, Clear{ClearCategory::Buildings, {ref(Kind::Building, 6)}}).status == Status::Applied, "unoccupied empty building clear failed");
      check(world.inspect(person)->links.front().target == ref(Kind::Building, 6) && !world.inspect(ref(Kind::Building, 6))->active && world.validate(), "clear removed archived residence history");
    });
    test("categorized vegetation and deposits clear without changing surface height scale or stocks", [] {
      auto world = fixture(); const auto before = world.snapshot(); const auto stock = world.compare({ref(Kind::World, 1)}).front();
      check(send(world, 1, Clear{ClearCategory::Trees, {ref(Kind::Plant, 9)}}).status == Status::Invalid && world.snapshot() == before, "tree filter cleared other vegetation");
      check(send(world, 2, Clear{ClearCategory::Trees, {ref(Kind::Plant, 8)}}).status == Status::Applied && send(world, 3, Clear{ClearCategory::Plants, {ref(Kind::Plant, 9)}}).status == Status::Applied && send(world, 4, Clear{ClearCategory::Minerals, {ref(Kind::MineralDeposit, 10)}}).status == Status::Applied, "supported category clear failed");
      const auto after = world.snapshot(); check(after.cells == before.cells && after.worldScale == before.worldScale && world.compare({ref(Kind::World, 1)}).front().itemUnits == stock.itemUnits, "deposit clear flattened land or stole mined goods");
      check(send(world, 5, Clear{ClearCategory::Minerals, {ref(Kind::World, 1)}}).status == Status::Invalid && world.validate(), "natural world geometry cleared");
    });
    test("extreme values cross-world identities and a second writer cannot mutate world", [] {
      auto world = fixture(); const auto before = world.snapshot();
      check(send(world, 1, Advance{std::numeric_limits<std::uint64_t>::max()}, Authority::Scheduler).status == Status::Invalid && world.snapshot() == before, "overflow advance committed");
      check(send(world, 2, Clear{ClearCategory::Trees, {{999, Kind::Plant, 8}}}).status == Status::NotFound && world.snapshot() == before, "cross-world clear succeeded");
      Status result = Status::Applied; int readsRejected = 0;
      std::thread other([&] {
        result = world.apply({3, 0, PlacePerson{"Other writer", {0, 0}, Sex::Male}}, Authority::Player).status;
        const auto reject = [&](auto read) { try { read(); } catch (const std::logic_error&) { ++readsRejected; } };
        reject([&] { (void)world.snapshot(); }); reject([&] { (void)world.inspect(ref(Kind::World, 1)); }); reject([&] { (void)world.search({}); });
        reject([&] { (void)world.follow_links(ref(Kind::World, 1)); }); reject([&] { (void)world.compare({ref(Kind::World, 1)}); }); reject([&] { (void)world.validate(); });
      }); other.join();
      check(result == Status::WrongWriter && readsRejected == 6 && world.snapshot() == before, "second thread raced world reads or mutation");
      invalid([&] { World malformed(worldId, {{{0, 0}, {}}}, {}, std::numeric_limits<double>::infinity()); }, "infinite world scale admitted");
      auto huge = record(Kind::StockBatch, 2, "Huge"); huge.inventoryUnits = std::numeric_limits<std::uint64_t>::max();
      invalid([&] { World malformed(worldId, {{{0, 0}, {}}}, {huge}); }, "overflow material total admitted");
    });
    test("every allocation boundary of a transaction preserves atomicity and retry identity", [] {
      bool finished = false; int failures = 0;
      for (long allocation = 0; allocation < 1000; ++allocation) {
        auto world = fixture(); const auto before = world.snapshot(); const Command command{1, before.revision, PlacePerson{"Allocation", {0, 0}, Sex::Male}};
        failAllocationAfter = allocation; const auto result = world.apply(command, Authority::Player); failAllocationAfter = -1;
        if (result.status == Status::Applied) { finished = true; check(world.apply(command, Authority::Player) == result && world.validate(), "retry changed successful allocation result"); break; }
        check(result.status == Status::AllocationFailure && world.snapshot() == before, "allocation failure leaked partial population/history/revision");
        ++failures; const auto retry = world.apply(command, Authority::Player); check(retry.status == Status::Applied && world.compare({ref(Kind::World, 1)}).front().livePeople == 1 && world.validate(), "allocation retry lost id or created duplicate");
      }
      check(finished && failures > 20, "allocation test did not traverse actual transaction writes");
    });
    std::cout << passed << " interaction scenarios passed\n";
    return 0;
  } catch (const std::exception& error) {
    failAllocationAfter = -1; std::cerr << "FAIL " << error.what() << '\n'; return 1;
  }
}
