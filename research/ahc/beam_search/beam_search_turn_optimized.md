# beam_search_turn_optimized.cpp

## 候補プールのハッシュ辞書

- `Candidates::push` は、置換で候補プールから除外した候補のキーを `hash_to_idx` から削除せずに残します。
- 辞書から取得した添字のスロットにある hash が検索に使用した hash と一致するときだけ、そのスロットの候補を候補プールに残っている同一の候補とみなします。
- 一致しないときは辞書に残った旧キーとみなし、新しい候補として扱います。
- この確認により、旧キーを -1 に更新するための再探索を省略できます。

## process_candidate を一か所に置く理由

- `State::try_op` と候補の登録を呼び出し箇所ごとに複製すると、命令キャッシュを圧迫します。
- 候補の列挙が支配的な場合は、hot section に 1 つだけ配置するほうが高速です。
- そのため `process_candidate` に `gnu::hot` と `gnu::noinline` を付けます。clang 以外では `gnu::noclone` も付けます。

## BeamCandidate の親番号

- `BeamCandidate` は、現在のメタターンで一度採用された候補を表します。
- 通常は候補ごとに親番号を保持せず、範囲の境界で親を表します。
- `SORT_CANDIDATES_BY_SCORE` が false のとき、`BeamCandidate` は親番号を保持しない `EmptyCandidateParent` を継承します。親ごとの区切りは `candidate_group_ends` に格納されます。
