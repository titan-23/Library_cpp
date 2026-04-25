import numpy as np
import matplotlib.pyplot as plt
import os
import sys


def generate_input(filename="input.txt", N=1000, K=10):
    """指定された N, K でテストケースを生成する"""
    # 1. 各クラスタの目標サイズ A_i を決定 (合計がNになるように分配)
    base_size = N // K
    remainder = N % K
    sizes = [base_size] * K
    for i in range(remainder):
        sizes[np.random.randint(0, K)] += 1

    # 2. 点の生成 (意図的にいくつかのまとまり=Blobを作る)
    points = []
    for _ in range(K):
        # ランダムな中心点を決める
        cx, cy = np.random.uniform(0, 100, 2)
        # 中心点の周りに正規分布で点を散らす
        num_points = np.random.randint(base_size // 2, int(base_size * 1.5))
        blob = np.random.normal(loc=[cx, cy], scale=8.0, size=(num_points, 2))
        points.extend(blob)

    # 点の数がNになるように調整
    points = np.array(points)
    if len(points) > N:
        indices = np.random.choice(len(points), N, replace=False)
        points = points[indices]
    elif len(points) < N:
        extra = np.random.uniform(0, 100, (N - len(points), 2))
        points = np.vstack([points, extra])

    np.random.shuffle(points)  # 順番をシャッフル

    # 3. ファイルへの書き出し
    with open(filename, "w") as f:
        f.write(f"{N} {K}\n")
        f.write(" ".join(map(str, sizes)) + "\n")
        for p in points:
            f.write(f"{p[0]:.4f} {p[1]:.4f}\n")

    print(f"[*] {filename} を生成しました。 (N={N}, K={K})")


def visualize_result(
    input_file="input.txt", output_file="output.txt", image_file="result2.png"
):
    """入力ファイルとC++の出力結果を読み込んで可視化する"""
    if not os.path.exists(output_file):
        print(
            f"[!] エラー: {output_file} が見つかりません。先にC++を実行してください。"
        )
        return

    # 入力の読み込み
    with open(input_file, "r") as f:
        lines = f.read().splitlines()
        N, K = map(int, lines[0].split())
        sizes = list(map(int, lines[1].split()))
        points = np.array([list(map(float, lines[i].split())) for i in range(2, 2 + N)])

    # 出力の読み込み (各行にクラスタID 0~K-1 が出力されている想定)
    with open(output_file, "r") as f:
        assignments = list(map(int, f.read().split()))

    assert len(assignments) == N, "出力された点の数がNと一致しません"

    # 可視化
    plt.figure(figsize=(10, 8))

    # 点のプロット (クラスタごとに色分け)
    scatter = plt.scatter(
        points[:, 0], points[:, 1], c=assignments, cmap="tab20", s=20, alpha=0.8
    )

    # 割り当てられた実際のサイズを確認し、重心を計算
    actual_sizes = [0] * K
    centroids = np.zeros((K, 2))
    for i in range(N):
        c_id = assignments[i]
        actual_sizes[c_id] += 1
        centroids[c_id] += points[i]

    for j in range(K):
        if actual_sizes[j] > 0:
            centroids[j] /= actual_sizes[j]

    # 重心を赤い×印でプロット
    plt.scatter(
        centroids[:, 0],
        centroids[:, 1],
        c="red",
        marker="x",
        s=100,
        linewidths=2,
        label="Centroids",
    )

    # サイズ制約が守られているかチェック
    is_valid = sizes == actual_sizes
    title = f"Size-Constrained K-means (N={N}, K={K})\n"
    title += "Constraints Satisfied!" if is_valid else "Constraint Violation!"

    plt.title(
        title, fontsize=14, fontweight="bold", color="green" if is_valid else "red"
    )
    plt.legend()
    plt.grid(True, linestyle="--", alpha=0.5)

    plt.savefig(image_file, dpi=150)
    print(f"[*] 結果を {image_file} に保存しました。")
    plt.show()


if __name__ == "__main__":
    mode = sys.argv[1] if len(sys.argv) > 1 else "gen"

    if mode == "gen":
        generate_input("input.txt", N=1000, K=10)
    elif mode == "vis":
        visualize_result("input.txt", "out.txt")
    else:
        print("Usage: python tester.py [gen|vis]")
