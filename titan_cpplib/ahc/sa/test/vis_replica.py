import sys
import numpy as np
import matplotlib.pyplot as plt


def visualize_replica_scores(csv_file):
    try:
        # ヘッダー行からカラム名を取得
        with open(csv_file, "r") as f:
            header = f.readline().strip().split(",")
    except FileNotFoundError:
        print(f"Error: ファイル '{csv_file}' が見つかりません。")
        sys.exit(1)

    try:
        # 1 列目: time_ms, 2 列目以降: 各レプリカのスコア
        data = np.loadtxt(csv_file, delimiter=",", skiprows=1)
    except Exception as e:
        print(f"Error: データの読み込みに失敗しました。詳細: {e}")
        sys.exit(1)

    # データが 1 行しかない場合は 2 次元配列に変換する
    if data.ndim == 1:
        data = data.reshape(1, -1)

    time_ms = data[:, 0]
    num_replicas = data.shape[1] - 1

    fig, ax = plt.subplots(figsize=(12, 8))

    # カラーマップの設定 / レプリカ数が多い場合でも見分けやすいように tab20 を使用する
    # cmap = plt.get_cmap("tab20")

    for i in range(num_replicas):
        ax.plot(
            time_ms,
            data[:, i + 1],
            label=header[i + 1],
            # color=cmap(i % 20),
            linewidth=1.5,
            alpha=0.8,
        )

    ax.set_title("Replica Exchange SA: Score Transitions")
    ax.set_xlabel("Time (ms)")
    ax.set_ylabel("Score")

    # 凡例をグラフの外側に配置
    # ax.legend(bbox_to_anchor=(1.01, 1), loc="upper left")
    ax.grid(True, linestyle="--", alpha=0.6)

    # 序盤のスコアが悪すぎて終盤の変化が見えない場合に、Y 軸の表示範囲を絞るための設定
    # 必要に応じて以下のコメントアウトを解除して使用する
    # y_min = np.min(data[:, 1:])
    # y_max = np.percentile(data[:, 1:], 90) # 上位 10% の悪いスコアを描画範囲から除外する
    # ax.set_ylim(y_min, y_max)

    plt.tight_layout()
    plt.savefig("vis.png")
    plt.show()


if __name__ == "__main__":
    visualize_replica_scores(sys.argv[1])
