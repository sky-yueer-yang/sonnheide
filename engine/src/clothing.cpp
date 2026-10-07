#include "sonnheide/clothing.hpp"
#include <algorithm>
#include <limits>
#include <stdexcept>

namespace sonnheide::clothing {
namespace {
constexpr Units maxQuantity = 1000000000000LL;
bool bounded(Units n) { return n >= 0 && n <= maxQuantity; }
bool valid_product(Product p) { return p >= Product::Underpants && p <= Product::UniformBottom; }
bool valid_era(Era e) { return e >= Era::Primitive && e <= Era::Later; }
bool valid_standard(Standard s) { return s >= Standard::None && s <= Standard::Uniform; }
bool uniform(Product p) { return p == Product::UniformTop || p == Product::UniformBottom; }
bool formal(Product p) { return p == Product::FormalTop || p == Product::FormalBottom; }
bool basic_kind(Product p) { return p == Product::Underpants || p == Product::Bra || p == Product::Tee || p == Product::Shorts; }
bool can_make(const Person& p, Product product) {
  if (!p.civilized) return false;
  if (p.era == Era::Primitive) return product == Product::Underpants || product == Product::Bra;
  if (p.era == Era::Ancient) return basic_kind(product);
  return valid_product(product);
}
Product basic_product(const Person& p, Slot s) {
  if (p.era == Era::Primitive) return s == Slot::Lower ? Product::Underpants : Product::Bra;
  return s == Slot::Lower ? Product::Shorts : Product::Tee;
}
Product work_product(Standard s, Slot slot) {
  if (s == Standard::Uniform) return slot == Slot::Lower ? Product::UniformBottom : Product::UniformTop;
  return slot == Slot::Lower ? Product::FormalBottom : Product::FormalTop;
}
}
Recipe recipe(Product p) {
  if (!valid_product(p)) throw std::invalid_argument("invalid clothing product");
  const bool lower = p == Product::Underpants || p == Product::Shorts || p == Product::CasualBottom || p == Product::FormalBottom || p == Product::UniformBottom;
  const Units fabric = p == Product::Underpants || p == Product::Bra ? 1 : formal(p) || uniform(p) ? 3 : 2;
  return {lower ? Slot::Lower : Slot::Upper, fabric, fabric * 15, fabric * 25};
}
void Ledger::add_person(Person p) {
  if (p.id == 0 || !valid_era(p.era) || !bounded(p.cash) || !bounded(p.foodReserve) || !bounded(p.housingReserve) || p.foodReserve > p.cash || p.housingReserve > p.cash - p.foodReserve || p.wardrobeLimit < 2 || p.wardrobeLimit > 12 || p.preferredStyle > 1024 || people_.contains(p.id) || moneyIntroduced_ > maxQuantity - p.cash) throw std::invalid_argument("invalid person");
  people_.emplace(p.id, p); moneyIntroduced_ += p.cash;
}
void Ledger::add_workshop(Workshop w) {
  if (w.id == 0 || !bounded(w.cash) || !bounded(w.fibre) || !bounded(w.fabric) || !bounded(w.dye) || !bounded(w.labourMinutes) || !valid_era(w.era) || (w.era != Era::Later && (w.formalDesign || w.uniformDesign)) || workshops_.contains(w.id) || moneyIntroduced_ > maxQuantity - w.cash || fibreIntroduced_ > maxQuantity - w.fibre || fabricIntroduced_ > maxQuantity - w.fabric || dyeIntroduced_ > maxQuantity - w.dye || labourIntroduced_ > maxQuantity - w.labourMinutes) throw std::invalid_argument("invalid workshop");
  workshops_.emplace(w.id, w); moneyIntroduced_ += w.cash; fibreIntroduced_ += w.fibre; fabricIntroduced_ += w.fabric; dyeIntroduced_ += w.dye; labourIntroduced_ += w.labourMinutes;
}
void Ledger::add_employer(Employer e) {
  if (e.id == 0 || !bounded(e.cash) || !valid_standard(e.standard) || (e.standard == Standard::Uniform && e.servingCountry == 0) || (e.standard == Standard::Formal && !e.formalKnowledge) || employers_.contains(e.id) || moneyIntroduced_ > maxQuantity - e.cash) throw std::invalid_argument("invalid employer");
  employers_.emplace(e.id, e); moneyIntroduced_ += e.cash;
}
bool Ledger::set_civilization(Id id, bool civilized, Era era) {
  auto it = people_.find(id); if (it == people_.end() || !valid_era(era)) return false;
  it->second.civilized = civilized; it->second.era = era; return true;
}
bool Ledger::set_preference(Id id, unsigned style) {
  auto it = people_.find(id); if (it == people_.end() || style > 1024) return false;
  it->second.preferredStyle = style; return true;
}
bool Ledger::assign_employer(Id p, Id e) {
  if (!people_.contains(p) || !employers_.contains(e)) return false;
  // A new employer cannot take possession of the previous employer's uniform.
  for (const auto id : live_) {
    const auto& g = garments_.at(id);
    if (g.owner.kind == OwnerKind::Employer && g.owner.id != e && g.location == p && (g.place == Place::Worn || g.place == Place::Wardrobe)) return false;
  }
  for (const auto id : activeOrders_) { const auto& o = orders_.at(id); if (o.demand.person == p && o.demand.payer.kind == OwnerKind::Employer && o.demand.payer.id != e) return false; }
  employment_.insert_or_assign(p, e); return true;
}
bool Ledger::set_standard(Id id, Standard s, Id country, bool knowledge) {
  auto it = employers_.find(id);
  if (it == employers_.end() || !valid_standard(s) || (s == Standard::Formal && !knowledge) || (s == Standard::Uniform && country == 0)) return false;
  it->second.standard = s; it->second.servingCountry = country; it->second.formalKnowledge = knowledge; return true;
}
bool Ledger::adopt(Id id, Era era, bool formalDesign, bool uniformDesign) {
  auto it = workshops_.find(id); if (it == workshops_.end() || !valid_era(era) || (era != Era::Later && (formalDesign || uniformDesign))) return false;
  it->second.era = era; it->second.formalDesign = formalDesign; it->second.uniformDesign = uniformDesign; return true;
}
bool Ledger::supply(Id id, Units fibre, Units dye) {
  auto it = workshops_.find(id); if (it == workshops_.end() || !bounded(fibre) || !bounded(dye) || it->second.fibre > maxQuantity - fibre || it->second.dye > maxQuantity - dye || fibreIntroduced_ > maxQuantity - fibre || dyeIntroduced_ > maxQuantity - dye) return false;
  it->second.fibre += fibre; it->second.dye += dye; fibreIntroduced_ += fibre; dyeIntroduced_ += dye; return true;
}
bool Ledger::work(Id w, Id p, Units minutes, Units rate) {
  auto wi = workshops_.find(w); auto pi = people_.find(p);
  if (wi == workshops_.end() || pi == people_.end() || minutes <= 0 || minutes > 480 || !bounded(rate) || rate > maxQuantity / minutes) return false;
  const auto day = minute_ / 1440; const auto previous = dailyWork_.find(p);
  const Units used = previous != dailyWork_.end() && previous->second.first == day ? previous->second.second : 0;
  const Units wage = minutes * rate;
  if (used > 480 - minutes || wi->second.cash < wage || pi->second.cash > maxQuantity - wage || wi->second.labourMinutes > maxQuantity - minutes || labourIntroduced_ > maxQuantity - minutes || totals_.wages > maxQuantity - wage) return false;
  dailyWork_.insert_or_assign(p, std::pair{day, used + minutes});
  wi->second.cash -= wage; pi->second.cash += wage; wi->second.labourMinutes += minutes; totals_.wages += wage; labourIntroduced_ += minutes; return true;
}
bool Ledger::weave(Id id, Units fabric) {
  auto it = workshops_.find(id); if (it == workshops_.end() || fabric <= 0 || fabric > maxQuantity / 5) return false;
  auto& w = it->second;
  if (w.fibre < fabric * 2 || w.labourMinutes < fabric * 5 || w.fabric > maxQuantity - fabric) return false;
  w.fibre -= fabric * 2; w.labourMinutes -= fabric * 5; w.fabric += fabric; totals_.fabricMade += fabric; totals_.minutesConsumed += fabric * 5; return true;
}
Id Ledger::sew(Id id, Product p, unsigned style, Id country) {
  auto it = workshops_.find(id); if (it == workshops_.end() || !valid_product(p) || style > 1024 || (uniform(p) && (country == 0 || style != 0)) || (!uniform(p) && p != Product::Tee && country != 0) || nextGarment_ == std::numeric_limits<Id>::max()) return 0;
  const auto& technology = it->second;
  if ((p == Product::Tee || p == Product::Shorts) && technology.era == Era::Primitive) return 0;
  if (!basic_kind(p) && technology.era != Era::Later) return 0;
  if ((formal(p) && !technology.formalDesign) || (uniform(p) && !technology.uniformDesign)) return 0;
  // Rights/knowledge validation precedes the trusted adopt event in the world integration.
  const auto r = recipe(p); auto& w = it->second; const Units dye = p == Product::Underpants || p == Product::Bra ? 0 : 1;
  if (w.fabric < r.fabric || w.labourMinutes < r.minutes || w.dye < dye) return 0;
  const Id created = nextGarment_; Garment g{}; g.id = created; g.product = p; g.style = style; g.themeCountry = country; g.owner = {OwnerKind::Workshop, id}; g.location = id; g.price = w.communal ? 0 : r.price;
  garments_.emplace(created, g);
  try { live_.insert(created); } catch (...) { garments_.erase(created); throw; }
  w.fabric -= r.fabric; w.labourMinutes -= r.minutes; w.dye -= dye; ++nextGarment_; ++totals_.garmentsMade; totals_.fabricConsumed += r.fabric; totals_.minutesConsumed += r.minutes; totals_.dyeConsumed += dye; return created;
}
Units Ledger::spendable(const Person& p) const { return p.cash - p.foodReserve - p.housingReserve; }
Units* Ledger::cash(Owner o) {
  if (o.kind == OwnerKind::Person) { auto i = people_.find(o.id); return i == people_.end() ? nullptr : &i->second.cash; }
  if (o.kind == OwnerKind::Employer) { auto i = employers_.find(o.id); return i == employers_.end() ? nullptr : &i->second.cash; }
  return nullptr;
}
bool Ledger::owned_for(Id p, const Garment& g) const {
  if (g.owner == Owner{OwnerKind::Person, p}) return true;
  const auto e = employment_.find(p); return e != employment_.end() && g.owner == Owner{OwnerKind::Employer, e->second};
}
unsigned Ledger::occupied(Id p) const {
  unsigned count = 0;
  for (const auto id : live_) { const auto& g = garments_.at(id); if ((g.place == Place::Worn || g.place == Place::Wardrobe) && g.location == p) ++count; }
  for (const auto id : activeOrders_) if (orders_.at(id).demand.person == p) ++count;
  return count;
}
const Garment* Ledger::worn(Id p, Slot s) const {
  for (const auto id : live_) { const auto& g = garments_.at(id); if (g.place == Place::Worn && g.location == p && recipe(g.product).slot == s && owned_for(p, g)) return &g; }
  return nullptr;
}
bool Ledger::appropriate(const Person& p, const Garment& g, Slot s) const {
  if (g.condition <= replacementCondition || recipe(g.product).slot != s) return false;
  if (p.era == Era::Primitive) return true;
  return g.product != Product::Underpants && g.product != Product::Bra;
}
bool Ledger::pending(Id p, Slot slot) const {
  for (const auto id : activeOrders_) { const auto& o = orders_.at(id); if (o.demand.person == p && recipe(o.demand.product).slot == slot) return true; }
  return false;
}
bool Ledger::available(Id p, Slot slot, const Demand* exact) const {
  for (const auto id : live_) {
    const auto& g = garments_.at(id);
    if ((g.place != Place::Worn && g.place != Place::Wardrobe) || g.location != p || !owned_for(p, g) || !appropriate(people_.at(p), g, slot)) continue;
    if (!exact || (g.product == exact->product && g.style == exact->style && g.themeCountry == exact->themeCountry)) return true;
  }
  return false;
}
std::vector<Demand> Ledger::demands(Id id) const {
  std::vector<Demand> result; const auto pi = people_.find(id); if (pi == people_.end() || !pi->second.civilized) return result;
  const auto& p = pi->second; bool basicsMissing = false;
  for (const auto s : {Slot::Lower, Slot::Upper}) {
    if (s == Slot::Upper && p.era == Era::Primitive && !p.female) continue;
    if (!available(id, s)) {
      basicsMissing = true;
      const auto product = basic_product(p, s);
      if (!pending(id, s)) result.push_back({id, product, Need::Basic, 0, product == Product::Tee ? p.nationality : 0, {OwnerKind::Person, id}});
    }
  }
  const auto ei = employment_.find(id);
  if (ei != employment_.end()) {
    const auto& e = employers_.at(ei->second);
    if (e.standard != Standard::None && p.era == Era::Later) for (const auto s : {Slot::Lower, Slot::Upper}) {
      const auto product = work_product(e.standard, s); const auto country = e.standard == Standard::Uniform ? e.servingCountry : 0;
      const bool supplied = e.standard == Standard::Uniform || e.suppliesFormal;
      const Demand demand{id, product, supplied ? Need::WorkSupply : Need::FormalPriority, 0, country, {supplied ? OwnerKind::Employer : OwnerKind::Person, supplied ? e.id : id}};
      if (!available(id, s, &demand) && !pending(id, s)) result.push_back(demand);
    }
  }
  const bool formalMissing = std::any_of(result.begin(), result.end(), [](const Demand& d) { return d.need == Need::FormalPriority || d.need == Need::WorkSupply; });
  if (!basicsMissing && !formalMissing && p.era == Era::Later && p.preferredStyle != 0 && occupied(id) < p.wardrobeLimit) {
    const auto* top = worn(id, Slot::Upper);
    const Demand demand{id, Product::CasualTop, Need::Preference, p.preferredStyle, 0, {OwnerKind::Person, id}};
    if (top && !available(id, Slot::Upper, &demand) && !pending(id, Slot::Upper)) result.push_back(demand);
  }
  return result;
}
Id Ledger::reserve(const Demand& d, Id garment) {
  const auto gi = garments_.find(garment); const auto pi = people_.find(d.person); const auto valid = demands(d.person);
  if (gi == garments_.end() || pi == people_.end() || std::find(valid.begin(), valid.end(), d) == valid.end() || occupied(d.person) >= pi->second.wardrobeLimit || nextOrder_ == std::numeric_limits<Id>::max()) return 0;
  auto& g = gi->second;
  if (g.place != Place::Stock || g.owner.kind != OwnerKind::Workshop || g.product != d.product || g.style != d.style || g.themeCountry != d.themeCountry || g.condition <= replacementCondition || !can_make(pi->second, g.product)) return 0;
  auto* payer = cash(d.payer); if (!payer || *payer < g.price || (d.payer.kind == OwnerKind::Person && spendable(pi->second) < g.price)) return 0;
  if (workshops_.at(g.owner.id).communal && d.need != Need::Basic) return 0;
  const Id id = nextOrder_; orders_.emplace(id, Order{id, garment, d, g.owner.id, g.price, false, false, false});
  try { activeOrders_.insert(id); } catch (...) { orders_.erase(id); throw; }
  *payer -= g.price; g.place = Place::Reserved; ++nextOrder_; return id;
}
bool Ledger::dispatch(Id id, Id carrier) {
  auto it = orders_.find(id); if (it == orders_.end() || carrier == 0 || it->second.complete || it->second.cancelled || it->second.inTransit) return false;
  auto& g = garments_.at(it->second.garment); if (g.place != Place::Reserved || g.condition <= replacementCondition) return false;
  it->second.inTransit = true; it->second.carrier = carrier; g.place = Place::Transit; g.location = carrier; return true;
}
bool Ledger::deliver(Id id) {
  auto it = orders_.find(id); if (it == orders_.end() || !it->second.inTransit || it->second.complete || it->second.cancelled) return false;
  auto& o = it->second; auto& g = garments_.at(o.garment); auto& seller = workshops_.at(o.seller);
  if (g.place != Place::Transit || g.condition <= replacementCondition || seller.cash > maxQuantity - o.escrow || totals_.payments > maxQuantity - o.escrow) return false;
  if (o.demand.payer.kind == OwnerKind::Employer && (!employment_.contains(o.demand.person) || employment_.at(o.demand.person) != o.demand.payer.id)) return false;
  seller.cash += o.escrow; totals_.payments += o.escrow; o.complete = true; o.inTransit = false; o.escrow = 0; activeOrders_.erase(id); g.owner = o.demand.payer; g.place = Place::Wardrobe; g.location = o.demand.person; return true;
}
bool Ledger::cancel(Id id) {
  auto it = orders_.find(id); if (it == orders_.end() || it->second.complete || it->second.cancelled || it->second.inTransit) return false;
  auto& o = it->second; auto* payer = cash(o.demand.payer); if (!payer || *payer > maxQuantity - o.escrow) return false;
  *payer += o.escrow; o.escrow = 0; o.cancelled = true; activeOrders_.erase(id); auto& g = garments_.at(o.garment);
  if (g.condition > 0) { g.place = Place::Stock; g.location = o.seller; } return true;
}
bool Ledger::return_order(Id id, Id carrier) {
  auto it = orders_.find(id); if (it == orders_.end() || !it->second.inTransit || it->second.complete || it->second.cancelled) return false;
  auto& g = garments_.at(it->second.garment);
  if (carrier == 0 || carrier != it->second.carrier || g.location != carrier || (g.place != Place::Transit && g.place != Place::Retired)) return false;
  auto* payer = cash(it->second.demand.payer); if (!payer || *payer > maxQuantity - it->second.escrow) return false;
  g.location = it->second.seller; if (g.condition > 0) g.place = Place::Reserved;
  it->second.inTransit = false; return cancel(id);
}
bool Ledger::return_issued(Id p, Id id, Id issuer) {
  auto it = garments_.find(id); if (it == garments_.end()) return false; auto& g = it->second;
  if (!people_.contains(p) || !employers_.contains(issuer) || g.owner != Owner{OwnerKind::Employer, issuer} || g.location != p || (g.place != Place::Wardrobe && g.place != Place::Worn)) return false;
  g.place = Place::InstitutionStock; g.location = issuer; return true;
}
bool Ledger::equip(Id p, Id id) {
  auto it = garments_.find(id); if (!people_.contains(p) || it == garments_.end()) return false; auto& g = it->second;
  if (g.place != Place::Wardrobe || g.location != p || !owned_for(p, g) || g.condition <= 0) return false;
  const auto slot = recipe(g.product).slot;
  for (const auto other : live_) { auto& old = garments_.at(other); if (old.place == Place::Worn && old.location == p && recipe(old.product).slot == slot) old.place = Place::Wardrobe; }
  g.place = Place::Worn; return true;
}
bool Ledger::retire(Id p, Id id) {
  auto it = garments_.find(id); if (it == garments_.end() || it->second.owner != Owner{OwnerKind::Person, p} || it->second.location != p || (it->second.place != Place::Wardrobe && it->second.place != Place::Worn)) return false;
  it->second.place = Place::Retired; live_.erase(id); ++totals_.retired; return true;
}
bool Ledger::repair(Id p, Id id, Id workshop) {
  auto gi = garments_.find(id); auto wi = workshops_.find(workshop);
  if (gi == garments_.end() || wi == workshops_.end() || gi->second.place != Place::Wardrobe || gi->second.location != p || !owned_for(p, gi->second)) return false;
  auto& g = gi->second; auto& w = wi->second;
  const Units added = std::min({Units{2000}, Units{3000} - g.repairAdded, Units{10000} - g.condition});
  const Units fabric = (added + 999) / 1000; const Units labour = fabric * 10; const Units price = fabric * 10; auto* payer = cash(g.owner);
  if (added <= 0 || !payer || *payer < price || w.cash > maxQuantity - price || w.fabric < fabric || w.labourMinutes < labour || totals_.payments > maxQuantity - price || (g.owner.kind == OwnerKind::Person && spendable(people_.at(p)) < price)) return false;
  *payer -= price; w.cash += price; w.fabric -= fabric; w.labourMinutes -= labour; g.condition += added; g.repairAdded += added; totals_.fabricConsumed += fabric; totals_.minutesConsumed += labour; totals_.payments += price; return true;
}
bool Ledger::advance(Units minutes) {
  if (minutes <= 0 || minutes > 100LL * 366 * 1440 || minute_ > maxQuantity - minutes) return false;
  for (auto it = live_.begin(); it != live_.end();) {
    auto& g = garments_.at(*it); const Units rate = g.place == Place::Worn ? 40 : 2;
    const Units numerator = g.decayRemainder + minutes * rate;
    g.condition = std::max(Units{0}, g.condition - numerator / 1440); g.decayRemainder = numerator % 1440;
    if (g.condition == 0) { g.place = Place::Retired; ++totals_.retired; it = live_.erase(it); } else ++it;
  }
  minute_ += minutes; return true;
}
bool Ledger::work_admitted(Id p) const { return people_.contains(p) && employment_.contains(p); }
bool Ledger::dress_compliant(Id p) const {
  const auto it = employment_.find(p); if (it == employment_.end()) return true;
  const auto& e = employers_.at(it->second); if (e.standard == Standard::None) return true;
  for (const auto s : {Slot::Lower, Slot::Upper}) { const auto* g = worn(p, s); if (!g || g->condition <= replacementCondition || g->product != work_product(e.standard, s) || (e.standard == Standard::Uniform && g->themeCountry != e.servingCountry)) return false; }
  return true;
}
bool Ledger::validate() const {
  std::set<std::pair<Id, Slot>> equipped; std::set<Id> ordered;
  Units money = 0; Units fibre = 0; Units fabric = 0; Units dye = 0; Units labour = 0;
  for (const auto& [id, p] : people_) { (void)id; money += p.cash; }
  for (const auto& [id, w] : workshops_) { (void)id; money += w.cash; fibre += w.fibre; fabric += w.fabric; dye += w.dye; labour += w.labourMinutes; }
  for (const auto& [id, e] : employers_) { (void)id; money += e.cash; }
  for (const auto& [id, g] : garments_) {
    if (id != g.id || !valid_product(g.product) || g.condition < 0 || g.condition > 10000 || g.repairAdded < 0 || g.repairAdded > 3000 || g.decayRemainder < 0 || g.decayRemainder >= 1440 || live_.contains(id) != (g.place != Place::Retired)) return false;
    if ((g.place == Place::Worn || g.place == Place::Wardrobe) && !people_.contains(g.location)) return false;
    if ((g.place == Place::Worn || g.place == Place::Wardrobe) && !owned_for(g.location, g)) return false;
    if (g.place == Place::Stock && (g.owner.kind != OwnerKind::Workshop || g.owner.id != g.location || !workshops_.contains(g.location))) return false;
    if (g.place == Place::InstitutionStock && (g.owner.kind != OwnerKind::Employer || g.owner.id != g.location || !employers_.contains(g.location))) return false;
    if ((g.place == Place::Transit || g.place == Place::Reserved) && g.owner.kind != OwnerKind::Workshop) return false;
    if (g.place == Place::Worn && !equipped.emplace(g.location, recipe(g.product).slot).second) return false;
  }
  for (const auto& [id, o] : orders_) {
    if (id != o.id || o.escrow < 0 || !garments_.contains(o.garment) || activeOrders_.contains(id) != (!o.complete && !o.cancelled) || (o.complete && o.cancelled) || ((o.complete || o.cancelled) && (o.inTransit || o.escrow != 0))) return false;
    money += o.escrow;
    if (!o.complete && !o.cancelled) {
      if (!ordered.insert(o.garment).second) return false;
      const auto& g = garments_.at(o.garment);
      if (g.owner != Owner{OwnerKind::Workshop, o.seller} || (g.place != Place::Reserved && g.place != Place::Transit && g.place != Place::Retired)) return false;
      if (g.place != Place::Retired && o.inTransit != (g.place == Place::Transit)) return false;
      if (o.inTransit && (o.carrier == 0 || g.location != o.carrier)) return false;
      if (!o.inTransit && g.location != o.seller) return false;
      if (o.escrow != g.price || o.demand.product != g.product || o.demand.style != g.style || o.demand.themeCountry != g.themeCountry) return false;
    }
  }
  for (const auto& [id, g] : garments_) if ((g.place == Place::Reserved || g.place == Place::Transit) && !ordered.contains(id)) return false;
  for (const auto& [id, p] : people_) if (p.cash < p.foodReserve + p.housingReserve || occupied(id) > p.wardrobeLimit) return false;
  return money == moneyIntroduced_ && fibre + totals_.fabricMade * 2 == fibreIntroduced_ && fabric + totals_.fabricConsumed == fabricIntroduced_ + totals_.fabricMade && dye + totals_.dyeConsumed == dyeIntroduced_ && labour + totals_.minutesConsumed == labourIntroduced_ && static_cast<Units>(garments_.size()) == totals_.garmentsMade;
}
} // namespace sonnheide::clothing
