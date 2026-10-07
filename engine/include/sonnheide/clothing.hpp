#pragma once
#include <cstdint>
#include <map>
#include <set>
#include <string>
#include <vector>

namespace sonnheide::clothing {
using Id = std::uint64_t;
using Units = std::int64_t;
enum class Era { Primitive, Ancient, Later };
enum class Slot { Lower, Upper };
enum class Product { Underpants, Bra, Tee, Shorts, CasualTop, CasualBottom, FormalTop, FormalBottom, UniformTop, UniformBottom };
enum class OwnerKind { Workshop, Person, Employer };
enum class Place { Stock, Reserved, Transit, Wardrobe, Worn, InstitutionStock, Retired };
enum class Need { Basic, FormalPriority, WorkSupply, Preference };
enum class Standard { None, Formal, Uniform };
struct Owner { OwnerKind kind; Id id; auto operator<=>(const Owner&) const = default; };
struct Person {
  Id id{}; bool female{}; bool civilized{}; Era era{Era::Primitive}; Id nationality{};
  Units cash{}; Units foodReserve{}; Units housingReserve{};
  unsigned wardrobeLimit{6}; unsigned preferredStyle{};
};
struct Workshop { Id id{}; bool communal{}; Units cash{}; Units fibre{}; Units fabric{}; Units dye{}; Units labourMinutes{}; Era era{Era::Primitive}; bool formalDesign{}; bool uniformDesign{}; };
struct Employer { Id id{}; Units cash{}; Standard standard{Standard::None}; Id servingCountry{}; bool formalKnowledge{}; bool suppliesFormal{}; };
struct Garment {
  Id id{}; Product product{}; unsigned style{}; Id themeCountry{};
  Owner owner{}; Place place{Place::Stock}; Id location{};
  Units condition{10000}; Units repairAdded{}; Units decayRemainder{}; Units price{};
};
struct Demand {
  Id person{}; Product product{}; Need need{}; unsigned style{}; Id themeCountry{}; Owner payer{};
  auto operator<=>(const Demand&) const = default;
};
struct Order { Id id{}; Id garment{}; Demand demand{}; Id seller{}; Units escrow{}; bool inTransit{}; bool complete{}; bool cancelled{}; Id carrier{}; };
struct Totals { Units fabricMade{}; Units garmentsMade{}; Units fabricConsumed{}; Units minutesConsumed{}; Units payments{}; Units retired{}; Units wages{}; Units dyeConsumed{}; };
struct Recipe { Slot slot; Units fabric; Units minutes; Units price; };
Recipe recipe(Product product);
constexpr Units replacementCondition = 2500;

// Single-writer, headless economic oracle; not the world's command/save integration.
// Prices/rates are initial game calibration. Two body slots, no layered wardrobe/3D fitting.
class Ledger {
public:
  void add_person(Person person);
  void add_workshop(Workshop workshop);
  void add_employer(Employer employer);
  bool set_civilization(Id person, bool civilized, Era era);
  bool set_preference(Id person, unsigned style);
  bool assign_employer(Id person, Id employer);
  bool set_standard(Id employer, Standard standard, Id servingCountry, bool formalKnowledge);
  bool adopt(Id workshop, Era era, bool formalDesign, bool uniformDesign); // Trusted blueprint/adoption event.
  bool supply(Id workshop, Units fibre, Units dye); // Trusted external material handoff, never creates clothes.
  bool work(Id workshop, Id person, Units minutes, Units wagePerMinute);
  bool weave(Id workshop, Units fabric);
  Id sew(Id workshop, Product product, unsigned style = 0, Id themeCountry = 0);
  std::vector<Demand> demands(Id person) const;
  Id reserve(const Demand& demand, Id garment);
  bool dispatch(Id order, Id carrier);
  bool deliver(Id order);
  bool cancel(Id order);
  bool return_order(Id order, Id carrier); // Physical source receipt before refund of in-transit goods.
  bool return_issued(Id person, Id garment, Id employer); // Trusted issuer receipt; required before changing issuer.
  bool equip(Id person, Id garment);
  bool retire(Id person, Id garment);
  bool repair(Id person, Id garment, Id workshop);
  bool advance(Units minutes);
  bool work_admitted(Id person) const; // Clothing never changes admission, attendance, or wages.
  bool dress_compliant(Id person) const;
  bool validate() const;
  Units minute() const { return minute_; }
  const auto& people() const { return people_; }
  const auto& workshops() const { return workshops_; }
  const auto& employers() const { return employers_; }
  const auto& garments() const { return garments_; }
  const auto& orders() const { return orders_; }
  const Totals& totals() const { return totals_; }
private:
  Units* cash(Owner owner);
  Units spendable(const Person& person) const;
  unsigned occupied(Id person) const;
  bool owned_for(Id person, const Garment& garment) const;
  bool appropriate(const Person& person, const Garment& garment, Slot slot) const;
  bool available(Id person, Slot slot, const Demand* exact = nullptr) const;
  const Garment* worn(Id person, Slot slot) const;
  bool pending(Id person, Slot slot) const;
  Units minute_{}; Id nextGarment_{1}; Id nextOrder_{1}; Totals totals_{};
  std::map<Id, Person> people_; std::map<Id, Workshop> workshops_; std::map<Id, Employer> employers_;
  std::map<Id, Id> employment_; std::map<Id, Garment> garments_; std::map<Id, Order> orders_;
  std::set<Id> live_; std::map<Id, std::pair<Units, Units>> dailyWork_;
  std::set<Id> activeOrders_;
  Units moneyIntroduced_{}; Units fibreIntroduced_{}; Units fabricIntroduced_{}; Units dyeIntroduced_{}; Units labourIntroduced_{};
};
} // namespace sonnheide::clothing
