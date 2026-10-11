"""Finite ADR0016 political invariants, not a production simulator.

Python 3.9+ standard library. The root design audit calls run() once after all
domain edits and peer reviews. This model intentionally omits propagation,
psychology, actual movement, secret-plot discovery, and every combat mechanic.
"""

from dataclasses import dataclass, field
from itertools import product
from typing import Dict, FrozenSet, Tuple


@dataclass(frozen=True)
class Ballot:
    seat: int
    choice: str
    proposal: str = "proposal-A"
    procedure: int = 1
    # Authoritative chamber receipt time; not remote writing or send time.
    cast_tick: int = 5
    signed_lawfully_at_cast: bool = True
    casting_actor_alive_now: bool = True


@dataclass(frozen=True)
class Session:
    seats: FrozenSet[int]
    proposal: str = "proposal-A"
    procedure: int = 1
    close_tick: int = 10


def close_session(session: Session, ballots: Tuple[Ballot, ...],
                  current_proposal: str = "proposal-A",
                  current_procedure: int = 1) -> str:
    """Ballots arrive in saved authoritative command order.

    Authority-at-cast is immutable evidence. A dead casting agent does not
    invalidate a legally issued surviving-seat ballot; withdrawn seats do.
    A later invalid attempt does not erase an earlier legally valid vote.
    """
    if (current_proposal != session.proposal or
            current_procedure != session.procedure):
        return "STALE_PROCEDURE_OR_PROPOSAL"
    latest = {}  # type: Dict[int, Ballot]
    for ballot in ballots:
        if (ballot.seat in session.seats and
                ballot.proposal == session.proposal and
                ballot.procedure == session.procedure and
                ballot.cast_tick < session.close_tick and
                ballot.signed_lawfully_at_cast and
                ballot.choice in ("YES", "NO", "ABSTAIN")):
            latest[ballot.seat] = ballot
    seat_count = len(session.seats)
    quorum = (2 * seat_count + 2) // 3
    if seat_count == 0 or len(latest) < quorum:
        return "NO_QUORUM"
    yes = sum(b.choice == "YES" for b in latest.values())
    no = sum(b.choice == "NO" for b in latest.values())
    return "PASSED" if yes > no else "REJECTED"


@dataclass(frozen=True)
class Authorization:
    """Current prepared requester authority, not an expiry rule for old law."""

    proposal: str
    role_revision: int
    max_spend: int

    def permits(self, proposal: str, role_revision: int, amount: int) -> bool:
        return (proposal == self.proposal and
                role_revision == self.role_revision and
                0 <= amount <= self.max_spend)


@dataclass
class Budget:
    cash: int
    forecast_income: int = 0
    reserved: Dict[str, int] = field(default_factory=dict)
    spent: Dict[str, int] = field(default_factory=dict)

    def available(self) -> int:
        return self.cash - sum(self.reserved.values())

    def reserve(self, command: str, amount: int) -> bool:
        if amount < 0:
            return False
        if command in self.spent:
            return self.spent[command] == amount
        if command in self.reserved:
            return self.reserved[command] == amount
        if amount > self.available():
            return False
        self.reserved[command] = amount
        return True

    def spend(self, command: str, amount: int) -> bool:
        if command in self.spent:
            return self.spent[command] == amount
        if self.reserved.get(command) != amount or amount > self.cash:
            return False
        self.cash -= amount
        del self.reserved[command]
        self.spent[command] = amount
        return True

    def release(self, command: str) -> None:
        self.reserved.pop(command, None)

    def checkpoint(self) -> Dict[str, object]:
        return {"cash": self.cash, "forecast_income": self.forecast_income,
                "reserved": dict(self.reserved), "spent": dict(self.spent)}

    @classmethod
    def from_checkpoint(cls, data: Dict[str, object]) -> "Budget":
        return cls(int(data["cash"]), int(data["forecast_income"]),
                   dict(data["reserved"]), dict(data["spent"]))


@dataclass(frozen=True)
class OwnBelief:
    required_benefit_lo: int
    required_benefit_hi: int
    risk_q: int
    lawful_authority: bool = True


def accept_offer(own: OwnBelief, offered_benefit: int) -> bool:
    """A tiny own-belief subcase, not a full diplomatic utility function.

    Deliberately has no argument for opponent wallet, reservation price,
    hidden army count, or true strategy. The full design has several distinct
    issue constraints and actual messages rather than this single scalar.
    """
    assert 0 <= own.risk_q <= 10000
    assert own.required_benefit_lo <= own.required_benefit_hi
    minimum = own.required_benefit_hi - (
        (own.required_benefit_hi - own.required_benefit_lo) *
        own.risk_q // 10000)
    return own.lawful_authority and offered_benefit >= minimum


def run() -> Dict[str, object]:
    checks = []

    def check(name: str, result: bool) -> None:
        assert result, name
        checks.append(name)

    s3 = Session(frozenset((1, 2, 3)))
    yes1 = Ballot(1, "YES")
    no2 = Ballot(2, "NO")
    yes3 = Ballot(3, "YES")
    check("duplicate_same_seat_not_extra_vote",
          close_session(s3, (yes1, yes1, no2)) == "REJECTED")
    check("lawful_replacement_before_close",
          close_session(s3, (yes1, no2, Ballot(2, "YES"))) == "PASSED")
    check("distinct_lawful_seats_not_actor_duplicates",
          close_session(s3, (yes1, yes3)) == "PASSED")
    check("old_terms_not_new_proposal_approval",
          close_session(s3, (yes1, yes3), "proposal-B") ==
          "STALE_PROCEDURE_OR_PROPOSAL")
    check("procedure_frozen_before_result",
          close_session(s3, (yes1, yes3), current_procedure=2) ==
          "STALE_PROCEDURE_OR_PROPOSAL")
    check("late_cast_excluded",
          close_session(s3, (yes1, Ballot(3, "YES", cast_tick=10))) ==
          "NO_QUORUM")
    check("unsigned_vote_not_consent",
          close_session(s3, (yes1, Ballot(3, "YES",
                                       signed_lawfully_at_cast=False))) ==
          "NO_QUORUM")
    check("dead_signer_issued_vote_survives_real_seat",
          close_session(s3, (yes1, Ballot(3, "YES",
                                       casting_actor_alive_now=False))) ==
          "PASSED")
    check("withdrawn_seat_final_set_not_counted",
          close_session(Session(frozenset((1, 2))),
                        (yes1, no2, yes3)) == "REJECTED")
    check("policy_tie_has_no_crown_tiebreak",
          close_session(s3, (yes1, no2)) == "REJECTED")
    check("all_abstain_does_not_pass",
          close_session(s3, (Ballot(1, "ABSTAIN"),
                             Ballot(2, "ABSTAIN"))) == "REJECTED")
    check("no_phantom_empty_institution_approval",
          close_session(Session(frozenset()), ()) == "NO_QUORUM")
    check("stale_authority_not_implementation_permission",
          not Authorization("proposal-A", 1, 10).permits("proposal-A", 2, 1))
    forecast = Budget(0, 100000)
    check("forecast_income_not_cash", not forecast.reserve("p1", 1))
    b = Budget(10)
    check("approval_not_money",
          Authorization("proposal-A", 1, 100).permits("proposal-A", 1, 100)
          and not b.reserve("unfunded", 100) and b.checkpoint() ==
          Budget(10).checkpoint())
    check("competing_reservations_do_not_overcommit",
          b.reserve("a", 7) and not b.reserve("b", 7) and b.available() == 3)
    check("idempotent_reserve_and_spend",
          b.reserve("a", 7) and b.spend("a", 7) and b.spend("a", 7)
          and b.cash == 3 and not b.reserved and b.spent == {"a": 7})
    check("same_command_different_payload_rejected",
          not b.reserve("a", 2) and not b.spend("a", 2) and b.cash == 3)
    b.release("a")
    check("release_or_repeal_does_not_refund_spent_cash", b.cash == 3)
    restored = Budget.from_checkpoint(b.checkpoint())
    check("load_idempotence_receipt_retained",
          restored.spend("a", 7) and restored.cash == 3)
    check("risk_changes_own_estimate_not_authority",
          not accept_offer(OwnBelief(3, 9, 0), 5) and
          accept_offer(OwnBelief(3, 9, 10000), 5) and
          not accept_offer(OwnBelief(3, 9, 10000, False), 100))

    ballot_cases = 0
    for n in range(1, 5):
        for choices in product(("ABSENT", "YES", "NO", "ABSTAIN"), repeat=n):
            original = tuple(Ballot(i, choices[i]) for i in range(n)
                             if choices[i] != "ABSENT")
            for final_mask in product((False, True), repeat=n):
                seats = frozenset(i for i, member in enumerate(final_mask)
                                  if member)
                presence = sum(choices[i] != "ABSENT" for i in seats)
                quorum = (2 * len(seats) + 2) // 3
                yes = sum(choices[i] == "YES" for i in seats)
                no = sum(choices[i] == "NO" for i in seats)
                expected = ("NO_QUORUM" if not seats or presence < quorum
                            else "PASSED" if yes > no else "REJECTED")
                session = Session(seats)
                assert close_session(session, original) == expected
                assert close_session(session, original + original) == expected
                dead_issuers = tuple(Ballot(b.seat, b.choice,
                                           casting_actor_alive_now=False)
                                     for b in original)
                assert close_session(session, dead_issuers) == expected
                ballot_cases += 1

    budget_cases = 0
    for cash, a, z in product(range(9), repeat=3):
        ledger = Budget(cash, 1000)
        accepted_a = ledger.reserve("a", a)
        accepted_z = ledger.reserve("z", z)
        assert ledger.available() >= 0
        assert accepted_a == (a <= cash)
        assert accepted_z == (z <= cash - (a if accepted_a else 0))
        expected_spent = 0
        for key, amount, admitted in (("a", a, accepted_a),
                                     ("z", z, accepted_z)):
            if admitted:
                assert ledger.spend(key, amount)
                assert ledger.spend(key, amount)
                expected_spent += amount
        assert ledger.cash == cash - expected_spent >= 0
        assert not ledger.reserved
        budget_cases += 1

    information_cases = 0
    for required_lo, width, risk, offered in product(
            range(5), range(4), (0, 5000, 10000), range(9)):
        belief = OwnBelief(required_lo, required_lo + width, risk)
        expected = accept_offer(belief, offered)
        # Hidden opponent worlds differ. None is passed into the planner.
        for hidden_wallet, hidden_army, hidden_price in (
                (0, 0, 0), (10000, 500, 100), (999999, 1, 10000)):
            world_secret = (hidden_wallet, hidden_army, hidden_price)
            assert world_secret is not None
            assert accept_offer(belief, offered) == expected
        information_cases += 1

    return {"passed": True,
            "scope": "finite_abstract_political_ballots_budget_and_own_belief_only",
            "production_implemented": False,
            "named_check_count": len(checks),
            "named_checks": checks,
            "ballot_final_membership_cases": ballot_cases,
            "integer_budget_cases": budget_cases,
            "own_belief_information_cases": information_cases,
            "not_proven": ["full_politics_or_personality",
                           "secret_plot_discovery_or_message_delivery",
                           "tactical_war", "performance_or_game_balance"]}


if __name__ == "__main__":
    import json
    print(json.dumps(run(), indent=2, sort_keys=True))
