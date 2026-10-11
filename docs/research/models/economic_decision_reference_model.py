"""Finite ADR0016 ledger model; not a runtime economy or game implementation.

Python 3.9+ standard library. run() is called by the unified design audit.
Orders assume retail payment at actual acceptance, two buyers, one seller,
no tax, fee, insurance, route, quality, personality, or general bank solver.
Explicitly model zero-sum cash, finite goods with losses, held claims,
partial delivery, cancellation, receipt replay, work exclusivity and quota.
"""

from dataclasses import dataclass, field
from itertools import permutations, product
import json


@dataclass
class Order:
    buyer: int
    price: int
    total: int
    unpicked: int
    transit: int = 0
    delivered: int = 0
    lost: int = 0
    cancelled: int = 0
    escrow: int = 0
    pick_closed: bool = False


@dataclass
class RetailModel:
    stock: int
    buyers: list
    floors: list
    seller_cash: int = 0
    household_goods: list = field(default_factory=lambda: [0, 0])
    losses: int = 0
    orders: dict = field(default_factory=dict)
    receipts: set = field(default_factory=set)

    def __post_init__(self):
        self.initial_items = self.stock
        self.initial_cash = sum(self.buyers) + self.seller_cash

    def reserve(self, order_id, buyer, quantity, price):
        if order_id in self.orders:
            return False
        if quantity <= 0 or price <= 0:
            return False
        available = self.stock - sum(o.unpicked for o in self.orders.values())
        cash_free = max(0, self.buyers[buyer] - self.floors[buyer])
        if quantity > available or quantity * price > cash_free:
            return False
        # All guards above; no state changes on failed atomic preparation.
        cash = quantity * price
        self.buyers[buyer] -= cash
        self.orders[order_id] = Order(buyer, price, quantity, quantity,
                                      escrow=cash)
        self.check()
        return True

    def pickup(self, order_id, quantity, receipt):
        if receipt in self.receipts:
            return False
        order = self.orders.get(order_id)
        if order is None or order.pick_closed or quantity <= 0:
            return False
        q = min(quantity, order.unpicked)
        if q <= 0:
            return False
        self.stock -= q
        order.unpicked -= q
        order.transit += q
        self.receipts.add(receipt)
        self.check()
        return True

    def deliver(self, order_id, quantity, receipt):
        if receipt in self.receipts:
            return False
        order = self.orders.get(order_id)
        if order is None or quantity <= 0:
            return False
        q = min(quantity, order.transit)
        if q <= 0:
            return False
        order.transit -= q
        order.delivered += q
        self.household_goods[order.buyer] += q
        payment = q * order.price
        order.escrow -= payment
        self.seller_cash += payment
        self.receipts.add(receipt)
        self.check()
        return True

    def lose(self, order_id, quantity, receipt):
        if receipt in self.receipts:
            return False
        order = self.orders.get(order_id)
        if order is None or quantity <= 0:
            return False
        q = min(quantity, order.transit)
        if q <= 0:
            return False
        order.transit -= q
        order.lost += q
        self.losses += q
        # This finite retail variant refunds undelivered escrow. The seller
        # bears inventory loss; general contractual risk is outside scope.
        refund = q * order.price
        order.escrow -= refund
        self.buyers[order.buyer] += refund
        self.receipts.add(receipt)
        self.check()
        return True

    def cancel(self, order_id, receipt):
        if receipt in self.receipts:
            return False
        order = self.orders.get(order_id)
        if order is None:
            return False
        q = order.unpicked
        order.pick_closed = True
        order.unpicked = 0
        order.cancelled += q
        refund = q * order.price
        order.escrow -= refund
        self.buyers[order.buyer] += refund
        # In-transit goods and their escrow remain here. Cancellation cannot
        # teleport picked cargo back, and later actual delivery can complete.
        self.receipts.add(receipt)
        self.check()
        return True

    def snapshot(self):
        return json.dumps({
            "stock": self.stock, "buyers": self.buyers,
            "seller_cash": self.seller_cash,
            "household_goods": self.household_goods, "losses": self.losses,
            "orders": {k: vars(v) for k, v in sorted(self.orders.items())},
            "receipts": sorted(self.receipts),
        }, sort_keys=True)

    def check(self):
        assert self.stock >= 0
        assert all(c >= floor for c, floor in zip(self.buyers, self.floors))
        assert all(q >= 0 for q in self.household_goods)
        assert self.stock >= sum(o.unpicked for o in self.orders.values())
        assert self.initial_cash == (sum(self.buyers) + self.seller_cash +
                                    sum(o.escrow for o in self.orders.values()))
        assert self.initial_items == (self.stock + sum(self.household_goods) +
                                     self.losses +
                                     sum(o.transit for o in self.orders.values()))
        for o in self.orders.values():
            assert min(o.unpicked, o.transit, o.delivered, o.lost,
                       o.cancelled, o.escrow) >= 0
            assert o.total == (o.unpicked + o.transit + o.delivered +
                               o.lost + o.cancelled)
            assert o.escrow == (o.unpicked + o.transit) * o.price


@dataclass
class WorkLedger:
    intervals: list = field(default_factory=list)
    receipts: set = field(default_factory=set)
    wage_numerator: int = 2
    wage_denominator: int = 3
    earned: int = 0
    remainder: int = 0

    def claim(self, actor, start, end, episode):
        if end <= start:
            return False
        if any(a == actor and max(start, s) < min(end, e)
               for a, s, e, unused in self.intervals):
            return False
        self.intervals.append((actor, start, end, episode))
        return True

    def record_work(self, actor, start, end, episode, units, receipt):
        if receipt in self.receipts or units < 0:
            return False
        if not any(a == actor and p == episode and s <= start < end <= e
                   for a, s, e, p in self.intervals):
            return False
        # Actual execution intervals must not overlap, even for a new ID.
        for previous in getattr(self, "actual_intervals", []):
            a, s, e = previous
            if a == actor and max(start, s) < min(end, e):
                return False
        if not hasattr(self, "actual_intervals"):
            self.actual_intervals = []
        self.actual_intervals.append((actor, start, end))
        raw = units * self.wage_numerator + self.remainder
        self.earned += raw // self.wage_denominator
        self.remainder = raw % self.wage_denominator
        self.receipts.add(receipt)
        return True


@dataclass
class DemandQuota:
    effective_demand: int
    existing_capacity: int
    approvals: dict = field(default_factory=dict)

    def approve(self, project_id, capacity):
        if project_id in self.approvals or capacity <= 0:
            return False
        free = self.effective_demand - self.existing_capacity - sum(
            self.approvals.values())
        if capacity > free:
            return False
        self.approvals[project_id] = capacity
        return True

    def expire(self, project_id):
        return self.approvals.pop(project_id, None)


@dataclass
class FiniteLoanPool:
    bank_cash: int
    demand_reserve: int
    safety_reserve: int
    borrower_cash: int = 0
    principal: int = 0
    interest_receivable: int = 0
    interest_remainder: int = 0
    receipts: set = field(default_factory=set)

    def lend(self, quantity, receipt):
        if receipt in self.receipts or quantity <= 0:
            return False
        lendable = max(0, self.bank_cash - self.demand_reserve -
                       self.safety_reserve)
        if quantity > lendable:
            return False
        self.bank_cash -= quantity
        self.borrower_cash += quantity
        self.principal += quantity
        self.receipts.add(receipt)
        return True

    def accrue(self, numerator, denominator, periods):
        assert min(numerator, periods) >= 0 and denominator > 0
        raw = self.principal * numerator * periods + self.interest_remainder
        self.interest_receivable += raw // denominator
        self.interest_remainder = raw % denominator
        # Accrual changes debt only; no cash transfer or mint.


def replay_once(model, operation, order_id, quantity, receipt):
    fn = getattr(model, operation)
    if operation == "cancel":
        fn(order_id, receipt)
    else:
        fn(order_id, quantity, receipt)
    before = model.snapshot()
    if operation == "cancel":
        fn(order_id, receipt)
    else:
        fn(order_id, quantity, receipt)
    assert before == model.snapshot()
    model.check()


def run():
    checks = []

    def verified(name, condition):
        assert condition, name
        checks.append(name)

    # buyers is already net of actual escrow transfers. The second order
    # must not subtract the first escrow again or consume the protected floor.
    net = RetailModel(3, [100, 0], [10, 0])
    verified("cash_claim_first_transfers_30_to_owned_escrow",
             net.reserve("first", 0, 1, 30) and net.buyers[0] == 70)
    verified("net_available_cash_not_subtracted_twice",
             net.reserve("second", 0, 1, 60) and net.buyers[0] == 10)
    before = net.snapshot()
    verified("third_claim_preserves_real_floor_and_escrow",
             not net.reserve("third", 0, 1, 1) and net.snapshot() == before)

    m = RetailModel(1, [4, 4], [1, 1])
    verified("one_last_item_claimed_once", m.reserve("a", 0, 1, 2))
    before = m.snapshot()
    verified("second_buyer_cannot_overdraw_stock", not m.reserve("b", 1, 1, 2))
    verified("failed_prepare_has_zero_writes", m.snapshot() == before)
    replay_once(m, "pickup", "a", 1, "pick-a")
    verified("picked_stock_not_available_at_source", m.stock == 0)
    replay_once(m, "cancel", "a", 0, "cancel-a")
    verified("cancel_does_not_teleport_in_transit", m.stock == 0 and
             m.orders["a"].transit == 1 and m.orders["a"].escrow == 2)
    replay_once(m, "lose", "a", 1, "loss-a")
    verified("loss_and_undelivered_refund_are_once", m.losses == 1 and
             m.buyers == [4, 4] and m.seller_cash == 0)
    m = RetailModel(3, [10, 0], [4, 0])
    verified("basic_floor_can_block_discretionary_order",
             not m.reserve("too-much", 0, 3, 3) and m.buyers[0] == 10)
    verified("actual_funded_quantity_reserves", m.reserve("partial", 0, 2, 3))
    replay_once(m, "pickup", "partial", 2, "pick-p")
    replay_once(m, "deliver", "partial", 1, "deliver-one")
    verified("partial_delivery_pays_only_accepted_goods", m.seller_cash == 3 and
             m.orders["partial"].escrow == 3 and m.household_goods[0] == 1)
    replay_once(m, "cancel", "partial", 0, "cancel-p")
    replay_once(m, "deliver", "partial", 1, "deliver-rest")
    verified("real_delivery_after_cancel_keeps_accepted_payment",
             m.seller_cash == 6 and m.household_goods[0] == 2)

    work = WorkLedger()
    verified("exclusive_primary_interval", work.claim(1, 0, 10, "farm") and
             not work.claim(1, 2, 8, "army") and
             work.claim(1, 10, 12, "care"))
    work.record_work(1, 0, 5, "farm", 2, "work-1")
    earned = work.earned
    verified("work_receipt_replay_cannot_earn_twice",
             not work.record_work(1, 0, 5, "farm", 2, "work-1") and
             work.earned == earned)
    verified("different_receipt_cannot_double_count_overlap",
             not work.record_work(1, 0, 5, "farm", 2, "work-cheat"))
    verified("no_work_without_claim",
             not work.record_work(2, 0, 5, "farm", 2, "work-no-claim"))
    wage_partition_count = 0
    for a, b, c in product(range(5), repeat=3):
        partitioned = WorkLedger()
        partitioned.claim(1, 0, 3, "job")
        for index, units in enumerate((a, b, c)):
            partitioned.record_work(1, index, index + 1, "job", units,
                                    "w-%d" % index)
        whole = WorkLedger()
        whole.claim(1, 0, 3, "job")
        whole.record_work(1, 0, 3, "job", a + b + c, "all")
        assert (partitioned.earned, partitioned.remainder) == (
            whole.earned, whole.remainder)
        wage_partition_count += 1
    verified("all_wage_work_partitions_have_identical_remainders",
             wage_partition_count == 125)

    quota = DemandQuota(10, 4)
    verified("funded_unique_demand_supports_finite_capacity",
             quota.approve("first", 4) and not quota.approve("second", 4) and
             quota.approve("small", 2))
    verified("same_approval_cannot_repeat", not quota.approve("first", 1))
    quota.expire("first")
    verified("expired_quota_releases_only_its_capacity",
             quota.approve("replacement", 4) and
             sum(quota.approvals.values()) == 6)

    loan = FiniteLoanPool(20, 12, 3)
    verified("demand_and_safety_reserves_not_lendable",
             not loan.lend(6, "oversized") and loan.lend(5, "legal") and
             loan.bank_cash == 15)
    verified("loan_disbursement_is_cash_transfer",
             loan.bank_cash + loan.borrower_cash == 20 and loan.principal == 5)
    verified("loan_receipt_cannot_disburse_twice", not loan.lend(5, "legal"))
    before_cash = loan.bank_cash + loan.borrower_cash
    loan.accrue(1, 10, 3)
    verified("interest_accrual_is_not_cash", loan.interest_receivable == 1 and
             loan.interest_remainder == 5 and
             loan.bank_cash + loan.borrower_cash == before_cash)

    # Enumerate small stock/cash/price/request sizes, both legal arbitration
    # orders, and six terminal paths. Every action is replayed with same ID.
    script_patterns = (
        (("pickup", 0, 9), ("pickup", 1, 9), ("deliver", 0, 9), ("deliver", 1, 9)),
        (("cancel", 0, 0), ("pickup", 1, 9), ("deliver", 1, 9)),
        (("pickup", 0, 9), ("pickup", 1, 9), ("lose", 0, 9), ("lose", 1, 9)),
        (("pickup", 0, 9), ("cancel", 0, 0), ("deliver", 0, 1), ("lose", 0, 9)),
        (("pickup", 0, 1), ("cancel", 0, 0), ("deliver", 0, 9), ("cancel", 1, 0)),
        (("pickup", 1, 9), ("deliver", 1, 1), ("cancel", 1, 0), ("lose", 1, 9),
         ("pickup", 0, 9), ("deliver", 0, 9)),
    )
    world_count = 0
    for stock, c0, c1, price, q0, q1 in product(
            range(4), range(6), range(6), (1, 2), (1, 2), (1, 2)):
        for arbitration in permutations((0, 1)):
            for script in script_patterns:
                model = RetailModel(stock, [c0, c1],
                                    [min(c0, 1), min(c1, 1)])
                for buyer in arbitration:
                    before = model.snapshot()
                    ok = model.reserve(str(buyer), buyer,
                                       (q0, q1)[buyer], price)
                    if not ok:
                        assert model.snapshot() == before
                    model.check()
                for index, (op, buyer, quantity) in enumerate(script):
                    replay_once(model, op, str(buyer), quantity,
                                "event-%d" % index)
                model.check()
                world_count += 1
    verified("exhaustive_small_retail_states_preserve_both_ledgers",
             world_count == 13824)

    return {
        "passed": True,
        "scope": "finite_ADR0016_economic_ledger_model_not_runtime_or_production",
        "named_checks": checks,
        "named_check_count": len(checks),
        "exhaustive_retail_world_count": world_count,
        "wage_work_partition_count": wage_partition_count,
        "future_production_scenarios_executed": 0,
        "excluded": ["full_AI_personality_beliefs", "routes_physiology_animation",
                     "market_price_tax_insurance_quality_fixed_fee_solver",
                     "full_bank_agriculture_research_economy", "Steam_GPU"],
    }


if __name__ == "__main__":
    print(json.dumps(run(), indent=2, sort_keys=True))
