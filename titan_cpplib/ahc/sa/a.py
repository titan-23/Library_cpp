import sys
import math
import matplotlib.pyplot as plt


def read_coords(filepath):
    coords = []
    original_n = 0
    is_coord_section = False
    with open(filepath, "r") as f:
        for line in f:
            line = line.strip()
            if not line:
                continue
            if line.startswith("DIMENSION"):
                # "DIMENSION : 100" や "DIMENSION: 100" に対応
                original_n = int(line.split()[-1])
            if line == "NODE_COORD_SECTION":
                is_coord_section = True
                continue
            if line == "EOF":
                break
            if is_coord_section:
                tokens = line.split()
                coords.append((float(tokens[1]), float(tokens[2])))
    return coords, original_n


def calc_tour_score(coords, tour):
    score = 0
    n = len(tour)
    if n <= 1:
        return 0
    for i in range(n):
        u, v = tour[i], tour[(i + 1) % n]
        dx = coords[u][0] - coords[v][0]
        dy = coords[u][1] - coords[v][1]
        score += int(math.sqrt(dx * dx + dy * dy) + 0.5)
    return score


def main():
    if len(sys.argv) < 2:
        print("Usage: python visualizer.py <tsp_filepath>")
        sys.exit(1)

    filepath = sys.argv[1]
    coords, original_N = read_coords(filepath)

    # 標準入力（C++の出力）からツアーを読み込む
    input_data = sys.stdin.read().strip().split("\n")
    if not input_data or not input_data[0]:
        print("No tour data found in stdin.")
        sys.exit(1)

    tours = [[int(x) for x in line.split()] for line in input_data if line.strip()]

    # --- デポ座標の補完ロジック ---
    # C++側で N += K した後の最大インデックスを確認
    max_node_idx = 0
    for tour in tours:
        if tour:
            max_node_idx = max(max_node_idx, max(tour))

    # K を算出し、C++側と同じロジック (N/K*k) で座標を追加
    if max_node_idx >= original_N:
        K = max_node_idx - original_N + 1
        for k in range(K):
            # C++: YX[N+k] = YX[N/K*k]
            source_idx = (original_N // K) * k
            coords.append(coords[source_idx])
        print(f"Detected K={K}, Added {K} depot coordinates.")

    # --- 描画処理 ---
    plt.figure(figsize=(10, 10))
    cmap = plt.get_cmap("tab10")
    total_dist = 0
    max_dist = 0

    for idx, tour in enumerate(tours):
        if not tour:
            continue
        d = calc_tour_score(coords, tour)
        total_dist += d * d
        max_dist = max(max_dist, d)

        tx = [coords[i][0] for i in tour] + [coords[tour[0]][0]]
        ty = [coords[i][1] for i in tour] + [coords[tour[0]][1]]

        color = cmap(idx % 10)
        plt.plot(
            tx,
            ty,
            marker=".",
            markersize=4,
            alpha=0.7,
            color=color,
            label=f"R{idx}: {d}",
        )
        # デポ（ツアーの1番目）を強調
        plt.plot(
            tx[0],
            ty[0],
            marker="*",
            markersize=12,
            color=color,
            markeredgecolor="black",
        )

    plt.title(f"mTSP: MaxDist={max_dist}, TotalDist={total_dist}")
    plt.legend()
    plt.grid(True)
    plt.savefig("result2.png")
    print(f"Saved to result2.png (Total: {total_dist}, Max: {max_dist})")


if __name__ == "__main__":
    main()
