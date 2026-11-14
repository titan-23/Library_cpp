# beam_search_turn_optimized.cpp

## 候補プールのハッシュ辞書

- `Candidates::push` は、置換で追い出した候補のキーを `hash_to_idx` から消さずに残す。
- 辞書から得た添字のスロットにある hash が引いた hash と一致するときだけ、そのスロットは生存中の同一候補である。
- 一致しないときは、辞書に残った旧キーとみなし、新しい候補として扱う。
- この確認により、旧キーを -1 に更新するための再探索を省ける。

## process_candidate を一か所に置く理由

- `State::try_op` と候補登録を呼出箇所ごとに複製すると、命令キャッシュを圧迫する。
- 候補列挙が支配的な場合は、hot section に一つだけ置く方が速い。
- そのため `process_candidate` に `gnu::hot` と `gnu::noinline` を付ける。clang 以外では `gnu::noclone` も付ける。

## BeamCandidate の親番号

- `BeamCandidate` は、現在のメタターンで一度採用された候補を表す。
- 通常は親番号を候補に持たせず、範囲の境界へ分離する。
- `SORT_CANDIDATES_BY_SCORE` が false のとき、`BeamCandidate` は親番号を持たない `EmptyCandidateParent` を継承する。親ごとの区切りは `candidate_group_ends` に入る。
