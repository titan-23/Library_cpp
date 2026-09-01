#!/usr/bin/env python3

import argparse
from dataclasses import dataclass


@dataclass(frozen=True)
class Profile:
    name: str
    width: int
    parents: int
    entry_distance: int
    cross_suffix: int
    live_depth: int
    trace_capacity: int
    generation_slots: int | None = None
    boundary_span: int | None = None
    next_boundary_span: int | None = None


@dataclass(frozen=True)
class Cost:
    profile: Profile
    tour_tokens: int
    first_prefix: int
    trace_fill: int
    parent_loads: int
    rollback: int
    apply: int
    state_ops: int
    live_edges: int
    generation_slots: int
    baseline_read: int
    slot_read: int
    parent_oracle_read: int
    baseline_write: int
    slot_write: int
    parent_oracle_write: int
    parent_derived_write: int
    baseline_memory: int
    slot_memory: int
    parent_oracle_memory: int
    parent_derived_memory: int


def evaluate(profile: Profile) -> Cost:
    w = profile.width
    p = profile.parents
    e = profile.entry_distance
    r = profile.cross_suffix
    d = profile.live_depth
    k = profile.trace_capacity
    boundary_span = profile.boundary_span if profile.boundary_span is not None else p - 1
    next_boundary_span = profile.next_boundary_span if profile.next_boundary_span is not None else p - 1
    if not 1 <= p <= w:
        raise ValueError("parents must be in [1, width]")
    if e < 0 or d < 1 or k < d:
        raise ValueError("entry distance and depth values are inconsistent")
    if p > 1 and r < 2:
        raise ValueError("cross-parent suffix must be at least 2")
    if not p - 1 <= boundary_span <= w - 1:
        raise ValueError("boundary span must be in [parents-1, width-1]")
    if not p - 1 <= next_boundary_span <= w - 1:
        raise ValueError("next boundary span must be in [parents-1, width-1]")

    same_parent_transitions = w - p
    cross_parent_transitions = p - 1
    m = same_parent_transitions + cross_parent_transitions * r
    b = e + 1
    f = b + m
    parent_loads = e + m - (w - 1)
    rollback = e + m
    apply = e + 1 + m
    state_ops = rollback + apply
    live_edges = m + d
    g = profile.generation_slots if profile.generation_slots is not None else live_edges
    if g < live_edges:
        raise ValueError("generation slots must be at least the live edge count")

    baseline_write = 8 * (b + m) + 4 * w
    slot_write = 4 * m + 4 * w
    parent_oracle_write = 12 * w
    parent_derived_write = 8 * w
    baseline_read = 8 * state_ops + 8 * parent_loads + 8 * f + 4 * (w + 3 * boundary_span)
    slot_read = 4 * state_ops + 4 * parent_loads + 4 * m + 4 * (w + 3 * boundary_span)
    parent_oracle_read = 4 * state_ops + 4 * parent_loads + 4 * w + 4 * next_boundary_span

    baseline_memory = 16 * (b + m) + 8 * w + 8 * k
    slot_memory = 8 * m + 8 * w + 4 * k
    parent_oracle_memory = 4 * g + 16 * w + 4 * k
    parent_derived_memory = 4 * g + 8 * w + 4 * k

    assert f == w + parent_loads
    assert state_ops == 2 * e + 1 + 2 * m
    if p == 1:
        assert m == w - 1
        assert parent_loads == e
    if p == w and w > 1:
        assert m >= 2 * (w - 1)

    return Cost(
        profile=profile,
        tour_tokens=m,
        first_prefix=b,
        trace_fill=f,
        parent_loads=parent_loads,
        rollback=rollback,
        apply=apply,
        state_ops=state_ops,
        live_edges=live_edges,
        generation_slots=g,
        baseline_read=baseline_read,
        slot_read=slot_read,
        parent_oracle_read=parent_oracle_read,
        baseline_write=baseline_write,
        slot_write=slot_write,
        parent_oracle_write=parent_oracle_write,
        parent_derived_write=parent_derived_write,
        baseline_memory=baseline_memory,
        slot_memory=slot_memory,
        parent_oracle_memory=parent_oracle_memory,
        parent_derived_memory=parent_derived_memory,
    )


def default_profiles() -> list[Profile]:
    deep_live_edges = 252 + 32
    return [
        Profile("W=1", 1, 1, 0, 2, 32, 32),
        Profile("P=1", 64, 1, 0, 2, 32, 32),
        Profile("P=W,r=2", 64, 64, 0, 2, 32, 32),
        Profile("P=W,r=4", 64, 64, 0, 4, 32, 32),
        Profile("G=10E", 64, 64, 0, 4, 32, 32, 10 * deep_live_edges),
    ]


def render(costs: list[Cost]) -> None:
    print("| case | M | L | X | E | G |")
    print("|---|---:|---:|---:|---:|---:|")
    for cost in costs:
        print(
            f"| {cost.profile.name} | {cost.tour_tokens} | {cost.parent_loads} | {cost.state_ops} "
            f"| {cost.live_edges} | {cost.generation_slots} |"
        )
    print()
    print("| case | base read | slot read | parent read | base write | slot write | parent write | derived write |")
    print("|---|---:|---:|---:|---:|---:|---:|---:|")
    for cost in costs:
        print(
            f"| {cost.profile.name} | {cost.baseline_read} | {cost.slot_read} | {cost.parent_oracle_read} "
            f"| {cost.baseline_write} | {cost.slot_write} | {cost.parent_oracle_write} "
            f"| {cost.parent_derived_write} |"
        )
    print()
    print("| case | base memory | slot memory | parent memory | derived memory |")
    print("|---|---:|---:|---:|---:|")
    for cost in costs:
        print(
            f"| {cost.profile.name} | {cost.baseline_memory} | {cost.slot_memory} "
            f"| {cost.parent_oracle_memory} | {cost.parent_derived_memory} |"
        )


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument("--width", type=int)
    parser.add_argument("--parents", type=int)
    parser.add_argument("--entry-distance", type=int, default=0)
    parser.add_argument("--cross-suffix", type=int, default=2)
    parser.add_argument("--unfixed-depth", type=int, default=32)
    parser.add_argument("--trace-capacity", type=int)
    parser.add_argument("--generation-slots", type=int)
    parser.add_argument("--boundary-span", type=int)
    parser.add_argument("--next-boundary-span", type=int)
    return parser.parse_args()


def main() -> None:
    args = parse_args()
    if args.width is None:
        profiles = default_profiles()
    else:
        parents = args.parents if args.parents is not None else args.width
        trace_capacity = args.trace_capacity if args.trace_capacity is not None else args.unfixed_depth
        profiles = [
            Profile(
                "custom",
                args.width,
                parents,
                args.entry_distance,
                args.cross_suffix,
                args.unfixed_depth,
                trace_capacity,
                args.generation_slots,
                args.boundary_span,
                args.next_boundary_span,
            )
        ]
    render([evaluate(profile) for profile in profiles])


if __name__ == "__main__":
    main()
