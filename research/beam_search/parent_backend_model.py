#!/usr/bin/env python3

import argparse
import bisect
import itertools
import random
from dataclasses import dataclass


@dataclass
class Stats:
    topologies: int = 0
    minimal_cases: int = 0
    adversarial_topologies: int = 0
    random_topologies: int = 0
    exhaustive_topologies: int = 0
    generations: int = 0
    transitions: int = 0
    lcp_skips: int = 0
    forward_moves: int = 0
    reverse_moves: int = 0
    unary_zero_bits_crossed: int = 0
    split_zero_bits_crossed: int = 0
    prefix_generations: int = 0
    prefix_reconstructions: int = 0
    open_walk_generations: int = 0
    end_free_generations: int = 0


class ParentMap:
    def __init__(self, parent, prev_slots, logical_to_slot):
        self.parent = tuple(parent)
        self.prev_width = len(prev_slots)
        self.width = len(parent)
        self.logical_to_slot = tuple(logical_to_slot)

        self.direct = [-1] * self.width
        for child, parent_ordinal in enumerate(self.parent):
            self.direct[self.logical_to_slot[child]] = prev_slots[parent_ordinal]

        self.run_end = []
        self.run_parent = []
        for child, parent_ordinal in enumerate(self.parent):
            if not self.run_parent or self.run_parent[-1] != parent_ordinal:
                self.run_end.append(child + 1)
                self.run_parent.append(parent_ordinal)
            else:
                self.run_end[-1] = child + 1

        self.unary_bits = bytearray(self.prev_width + self.width)
        self.unary_select = []
        for child, parent_ordinal in enumerate(self.parent):
            reversed_parent = self.prev_width - 1 - parent_ordinal
            bit_position = child + reversed_parent
            assert not self.unary_bits[bit_position]
            self.unary_bits[bit_position] = 1
            self.unary_select.append(bit_position)

        self.group_start_bits = bytearray(self.width)
        self.used_parent_bits = bytearray(self.prev_width)
        self.group_starts = []
        self.used_parents = []
        for child, parent_ordinal in enumerate(self.parent):
            if child == 0 or self.parent[child - 1] != parent_ordinal:
                reversed_parent = self.prev_width - 1 - parent_ordinal
                self.group_start_bits[child] = 1
                self.used_parent_bits[reversed_parent] = 1
                self.group_starts.append(child)
                self.used_parents.append(reversed_parent)

        self.run_x = 0
        self.run_cursor = 0
        self.unary_x = 0
        self.unary_position = self.unary_select[0]
        self.split_x = 0
        self.split_group = 0
        self.split_parent_position = self.used_parents[0]

    def direct_parent(self, child):
        child_slot = self.logical_to_slot[child]
        return self.direct[child_slot]

    def random_run_parent(self, child):
        run = bisect.bisect_right(self.run_end, child)
        return self.run_parent[run]

    def random_unary_parent(self, child):
        bit_position = self.unary_select[child]
        reversed_parent = bit_position - child
        return self.prev_width - 1 - reversed_parent

    def random_split_parent(self, child):
        group = bisect.bisect_right(self.group_starts, child) - 1
        return self.prev_width - 1 - self.used_parents[group]

    def _record_direction(self, old_child, child, expected_direction, stats):
        delta = child - old_child
        assert delta * expected_direction >= 0
        if delta > 0:
            stats.forward_moves += 1
        elif delta < 0:
            stats.reverse_moves += 1

    def move_run(self, child, expected_direction, stats):
        self._record_direction(self.run_x, child, expected_direction, stats)
        while child >= self.run_end[self.run_cursor]:
            self.run_cursor += 1
        while self.run_cursor and child < self.run_end[self.run_cursor - 1]:
            self.run_cursor -= 1
        begin = 0 if self.run_cursor == 0 else self.run_end[self.run_cursor - 1]
        assert begin <= child < self.run_end[self.run_cursor]
        self.run_x = child
        return self.run_parent[self.run_cursor]

    def move_unary(self, child, expected_direction, stats):
        self._record_direction(self.unary_x, child, expected_direction, stats)
        direction = (child > self.unary_x) - (child < self.unary_x)
        while self.unary_x != child:
            self.unary_position += direction
            assert 0 <= self.unary_position < len(self.unary_bits)
            if self.unary_bits[self.unary_position]:
                self.unary_x += direction
            else:
                stats.unary_zero_bits_crossed += 1
        assert self.unary_position == self.unary_select[child]
        reversed_parent = self.unary_position - child
        return self.prev_width - 1 - reversed_parent

    def move_split(self, child, expected_direction, stats):
        self._record_direction(self.split_x, child, expected_direction, stats)
        if child > self.split_x:
            crossed = sum(self.group_start_bits[self.split_x + 1 : child + 1])
            for _ in range(crossed):
                self.split_parent_position += 1
                while not self.used_parent_bits[self.split_parent_position]:
                    self.split_parent_position += 1
                    stats.split_zero_bits_crossed += 1
            self.split_group += crossed
        elif child < self.split_x:
            crossed = sum(self.group_start_bits[child + 1 : self.split_x + 1])
            for _ in range(crossed):
                self.split_parent_position -= 1
                while not self.used_parent_bits[self.split_parent_position]:
                    self.split_parent_position -= 1
                    stats.split_zero_bits_crossed += 1
            self.split_group -= crossed
        self.split_x = child
        assert self.group_starts[self.split_group] <= child
        if self.split_group + 1 < len(self.group_starts):
            assert child < self.group_starts[self.split_group + 1]
        assert self.split_parent_position == self.used_parents[self.split_group]
        return self.prev_width - 1 - self.split_parent_position

    def sync_lcp_skip(self, child, expected_direction, stats):
        self._record_direction(self.run_x, child, expected_direction, stats)
        source_parent = self.parent[self.run_x]
        assert self.parent[child] == source_parent

        run_begin = 0 if self.run_cursor == 0 else self.run_end[self.run_cursor - 1]
        assert run_begin <= child < self.run_end[self.run_cursor]
        self.run_x = child

        assert self.random_unary_parent(self.unary_x) == source_parent
        self.unary_position += child - self.unary_x
        self.unary_x = child
        assert self.unary_position == self.unary_select[child]

        old_split_x = self.split_x
        if child > old_split_x:
            assert not any(self.group_start_bits[old_split_x + 1 : child + 1])
        elif child < old_split_x:
            assert not any(self.group_start_bits[child + 1 : old_split_x + 1])
        self.split_x = child
        assert self.random_split_parent(child) == source_parent
        assert bisect.bisect_right(self.group_starts, child) - 1 == self.split_group
        assert self.used_parents[self.split_group] == self.split_parent_position
        stats.lcp_skips += 1


def lcp_depth(lhs, rhs):
    common = 0
    for left_node, right_node in zip(lhs, rhs):
        if left_node != right_node:
            break
        common += 1
    return common - 1


def make_slot_order(width, rng, slot_mode):
    slots = list(range(width))
    if slot_mode == "random":
        rng.shuffle(slots)
    elif slot_mode == "reverse":
        slots.reverse()
    else:
        assert slot_mode == "identity"
    return slots


def validate_lcp_formula(old_paths, new_paths, parent):
    old_depth = len(old_paths[0]) - 1
    old_adjacent = [lcp_depth(old_paths[index], old_paths[index + 1]) for index in range(len(old_paths) - 1)]

    if parent[0] == len(old_paths) - 1:
        entry_lcp = old_depth
    else:
        entry_lcp = min(old_adjacent[parent[0] :])
    assert entry_lcp == lcp_depth(old_paths[-1], new_paths[0])

    for child in range(1, len(parent)):
        if parent[child - 1] == parent[child]:
            adjacent_lcp = old_depth
        else:
            assert parent[child - 1] > parent[child]
            adjacent_lcp = min(old_adjacent[parent[child] : parent[child - 1]])
        assert adjacent_lcp == lcp_depth(new_paths[child - 1], new_paths[child])


def tree_distance(lhs, rhs):
    common_depth = lcp_depth(lhs, rhs)
    return len(lhs) + len(rhs) - 2 * common_depth - 2


def validate_open_walk(source, targets, stats):
    common_depth = min(lcp_depth(source, target) for target in targets)
    union_edges = set()
    for path in [source] + targets:
        for depth in range(1, len(path)):
            union_edges.add(path[:depth + 1])
    induced_edges = len(union_edges) - common_depth

    walk_length = tree_distance(source, targets[0])
    for index in range(1, len(targets)):
        walk_length += tree_distance(targets[index - 1], targets[index])

    last_distance = tree_distance(source, targets[-1])
    assert walk_length == 2 * induced_edges - last_distance
    stats.open_walk_generations += 1

    farthest_distance = max(tree_distance(source, target) for target in targets)
    assert last_distance == farthest_distance
    stats.end_free_generations += 1


def validate_random_lookup(parent_map, child, previous_slots):
    expected_parent = parent_map.parent[child]
    assert parent_map.random_run_parent(child) == expected_parent
    assert parent_map.random_unary_parent(child) == expected_parent
    assert parent_map.random_split_parent(child) == expected_parent
    assert parent_map.direct_parent(child) == previous_slots[expected_parent]


def validate_prefix_formulas(paths, parent_maps, stats):
    freed_compat = 0
    freed_current = 0
    freed_keep = 0

    for depth in range(1, len(parent_maps)):
        current_paths = paths[depth]
        endpoint = current_paths[-1]
        adjacent = [lcp_depth(current_paths[index], current_paths[index + 1])
                    for index in range(len(current_paths) - 1)]
        entry = lcp_depth(paths[depth - 1][-1], current_paths[0])

        compat_depth = min([entry] + adjacent)
        current_depth = depth if len(current_paths) == 1 else min(adjacent)

        next_parent = parent_maps[depth]
        survivor_min = min(next_parent)
        keep_depth = depth if survivor_min == len(current_paths) - 1 else min(adjacent[survivor_min:])

        assert freed_compat <= compat_depth <= current_depth
        assert freed_current <= current_depth
        assert freed_keep <= keep_depth

        for parent_ordinal in set(next_parent):
            assert lcp_depth(endpoint, current_paths[parent_ordinal]) >= keep_depth

        for target in paths[depth + 1]:
            for confirmed_depth in (compat_depth, current_depth, keep_depth):
                rebuilt = endpoint[:confirmed_depth + 1] + target[confirmed_depth + 1:]
                assert rebuilt == target
                stats.prefix_reconstructions += 1

        freed_compat = compat_depth
        freed_current = current_depth
        freed_keep = keep_depth
        stats.prefix_generations += 1


def validate_topology(parent_maps, rng, slot_mode, stats):
    paths = [[(0,)]]
    slots = [[0]]
    maps = [None]
    state_path = (0,)

    for depth, parent in enumerate(parent_maps, 1):
        previous_width = len(paths[depth - 1])
        assert parent
        assert all(0 <= value < previous_width for value in parent)
        assert all(parent[index - 1] >= parent[index] for index in range(1, len(parent)))

        logical_to_slot = make_slot_order(len(parent), rng, slot_mode)
        parent_map = ParentMap(parent, slots[depth - 1], logical_to_slot)
        maps.append(parent_map)
        slots.append(logical_to_slot)
        new_paths = [paths[depth - 1][value] + (child,) for child, value in enumerate(parent)]
        validate_lcp_formula(paths[depth - 1], new_paths, parent)
        validate_open_walk(paths[depth - 1][-1], new_paths, stats)
        paths.append(new_paths)
        stats.generations += 1

        for child, target_path in enumerate(new_paths):
            common_depth = lcp_depth(state_path, target_path)
            assert common_depth >= 0

            for map_depth in range(depth, common_depth + 1, -1):
                expected_direction = 1 if (depth - map_depth) % 2 == 0 else -1
                target_ordinal = target_path[map_depth]
                expected_parent = target_path[map_depth - 1]
                current_map = maps[map_depth]
                assert current_map.move_run(target_ordinal, expected_direction, stats) == expected_parent
                assert current_map.move_unary(target_ordinal, expected_direction, stats) == expected_parent
                assert current_map.move_split(target_ordinal, expected_direction, stats) == expected_parent
                assert current_map.direct_parent(target_ordinal) == slots[map_depth - 1][expected_parent]

            skipped_depth = common_depth + 1
            expected_direction = 1 if (depth - skipped_depth) % 2 == 0 else -1
            if child == 0 and skipped_depth == depth and len(state_path) == depth:
                assert parent_map.run_x == target_path[depth] == 0
                assert parent_map.unary_x == target_path[depth] == 0
                assert parent_map.split_x == target_path[depth] == 0
            else:
                maps[skipped_depth].sync_lcp_skip(target_path[skipped_depth], expected_direction, stats)

            state_path = target_path
            stats.transitions += 1
            for map_depth in range(1, depth + 1):
                current_map = maps[map_depth]
                assert current_map.run_x == state_path[map_depth]
                assert current_map.unary_x == state_path[map_depth]
                assert current_map.split_x == state_path[map_depth]
                assert current_map.unary_position == current_map.unary_select[state_path[map_depth]]

        final_child = rng.randrange(len(parent))
        final_ordinal = final_child
        for map_depth in range(depth, 0, -1):
            validate_random_lookup(maps[map_depth], final_ordinal, slots[map_depth - 1])
            final_ordinal = maps[map_depth].parent[final_ordinal]
        assert final_ordinal == 0

        final_slot = slots[depth][final_child]
        for map_depth in range(depth, 0, -1):
            expected_ordinal = paths[depth][final_child][map_depth]
            assert final_slot == slots[map_depth][expected_ordinal]
            final_slot = maps[map_depth].direct[final_slot]
        assert final_slot == 0

    validate_prefix_formulas(paths, parent_maps, stats)
    stats.topologies += 1


def random_parent_maps(rng, max_depth, max_width):
    depth = rng.randint(2, max_depth)
    previous_width = 1
    result = []
    for _ in range(depth):
        width = rng.randint(1, max_width)
        parent = sorted((rng.randrange(previous_width) for _ in range(width)), reverse=True)
        result.append(tuple(parent))
        previous_width = width
    return result


def nonincreasing_maps(previous_width, max_width):
    for width in range(1, max_width + 1):
        for values in itertools.combinations_with_replacement(range(previous_width), width):
            yield tuple(reversed(values))


def exhaustive_topologies(depth, max_width):
    prefix = []

    def visit(previous_width, remaining):
        if remaining == 0:
            yield tuple(prefix)
            return
        for parent in nonincreasing_maps(previous_width, max_width):
            prefix.append(parent)
            yield from visit(len(parent), remaining - 1)
            prefix.pop()

    yield from visit(1, depth)


def adversarial_parent_maps():
    yield (
        (0,) * 130,
        (64,),
    )
    yield (
        (0,) * 257,
        (256, 256, 128, 128, 0),
        (4, 0),
    )
    yield (
        (0,) * 260,
        (259,) * 63 + (200,) + (64,) * 64 + (0,) * 132,
    )
    yield (
        (0,) * 1024,
        (500,),
    )
    yield (
        (0,),
        (0,) * 1024,
    )
    yield (
        (0,) * 128,
        tuple(range(127, -1, -1)),
    )
    yield (
        (0,) * 8,
        (7, 0),
        (1, 1, 1, 0, 0, 0, 0, 0),
        (7, 0),
    )


def validate_minimal_slot_counterexample(stats):
    parent_map = ParentMap((0, 0), (0,), (1, 0))
    assert parent_map.logical_to_slot == (1, 0)
    assert parent_map.direct_parent(0) == 0
    assert parent_map.direct_parent(1) == 0
    assert parent_map.logical_to_slot[0] != 0
    stats.topologies += 1
    stats.minimal_cases += 1


def parse_args():
    parser = argparse.ArgumentParser()
    parser.add_argument("--seed", type=int, default=20260901)
    parser.add_argument("--trials", type=int, default=30000)
    parser.add_argument("--max-depth", type=int, default=15)
    parser.add_argument("--max-width", type=int, default=8)
    parser.add_argument("--exhaustive-depth", type=int, default=3)
    parser.add_argument("--exhaustive-width", type=int, default=3)
    return parser.parse_args()


def main():
    if not __debug__:
        raise RuntimeError("assertions must be enabled")
    args = parse_args()
    assert args.trials >= 0
    assert args.max_depth >= 2
    assert args.max_width >= 1
    assert args.exhaustive_depth >= 0
    assert args.exhaustive_width >= 1

    rng = random.Random(args.seed)
    stats = Stats()
    validate_minimal_slot_counterexample(stats)

    for topology in adversarial_parent_maps():
        validate_topology(topology, rng, "reverse", stats)
        stats.adversarial_topologies += 1

    for _ in range(args.trials):
        validate_topology(random_parent_maps(rng, args.max_depth, args.max_width), rng, "random", stats)
        stats.random_topologies += 1

    if args.exhaustive_depth:
        for index, topology in enumerate(exhaustive_topologies(args.exhaustive_depth, args.exhaustive_width)):
            slot_mode = "identity" if index % 2 == 0 else "reverse"
            validate_topology(topology, rng, slot_mode, stats)
            stats.exhaustive_topologies += 1

    print(f"seed={args.seed}")
    print(f"random_trials={args.trials}")
    print(f"random_depth=2..{args.max_depth}")
    print(f"random_width=1..{args.max_width}")
    print(f"exhaustive_depth={args.exhaustive_depth}")
    print(f"exhaustive_width=1..{args.exhaustive_width}")
    for field, value in vars(stats).items():
        print(f"{field}={value}")


if __name__ == "__main__":
    main()
