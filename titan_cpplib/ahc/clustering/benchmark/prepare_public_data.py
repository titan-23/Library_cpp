#!/usr/bin/env python3
import argparse
import csv
import hashlib
import math
import shlex
import shutil
import tempfile
import urllib.request
import zipfile
from pathlib import Path

GROUNDTRUTH_COMMIT = "e60cb1c2dedc717a54a67a2c1caac543c49a34b9"
GROUNDTRUTH_URL = f"https://codeload.github.com/shudianzhao/GroundTruth_VS_OptimalCluster/zip/{GROUNDTRUTH_COMMIT}"


def quote_text(value):
    if "\n" in value or "\r" in value:
        raise ValueError("name and source must be one line")
    return '"' + value.replace("\\", "\\\\").replace('"', '\\"') + '"'


def read_header_points(path):
    tokens = Path(path).read_text(encoding="utf-8").split()
    if len(tokens) < 2:
        raise ValueError(f"missing point count and dimension: {path}")
    point_count = int(tokens[0])
    dimension = int(tokens[1])
    expected = 2 + point_count * dimension
    if point_count <= 0 or dimension <= 0 or len(tokens) != expected:
        raise ValueError(f"expected exactly {point_count} x {dimension} coordinates: {path}")
    values = [float(token) for token in tokens[2:]]
    if not all(math.isfinite(value) for value in values):
        raise ValueError(f"coordinates must be finite: {path}")
    return [values[i:i + dimension] for i in range(0, len(values), dimension)]


def read_table_points(path, dimension, skip_lines=0, skip_columns=0):
    if dimension <= 0 or skip_lines < 0 or skip_columns < 0:
        raise ValueError("dimension must be positive and skipped counts must be nonnegative")
    points = []
    lines = Path(path).read_text(encoding="utf-8").splitlines()[skip_lines:]
    for line_number, line in enumerate(lines, skip_lines + 1):
        if not line.strip():
            continue
        fields = line.split()
        if len(fields) != skip_columns + dimension:
            raise ValueError(f"expected {skip_columns + dimension} columns: {path}:{line_number}")
        point = [float(value) for value in fields[skip_columns:]]
        if not all(math.isfinite(value) for value in point):
            raise ValueError(f"coordinates must be finite: {path}:{line_number}")
        points.append(point)
    if not points:
        raise ValueError(f"no points: {path}")
    return points


def read_integer_labels(path, point_count, base):
    labels = [int(token) - base for token in Path(path).read_text(encoding="utf-8").split()]
    if len(labels) != point_count:
        raise ValueError(f"expected {point_count} labels: {path}")
    return labels


def write_case(path, name, source, points, cluster_count, ranges, best_known=None, labels=None):
    if not name:
        raise ValueError("name must not be empty")
    if not points or not points[0] or any(len(point) != len(points[0]) for point in points):
        raise ValueError("points must be a nonempty rectangular table")
    if len(points) > 2_147_483_647 or len(points[0]) > 2_147_483_647:
        raise ValueError("point count and dimension must fit in int")
    if any(not math.isfinite(value) for point in points for value in point):
        raise ValueError("coordinates must be finite")
    if cluster_count <= 0 or cluster_count > len(points):
        raise ValueError("cluster count must be in [1, point count]")
    if len(ranges) != cluster_count:
        raise ValueError("range count must equal cluster count")
    if any(lower <= 0 or lower > upper or upper > len(points) for lower, upper in ranges):
        raise ValueError("each range must satisfy 1 <= lower <= upper <= point count")
    if sum(lower for lower, _ in ranges) > len(points) or sum(upper for _, upper in ranges) < len(points):
        raise ValueError("size ranges are infeasible")
    if best_known is not None and (not math.isfinite(best_known) or best_known < 0):
        raise ValueError("best known cost must be finite and nonnegative")
    labels = [] if labels is None else labels
    if labels and len(labels) != len(points):
        raise ValueError("label count must equal point count")
    if labels and min(labels) < 0:
        raise ValueError("labels must be nonnegative")
    output = Path(path)
    output.parent.mkdir(parents=True, exist_ok=True)
    with output.open("w", encoding="utf-8", newline="\n") as file:
        file.write("titan_clustering_benchmark_v1\n")
        file.write(f"name {quote_text(name)}\n")
        file.write(f"source {quote_text(source)}\n")
        file.write(f"points {len(points)}\n")
        file.write(f"dimension {len(points[0])}\n")
        file.write(f"clusters {cluster_count}\n")
        file.write("best_known_cost none\n" if best_known is None else f"best_known_cost {best_known:.17g}\n")
        file.write("ranges" + "".join(f" {lower} {upper}" for lower, upper in ranges) + "\n")
        if labels:
            file.write("reference_labels present " + " ".join(map(str, labels)) + "\n")
        else:
            file.write("reference_labels none\n")
        file.write("data\n")
        for point in points:
            file.write(" ".join(format(value, ".17g") for value in point) + "\n")


def parse_ranges(args, point_count):
    if args.sizes is not None:
        if not args.sizes:
            raise ValueError("sizes must not be empty")
        sizes = [int(value) for value in args.sizes.split(",")]
        return [(size, size) for size in sizes]
    if args.ranges is not None:
        if not args.ranges:
            raise ValueError("ranges must not be empty")
        result = []
        for item in args.ranges.split(","):
            lower, upper = item.split(":", 1)
            result.append((int(lower), int(upper)))
        return result
    return [(1, point_count)] * args.clusters


def convert_plain(args):
    points = read_header_points(args.input)
    labels = None if args.labels is None else read_integer_labels(args.labels, len(points), args.label_base)
    ranges = parse_ranges(args, len(points))
    write_case(args.output, args.name, args.source, points, args.clusters, ranges, args.best_known, labels)


def convert_table(args):
    points = read_table_points(args.input, args.dimension, args.skip_lines, args.skip_columns)
    labels = None if args.labels is None else read_integer_labels(args.labels, len(points), args.label_base)
    ranges = parse_ranges(args, len(points))
    write_case(args.output, args.name, args.source, points, args.clusters, ranges, args.best_known, labels)


def add_conversion_arguments(parser):
    parser.add_argument("input")
    parser.add_argument("output")
    parser.add_argument("--name", required=True)
    parser.add_argument("--source", default="user supplied")
    parser.add_argument("--clusters", required=True, type=int)
    size_group = parser.add_mutually_exclusive_group()
    size_group.add_argument("--sizes")
    size_group.add_argument("--ranges")
    parser.add_argument("--best-known", type=float)
    parser.add_argument("--labels")
    parser.add_argument("--label-base", type=int, default=0)


def read_groundtruth_summary(repository):
    summary_path = repository / "input_data_for_exact_solver" / "Table_summary.txt"
    result = {}
    with summary_path.open(encoding="utf-8") as file:
        next(file)
        for line in file:
            fields = line.split()
            if len(fields) != 5 or fields[1] not in {"real", "artificial"}:
                continue
            name, data_type, point_count, dimension_with_label, cluster_count = fields
            result[name.removesuffix(".arff")] = {
                "type": data_type,
                "point_count": int(point_count),
                "dimension": int(dimension_with_label) - 1,
                "cluster_count": int(cluster_count),
            }
    return result


def read_groundtruth_labels(path, points):
    lines = Path(path).read_text(encoding="utf-8").splitlines()
    if len(lines) != len(points) + 1:
        raise ValueError(f"unexpected labelled file length: {path}")
    header = shlex.split(lines[0])
    class_columns = [index for index, name in enumerate(header) if name.lower() == "class"]
    if len(class_columns) != 1:
        raise ValueError(f"expected exactly one class column: {path}:1")
    class_column = class_columns[0]
    labels = []
    label_ids = {}
    for index, line in enumerate(lines[1:]):
        fields = shlex.split(line)
        if len(fields) != len(header) or len(fields) != len(points[index]) + 1:
            raise ValueError(f"unexpected labelled row: {path}:{index + 2}")
        labelled_point = [float(value) for column, value in enumerate(fields) if column != class_column]
        if not points_match(labelled_point, points[index]):
            raise ValueError(f"point order differs between files: {path}:{index + 2}")
        label = fields[class_column]
        if label not in label_ids:
            label_ids[label] = len(label_ids)
        labels.append(label_ids[label])
    return labels


def points_match(left, right):
    return len(left) == len(right) and all(
        math.isclose(a, b, rel_tol=1e-12, abs_tol=1e-12)
        for a, b in zip(left, right)
    )


def clustering_cost(points, labels):
    if len(points) != len(labels):
        raise ValueError("point and label counts differ")
    cluster_count = max(labels) + 1
    dimension = len(points[0])
    members = [[] for _ in range(cluster_count)]
    for index, label in enumerate(labels):
        if label < 0:
            raise ValueError("labels must be nonnegative")
        members[label].append(index)
    if any(not cluster for cluster in members):
        raise ValueError("labels must use every cluster number")
    centers = []
    for cluster in members:
        centers.append([
            math.fsum(points[index][axis] for index in cluster) / len(cluster)
            for axis in range(dimension)
        ])
    return math.fsum(
        (points[index][axis] - centers[label][axis]) ** 2
        for index, label in enumerate(labels)
        for axis in range(dimension)
    )


def read_exact_clusterings(path, points):
    lines = Path(path).read_text(encoding="utf-8").splitlines()
    if len(lines) != len(points) + 1:
        raise ValueError(f"unexpected exact-clustering file length: {path}")
    header = shlex.split(lines[0])
    exact_columns = []
    for index, name in enumerate(header):
        lower_name = name.lower()
        if not lower_name.startswith("class_k_"):
            continue
        cluster_count = int(lower_name.removeprefix("class_k_"))
        if cluster_count <= 0 or any(count == cluster_count for count, _ in exact_columns):
            raise ValueError(f"invalid exact-clustering column: {path}:1")
        exact_columns.append((cluster_count, index))
    class_columns = [index for index, name in enumerate(header) if name.lower() == "class"]
    if not exact_columns or len(class_columns) != 1:
        raise ValueError(f"missing class or exact-clustering columns: {path}:1")
    coordinate_columns = [
        index for index, name in enumerate(header)
        if name.lower() != "class" and not name.lower().startswith("class_k_")
    ]
    if len(coordinate_columns) != len(points[0]):
        raise ValueError(f"coordinate count differs from the point file: {path}:1")
    labels_by_count = {cluster_count: [] for cluster_count, _ in exact_columns}
    for index, line in enumerate(lines[1:]):
        fields = shlex.split(line)
        if len(fields) != len(header):
            raise ValueError(f"unexpected exact-clustering row: {path}:{index + 2}")
        exact_point = [float(fields[column]) for column in coordinate_columns]
        if not points_match(exact_point, points[index]):
            raise ValueError(f"point order differs between files: {path}:{index + 2}")
        for cluster_count, exact_column in exact_columns:
            raw_label = float(fields[exact_column])
            if not raw_label.is_integer():
                raise ValueError(f"exact cluster number is not an integer: {path}:{index + 2}")
            labels_by_count[cluster_count].append(int(raw_label) - 1)
    for cluster_count, labels in labels_by_count.items():
        if min(labels) < 0 or max(labels) >= cluster_count or len(set(labels)) != cluster_count:
            raise ValueError(f"exact labels do not use clusters 1 through {cluster_count}: {path}")
    return sorted(labels_by_count.items())


def convert_sos_sdp_reference(repository, output_directory):
    repository = Path(repository)
    output_directory = Path(output_directory)
    summary = read_groundtruth_summary(repository)
    data_directory = repository / "input_data_for_exact_solver" / "real_data"
    selected_real_data_path = data_directory / "opt_mssc_real_data_true_label.csv"
    exact_directory = repository / "Results_clustering"
    converted = []
    with selected_real_data_path.open(encoding="utf-8", newline="") as file:
        for row in csv.reader(file):
            if len(row) != 2 or not row[0]:
                continue
            name = row[0]
            information = summary[name]
            points = read_header_points(data_directory / f"{name}.txt")
            if len(points) != information["point_count"] or len(points[0]) != information["dimension"]:
                raise ValueError(f"summary does not match {name}")
            reference_labels = read_groundtruth_labels(data_directory / f"{name}_with_clust_labels.txt", points)
            source = (
                "GroundTruth_VS_OptimalCluster published SOS-SDP clustering, "
                f"commit {GROUNDTRUTH_COMMIT}"
            )
            exact_clusterings = read_exact_clusterings(
                exact_directory / f"{name}_clustering_sum.txt", points
            )
            for cluster_count, exact_labels in exact_clusterings:
                best_known = clustering_cost(points, exact_labels)
                case_name = f"sos_sdp_reference_{name}_k{cluster_count}"
                output = output_directory / f"{case_name}.tcb"
                write_case(
                    output,
                    case_name,
                    source,
                    points,
                    cluster_count,
                    [(1, len(points))] * cluster_count,
                    best_known,
                    reference_labels,
                )
                converted.append(output)
    return converted


def convert_groundtruth_command(args):
    converted = convert_sos_sdp_reference(args.repository, args.output_directory)
    for path in converted:
        print(path)


def download_groundtruth_command(args):
    with tempfile.TemporaryDirectory() as temporary_directory:
        archive = Path(temporary_directory) / "data.zip"
        with urllib.request.urlopen(GROUNDTRUTH_URL, timeout=60) as response, archive.open("wb") as file:
            shutil.copyfileobj(response, file)
        digest = hashlib.sha256(archive.read_bytes()).hexdigest()
        with zipfile.ZipFile(archive) as file:
            file.extractall(temporary_directory)
        roots = list(Path(temporary_directory).glob("GroundTruth_VS_OptimalCluster-*"))
        if len(roots) != 1:
            raise RuntimeError("downloaded archive has an unexpected layout")
        converted = convert_sos_sdp_reference(roots[0], args.output_directory)
        metadata = Path(args.output_directory) / "sos_sdp_reference_source.txt"
        metadata.write_text(
            f"url {GROUNDTRUTH_URL}\ncommit {GROUNDTRUTH_COMMIT}\narchive_sha256 {digest}\n",
            encoding="utf-8"
        )
    for path in converted:
        print(path)
    print(metadata)


def main():
    parser = argparse.ArgumentParser()
    subparsers = parser.add_subparsers(dest="command", required=True)
    plain = subparsers.add_parser("convert")
    add_conversion_arguments(plain)
    plain.set_defaults(function=convert_plain)
    table = subparsers.add_parser("convert-table")
    add_conversion_arguments(table)
    table.add_argument("--dimension", required=True, type=int)
    table.add_argument("--skip-lines", type=int, default=0)
    table.add_argument("--skip-columns", type=int, default=0)
    table.set_defaults(function=convert_table)
    groundtruth = subparsers.add_parser(
        "sos-sdp-reference", aliases=["sos-sdp-exact", "groundtruth-optimal"]
    )
    groundtruth.add_argument("repository")
    groundtruth.add_argument("output_directory")
    groundtruth.set_defaults(function=convert_groundtruth_command)
    download = subparsers.add_parser(
        "download-sos-sdp-reference",
        aliases=["download-sos-sdp-exact", "download-groundtruth-optimal"],
    )
    download.add_argument("output_directory")
    download.set_defaults(function=download_groundtruth_command)
    args = parser.parse_args()
    args.function(args)


if __name__ == "__main__":
    main()
