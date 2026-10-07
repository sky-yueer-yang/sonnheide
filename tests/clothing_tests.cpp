#include "sonnheide/clothing.hpp"
#include <algorithm>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>

using namespace sonnheide::clothing;
namespace {
void check(bool ok, const char* reason) { if (!ok) throw std::runtime_error(reason); }
Person person(Id id = 1, Era era = Era::Ancient, bool female = false, Units money = 10000) {
  return {id, female, true, era, 7, money, 100, 100, 6, 0};
}
Ledger fixture(Era era = Era::Ancient, bool communal = false) {
  Ledger book; book.add_person(person(1, era)); book.add_workshop({10, communal, 10000, 10000, 500, 1000, 10000, era, era == Era::Later, era == Era::Later}); return book;
}
Demand need(const Ledger& book, Product p, Id buyer = 1) {
  const auto choices = book.demands(buyer);
  const auto it = std::find_if(choices.begin(), choices.end(), [p](const auto& d) { return d.product == p; });
  if (it == choices.end()) throw std::runtime_error("requested clothing need not present"); return *it;
}
Id make(Ledger& book, const Demand& d) { const Id g = book.sew(10, d.product, d.style, d.themeCountry); check(g != 0, "actual materials/recipe could not make garment"); return g; }
Id buy(Ledger& book, Demand d, bool wear = true) {
  const Id g = make(book, d); const Id o = book.reserve(d, g); check(o != 0, "physical purchase rejected");
  check(!book.equip(d.person, g), "reserved stock was worn before delivery");
  check(book.dispatch(o, 99) && book.deliver(o), "physical dispatch/delivery failed");
  if (wear) check(book.equip(d.person, g), "delivered outfit cannot be worn"); return g;
}
void basics(Ledger& book) {
  const auto initial = book.demands(1);
  for (const auto& d : initial) if (d.need == Need::Basic) (void)buy(book, d);
}
}
int main() {
  int passed = 0;
  const auto test = [&](const char* name, auto body) { body(); ++passed; std::cout << "PASS " << name << '\n'; };
  try {
    test("civilization changes needs without creating or magically dressing clothes", [] {
      auto book = fixture(Era::Primitive, true); check(book.set_civilization(1, false, Era::Primitive), "uncivilized change failed");
      check(book.demands(1).empty() && book.garments().empty(), "culture inferred from nationality or clothes created");
      check(book.set_civilization(1, true, Era::Primitive), "civilization adoption failed");
      check(book.demands(1).size() == 1 && book.garments().empty(), "male primitive outfit incorrect");
      (void)buy(book, need(book, Product::Underpants));
      const auto cash = book.people().at(1).cash; check(book.set_civilization(1, true, Era::Ancient), "era transition failed");
      check(book.demands(1).size() == 2 && book.garments().size() == 1 && book.people().at(1).cash == cash, "era gifted T-shirt");
      check(book.sew(10, Product::Tee) == 0, "workshop skipped adoption");
      check(book.adopt(10, Era::Ancient, false, false), "basic recipe adoption failed");
      basics(book); check(book.demands(1).empty() && book.validate(), "basic industry depended on advanced textile invention");
    });
    test("female primitive coverage and no-money communal production consume resources", [] {
      Ledger book; auto p = person(1, Era::Primitive, true, 0); p.foodReserve = 0; p.housingReserve = 0; book.add_person(p);
      book.add_workshop({10, true, 0, 10, 0, 0, 0}); check(book.demands(1).size() == 2, "primitive chest/lower requirements absent");
      check(book.sew(10, Product::Bra) == 0, "no-resource sewing created clothes");
      check(book.work(10, 1, 40, 0) && book.weave(10, 2), "actual unpaid communal weaving failed"); basics(book);
      check(book.people().at(1).cash == 0 && book.workshops().at(10).fibre == 6 && book.totals().fabricConsumed == 2 && book.validate(), "free distribution also generated free inputs");
    });
    test("one physical owner/location through reservation transport delivery and cancellation", [] {
      auto book = fixture(); const auto d = need(book, Product::Tee); const auto g = make(book, d); const auto before = book.people().at(1).cash;
      const auto o = book.reserve(d, g); check(o && !book.reserve(d, g), "same stock sold twice");
      check(book.garments().at(g).owner == Owner{OwnerKind::Workshop, 10} && book.people().at(1).cash == before - 50, "escrow ownership incorrect");
      check(book.dispatch(o, 99) && !book.deliver(o + 1), "transport not authoritative");
      check(!book.cancel(o) && !book.return_order(o, 98) && book.return_order(o, 99) && !book.return_order(o, 99) && book.people().at(1).cash == before && book.garments().at(g).place == Place::Stock, "in-transit cancellation teleported stock or return did not refund once");
      const auto o2 = book.reserve(d, g); check(book.dispatch(o2, 99) && book.deliver(o2) && !book.deliver(o2), "double delivery succeeded");
      check(book.orders().at(o2).complete && !book.orders().at(o2).inTransit && book.orders().at(o2).escrow == 0, "delivered order still reported transport or held money");
      check(book.garments().at(g).owner == Owner{OwnerKind::Person, 1} && book.validate(), "delivery violated inventory/money conservation");
    });
    test("stored delivered essentials and formal outfits suppress duplicate orders", [] {
      auto book = fixture(Era::Later); (void)buy(book, need(book, Product::Tee), false); (void)buy(book, need(book, Product::Shorts), false);
      check(book.demands(1).empty(), "unworn available basic stock bought repeatedly");
      book.add_employer({20, 0, Standard::Formal, 0, true}); check(book.assign_employer(1, 20), "assignment failed");
      (void)buy(book, need(book, Product::FormalTop), false); (void)buy(book, need(book, Product::FormalBottom), false);
      check(book.demands(1).empty() && !book.dress_compliant(1) && book.work_admitted(1) && book.validate(), "stored formal outfit repeated or absence blocked work");
    });
    test("formal policy only raises voluntary purchase priority without pay or admission penalty", [] {
      auto book = fixture(Era::Later); basics(book); check(book.set_preference(1, 8), "preference failed");
      check(book.demands(1).front().need == Need::Preference, "late variety demand absent");
      book.add_employer({20, 0, Standard::None, 0, false}); check(book.assign_employer(1, 20), "job unavailable");
      check(!book.set_standard(20, Standard::Formal, 0, false), "uninvented policy enabled");
      check(book.set_standard(20, Standard::Formal, 0, true), "stock-free adoption rejected");
      const auto ds = book.demands(1); check(ds.size() == 2 && ds[0].need == Need::FormalPriority && ds[0].payer == Owner{OwnerKind::Person, 1}, "formal requirement forced employer payment or stayed below novelty");
      const auto cash = book.people().at(1).cash; check(book.work_admitted(1) && !book.dress_compliant(1), "formal absence blocked work");
      check(book.work(10, 1, 10, 3) && book.people().at(1).cash == cash + 30, "formal absence deducted wages");
      check(book.advance(20 * 365 * 1440LL) && book.work_admitted(1), "grace expiry blocked employment");
    });
    test("clothing cannot spend food housing reserves and finite budget shows shortage", [] {
      Ledger book; book.add_person(person(1, Era::Later, false, 225)); book.add_workshop({10, false, 1000, 0, 10, 10, 1000, Era::Later, true, false});
      const auto tee = need(book, Product::Tee); const auto g = make(book, tee); check(book.reserve(tee, g) == 0 && book.people().at(1).cash == 225, "clothing stole food/housing budget");
      book.add_employer({20, 0, Standard::Formal, 0, true}); check(book.assign_employer(1, 20) && book.work_admitted(1), "poor worker lost employment");
      const auto formal = need(book, Product::FormalTop); const auto fg = make(book, formal); check(book.reserve(formal, fg) == 0 && book.validate(), "formal priority forced unaffordable purchase");
    });
    test("civilian national shirt and issued uniform serving identity are distinct", [] {
      auto book = fixture(Era::Later); const auto tee = buy(book, need(book, Product::Tee)); (void)buy(book, need(book, Product::Shorts));
      check(book.garments().at(tee).themeCountry == 7, "basic national shirt lost theme binding");
      book.add_employer({20, 1000, Standard::Uniform, 42, false}); book.add_employer({21, 1000, Standard::Uniform, 43, false});
      check(book.sew(10, Product::UniformTop, 2, 42) == 0, "country-specific uniform cut variation bypassed common design");
      check(book.assign_employer(1, 20), "army entry failed"); const auto top = buy(book, need(book, Product::UniformTop)); const auto bottom = buy(book, need(book, Product::UniformBottom));
      check(book.dress_compliant(1) && book.garments().at(top).themeCountry == 42 && book.people().at(1).nationality == 7, "foreign service changed citizenship or uniform theme");
      check(!book.assign_employer(1, 21) && book.return_issued(1, top, 20) && book.return_issued(1, bottom, 20) && book.assign_employer(1, 21) && !book.dress_compliant(1), "old issue vanished on reassignment or satisfied new serving country");
      check(book.garments().at(top).place == Place::InstitutionStock && book.garments().at(top).themeCountry == 42 && !book.equip(1, top) && book.validate(), "changing employer recolored or stole issued stock");
    });
    test("integer wear is partition invariant including carry across worn and stored modes", [] {
      auto a = fixture(); const auto g = buy(a, need(a, Product::Tee)); auto b = a;
      check(a.advance(123456), "long wear failed"); for (int i = 0; i < 128; ++i) check(b.advance(964), "partition wear failed"); check(b.advance(64), "wear remainder failed");
      check(a.garments().at(g).condition == b.garments().at(g).condition && a.garments().at(g).decayRemainder == b.garments().at(g).decayRemainder, "step size changes durability");
      auto stored = fixture(); const auto sg = buy(stored, need(stored, Product::Tee), false); check(stored.advance(123456) && stored.garments().at(sg).condition > a.garments().at(g).condition, "wardrobe wears as fast as body");
      check(a.validate() && b.validate() && stored.validate(), "durability corrupted inventory");
    });
    test("delivery preserves aged condition and bounded repairs never restore infinite life", [] {
      auto book = fixture(); const auto d = need(book, Product::Tee); const auto g = make(book, d); check(book.advance(500 * 1440LL), "stock age failed");
      const auto aged = book.garments().at(g).condition; const auto o = book.reserve(d, g); check(book.dispatch(o, 99) && book.deliver(o) && book.garments().at(g).condition == aged, "delivery reset durability");
      check(book.equip(1, g) && book.advance(170 * 1440LL), "wear did not occur");
      const auto replacement = buy(book, need(book, Product::Tee)); (void)replacement;
      check(book.repair(1, g, 10) && book.repair(1, g, 10) && !book.repair(1, g, 10) && book.garments().at(g).repairAdded == 3000, "repair cap allowed immortal garment");
      check(book.advance(100 * 366 * 1440LL) && book.garments().at(g).place == Place::Retired && book.work_admitted(1) == false && book.validate(), "old clothing regenerated after depletion");
    });
    test("daily work input and wardrobe are finite rather than unbounded cash/stock exploits", [] {
      auto book = fixture(Era::Later); check(book.work(10, 1, 480, 0) && !book.work(10, 1, 1, 100), "labour input exceeded person day budget");
      basics(book); check(book.set_preference(1, 1), "style absent");
      for (unsigned style = 1; style <= 4; ++style) { check(book.set_preference(1, style), "style update failed"); (void)buy(book, need(book, Product::CasualTop)); }
      check(book.demands(1).empty() && book.validate(), "bounded wardrobe kept buying optional clothes");
    });
    test("expired transit escrow returns only with a real source receipt", [] {
      auto book = fixture(); const auto d = need(book, Product::Tee); const auto g = make(book, d); const auto cash = book.people().at(1).cash;
      const auto order = book.reserve(d, g); check(book.dispatch(order, 10), "carrier sharing numeric id namespace failed");
      check(book.advance(6000 * 1440LL) && book.garments().at(g).place == Place::Retired && !book.deliver(order) && !book.cancel(order) && book.validate(), "expired transport reset or released escrow without receipt");
      check(book.return_order(order, 10) && book.people().at(1).cash == cash && book.garments().at(g).place == Place::Retired && book.validate(), "expired physical return failed to reconcile escrow");
    });
    test("raw material shortage preserves demand without automatic restock or lethal consequence", [] {
      Ledger book; book.add_person(person()); book.add_workshop({10, false, 1000, 8, 0, 2, 0, Era::Ancient});
      for (const auto& d : book.demands(1)) { const auto r = recipe(d.product); check(book.work(10, 1, 40, 1) && book.weave(10, r.fabric), "first real production inputs failed"); (void)buy(book, d); }
      check(book.advance(200 * 1440LL), "replacement time failed");
      check(book.demands(1).size() == 2 && !book.weave(10, 1) && !book.sew(10, Product::Shorts) && book.people().contains(1) && book.validate(), "raw shortage removed need, killed wearer, or created free resources");
    });
    test("active issued shipment must settle physically before changing institution", [] {
      auto book = fixture(Era::Later); basics(book); book.add_employer({20, 1000, Standard::Uniform, 42, false}); book.add_employer({21, 1000, Standard::Uniform, 43, false});
      check(book.assign_employer(1, 20), "first service assignment failed"); const auto d = need(book, Product::UniformTop); const auto g = make(book, d); const auto o = book.reserve(d, g);
      check(book.dispatch(o, 99) && !book.assign_employer(1, 21) && book.validate(), "changing issuer stranded active escrow");
      check(book.return_order(o, 99) && book.assign_employer(1, 21) && book.garments().at(g).place == Place::Stock && book.garments().at(g).themeCountry == 42 && book.validate(), "physical return recolored old issuer shipment");
    });
    test("employee cannot discard institutional garments to bypass physical return", [] {
      auto book = fixture(Era::Later); basics(book);
      book.add_employer({20, 1000, Standard::Uniform, 42, false}); book.add_employer({21, 1000, Standard::Uniform, 43, false});
      check(book.assign_employer(1, 20), "uniform issuer unavailable");
      const auto top = buy(book, need(book, Product::UniformTop));
      const auto bottom = buy(book, need(book, Product::UniformBottom));
      check(!book.retire(1, top) && book.garments().at(top).condition == 10000 && book.garments().at(top).place == Place::Worn, "employee destroyed another owner's intact asset");
      check(!book.assign_employer(1, 21) && book.work_admitted(1) && book.validate(), "discard bypassed issuer return or removed work admission");
      check(book.return_issued(1, top, 20) && book.return_issued(1, bottom, 20) && book.assign_employer(1, 21) && book.validate(), "actual issuer receipt failed to permit transfer");
    });
    test("initial style must be producible within the same bounded catalog as later preferences", [] {
      Ledger book; auto p = person(); p.preferredStyle = std::numeric_limits<unsigned>::max(); bool rejected = false;
      try { book.add_person(p); } catch (const std::invalid_argument&) { rejected = true; }
      check(rejected && book.people().empty() && book.validate(), "unproducible initial style created an endless invalid need");
      p.preferredStyle = 1024; book.add_person(p);
      check(book.people().at(1).preferredStyle == 1024 && !book.set_preference(1, 1025) && book.validate(), "valid style boundary disagreed between creation and setter");
    });
    test("hundred people century closed money/material production replacement cycle", [] {
      Ledger book; for (Id id = 1; id <= 100; ++id) book.add_person(person(id, Era::Ancient, id % 2 == 0, 10000));
      book.add_workshop({10, false, 10000, 200000, 0, 50000, 0, Era::Ancient});
      Units perYear = 0; Units minimumYear = 1000000; Units completedYears = 0;
      for (int month = 0; month < 1200; ++month) {
        for (Id id = 1; id <= 100; ++id) {
          const auto ds = book.demands(id);
          for (const auto& d : ds) {
            check(d.need == Need::Basic, "century scenario injected novelty demand"); const auto r = recipe(d.product);
            check(book.work(10, id, r.fabric * 5 + r.minutes, 1) && book.weave(10, r.fabric), "real wages/raw inputs ran out unexpectedly");
            const auto g = make(book, d); const auto o = book.reserve(d, g);
            check(o && book.dispatch(o, 99) && book.deliver(o) && book.equip(id, g), "replacement was not purchased/delivered/worn"); ++perYear;
          }
        }
        std::vector<std::pair<Id, Id>> retirements;
        for (const auto& [id, g] : book.garments()) if (g.place == Place::Wardrobe && g.owner.kind == OwnerKind::Person && g.condition <= replacementCondition) retirements.emplace_back(g.location, id);
        for (const auto& [p, g] : retirements) check(book.retire(p, g), "discarding worn-out spare failed");
        check(book.advance(30 * 1440), "century clock failed");
        if (month % 12 == 11) { check(book.validate(), "century conservation failed"); minimumYear = std::min(minimumYear, perYear); perYear = 0; ++completedYears; }
      }
      check(book.minute() == 36000LL * 1440 && completedYears == 100 && minimumYear > 0 && book.totals().garmentsMade > 30000 && book.totals().wages > 0 && book.totals().payments > 0 && book.workshops().at(10).fibre < 200000 && book.validate(), "industry exhausted demand or manufactured with unlimited injected money/materials");
      std::cout << "CENTURY garments=" << book.totals().garmentsMade << " paid=" << book.totals().payments << " wages=" << book.totals().wages << " fibre_left=" << book.workshops().at(10).fibre << '\n';
    });
    std::cout << "clothing: " << passed << " scenarios passed\n"; return 0;
  } catch (const std::exception& e) { std::cerr << "FAIL " << e.what() << '\n'; return 1; }
}
