# titan_cpplib/geometry レビュー

対象は1ファイル。

- geometry.cpp

重要度は3段階。

- **[バグ]** 誤動作・UB・コンパイル不能につながる
- **[注意]** 特定条件で問題になる
- **[軽微]** 動作に影響しない指摘

## バグ一覧(要約)

| 関数 | 内容 |
|---|---|
| getCircleLineIntersection | 交点が無いとき空の vector に res[0] で添字アクセスし範囲外参照 |
| getCommonTangentsLine | 外接線と内接線の構成が混線し、返る線分がどれも接線にならない |
| countCirclesIntersection | 交点の個数ではなく共通接線の本数を返す(離れた2円で4を返す) |
| convexHullDiameter | 距離の単峰性を前提にした打ち切りが凸多角形では成立せず、最遠点対を見逃しうる |
| convexHull | include_collinear=true かつ全点共線のとき同じ頂点を2回含む列を返す |
| countIntersections | activeSegments が set のため、同一 y の水平線分が複数重なると数え漏れる |

## 横断事項

- **[注意] EPS が絶対値 1e-9 固定**。外積・内積は座標の2乗スケールで効くため、座標が 1e6 を超えると許容誤差が実質不足し、座標が小さいと過大になる。isIntersecting の d1\*d2 のような積の比較では4乗スケールになり、さらにずれる。相対誤差との併用か、想定する座標範囲の明記が要る。
- **[注意] EPS 付き比較を sort / inplace_merge の比較器に使っている**。convexHull・getCommonTangentsLine の sort、closestPair の inplace_merge、countIntersections の事前 sort が該当する。almostEqual は推移律を満たさないため strict weak ordering にならず、std::sort の前提違反で未定義動作になりうる。比較器内は生の `<` で十分。
- **[注意] 単体でコンパイルできない**。`#pragma once` も include も `namespace titan23` もなく、`rep` マクロと `using namespace std` を呼び出し側に依存する。リポジトリの規約(全ファイル `#pragma once`、`namespace titan23`、標準 include のみ)から外れている。
- **[軽微]** 非 inline の自由関数をヘッダ相当のファイルに定義している。複数翻訳単位で ODR 違反になる。単一ファイル提出なら実害なし。
- **[軽微] EPS の使用が不統一**。cutPolygon の isLeft、countIntersections の座標比較、getCirclesIntersect の存在判定、Circle::operator== の r は生比較で、他の EPS 付き判定と食い違う。

## 基本構造体(Point / Line / Segment / Circle)

- Point の演算子、dot / cross / norm / abs / arg は正しい。
- Line(A, B, C) の4分岐(A≈0、B≈0、C≈0、一般)を全て検証した。いずれも Ax+By=C を満たす2点を返す。A≈0 かつ B≈0 は assert で弾いている。正しい。
- **[軽微]** Segment が `using Line::Line` で直線の方程式コンストラクタを継承している。方程式から作った Segment の端点は直線上の便宜的な2点で、線分として意味を持たない。
- **[軽微]** Circle::operator== が r を生の `==` で比べる。center は almostEqual なのに不統一。

## ccw / projection / reflection / 平行・直交判定

- ccw は標準形で正しい。p0 == p1 の退化時は ONLINE_FRONT か ON_SEGMENT に落ちるが実害は薄い。
- **[注意] projection は p1 == p2 でゼロ除算**。base.norm() が 0 になる。distancePointToSegment にも波及する。退化線分を渡さない前提の明記か assert が要る。
- reflection、isParallel / isOrthogonal の6オーバーロード、isPointOnLine、isPointOnSegment は正しい。

## 線分の交差(isIntersecting / getIntersection / distance 系)

- isIntersecting の構成(符号判定+端点オンライン判定)は標準形で正しい。
- **[注意] d1\*d2 の積で符号判定している**。d1 = 1e-5、d2 = -1e-5 のような真の交差で積が -1e-10 となり、almostEqual で 0 扱いになる。その後の端点判定にも掛からず false を返す。d1 と d2 の符号を個別に判定すべき。
- **[注意] getIntersection は共線で重なる線分に対して NaN を返す**。共線重複は isIntersecting を通るが、d1 = d2 = 0 となり t = 0/0 になる。assert では防げない。1点で交わる場合(端点接触の T 字を含む)は正しいことを確認した。
- distancePointToSegment、distanceSegmentToSegment は正しい。
- **[軽微]** isIntersecting の doc コメントが「直線の交差判定」になっているが、引数も中身も線分。

## 多角形

- getPolygonArea(靴紐公式)、isConvex は正しい。
- convexPolygonContainsPoint は、くさび判定・二分探索・最終の三角形判定、および境界(p が hull[0] に一致、辺 hull[0]-hull[1] の延長上など)を検証した。正しい。**[軽微]** 反時計回りの凸包が前提だが doc に記載がない。convexHull の出力(反時計回り)とは整合する。
- isPointOnPolygon は正しい。doc は「凸多角形」だが任意の単純多角形で動く。
- **[注意] isPointInsidePolygon は辺上の点の結果が不定**。doc は「辺上は含まない」だが、実際は辺上でどちらも返りうる。正方形 (0,0),(1,0),(1,1),(0,1) で、左辺上の (0, 0.5) は true、右辺上の (1, 0.5) は false になる。辺上を除外したいなら isPointOnPolygon で先に弾く手順を doc に書くべき。頂点通過は半開区間 [a.y, b.y) の扱いで正しい。
- cutPolygon の交点計算(A1/(A1-A2))は正しい。交差判定が立つとき A1 ≠ A2 が保証されることも確認した。**[軽微]** isLeft が EPS なしの生比較で、直線上の頂点は出力から落ちる(交点として復元される場合を除く)。切断線が頂点を通ると重複点や退化辺が出うる。

## 凸包(convexHull / convexHullDiameter)

- convexHull の主経路は正しい。y→x ソート後の monotone chain で、出力は最下点始まりの反時計回りになることを正方形の例で追跡確認した。全点共線(include_collinear=false)も {端点2つ} を返し正しい。
- **[バグ] include_collinear=true かつ全点共線で頂点が重複する**。共線3点 p0, p1, p2 では upper と lower が両方 {p0, p1, p2} になり、結合結果が {p0, p1, p2, p1} と p1 を2回含む。cp-algorithms でもこのケースは特別扱いしており、全点共線の分岐が要る。
- **[バグ] convexHullDiameter の打ち切り条件が誤り**。「hull[i] からの距離が増える間 k を進め、減ったら止める」は距離の単峰性を仮定するが、凸多角形の頂点間距離は単峰とは限らない。凸多角形 (0,0), (10,0), (6,6), (0,10) で (0,0) からの距離は 10, 8.49, 10 と途中で凹む。k は後退しないため、早く止まった k がその後の i でも最遠対を跨いでしまい、最大値を取り逃す入力があり得る。正しい回転キャリパーは距離でなく、辺に対する外積(面積)の比較で対蹠点を進める。凸包サイズが小さいなら全対全 O(n²) の方が安全。
- **[軽微]** 同関数の距離比較が `dist1.abs() > dist2.abs()` と sqrt 後の値で行われている。sqrt は単調なので norm() の比較で結果は変わらず、sqrt の分だけ無駄で僅かに精度も落とす。

## closestPair

- 分割統治のロジック(y でのマージ、帯の走査、`dv.y >= d` での打ち切り)は正しい。計算量も O(n log n)。
- x 座標ソート済みが前提である点は doc に記載あり。inplace_merge の比較器の問題は横断事項の通り。

## countIntersections

- 中身は軸平行(水平・垂直)線分専用の平面走査だが、**[注意] doc「線分の交差数を数える」に前提の記載がない**。斜めの線分は `seg.a.y == seg.b.y` の判定で垂直扱いになり、seg.a.x しか見ないため誤った結果を黙って返す。
- **[バグ] activeSegments が set&lt;Real&gt;**。同じ y を持つ水平線分が区間を重ねて複数アクティブになると、insert が重複で潰れ、先に終わる線分の erase で残りの線分も消える。以降その y との交差を数え漏れる。multiset か座標圧縮+カウントにすべき。
- **[軽微]** 冒頭の segments のソートは、直後に events を別途ソートするため無意味。削除してよい。
- **[軽微]** Event::operator< と水平判定が生の `==` で、他の EPS 付き判定と不統一。水平線分同士・垂直線分同士の重なりを数えない仕様も明記した方がよい。
- イベント順(同一 x で start → vertical → end)は端点接触を交差に数える向きで一貫している。distance(lower, upper) は数えた本数に比例するため、全体は O(n log n + 交差数) で問題ない。

## 円

- **[バグ] countCirclesIntersection は共通接線の本数を返している**。離れた2円(d > r1+r2)で 4、外接で 3、交差で 2、内接で 1、包含で 0。交点の個数なら 0, 1, 2, 1, 0 のはず。中身は正しく接線本数の分類なので、doc と関数名を countCommonTangents 相当に直すのが早い。
- getInCircle は正しい(a=|BC| と重み付けの対応を確認)。**[軽微]** ヘロンの公式は針状三角形で桁落ちする。共線入力では s-a などが誤差で負になり sqrt が NaN を返しうる。
- getCircumCircle の公式は正しい。**[注意] 3点共線で D ≈ 0 となりゼロ除算**。ガードがない。
- **[バグ] getCircleLineIntersection は交点が無いとき範囲外参照**。判別式が負(かつ almostEqual で 0 でない)のとき res は空のまま、末尾の並べ替えで res[0]・res[1] にアクセスする。空なら並べ替えを飛ばして return すべき。
- **[注意]** 同関数の t1 = (-b + √D) / (2a) は b と √D が近いとき桁落ちする。解の公式の安定形(符号で分けて q = -(b + sign(b)√D)/2 を使う形)が望ましい。判別式への絶対 EPS も横断事項のスケール問題を受ける。**[軽微]** 接するときは同一点を2個返す仕様が doc にない。
- getCirclesIntersect の交点構成(a, h, 垂直ベクトル)は正しい。**[注意]** 存在判定が生の `>` `<` で、接する場合に浮動小数点誤差で空を返しうる。EPS 判定の countCirclesIntersection と結果が食い違う。h = sqrt(r1²-a²) は接する付近で被開平数が誤差で負になり NaN を返しうるため、max(Real(0), ·) でのクランプが要る。d ≈ 0(同心円)はゼロ除算。
- getTangentLinesFromPoint は正しい。d > r なら a = r²/d < r で被開平数は負にならないことも確認した。**[軽微]** 名前は Lines だが返すのは接点。doc は接点と書いてあり名前だけ紛らわしい。
- **[バグ] getCommonTangentsLine は外接線と内接線の構成が混線している**。半径 r1, r2、中心距離 d に対し、正しくは外接線が θ = acos((r1-r2)/d) で両円とも中心線から同方向(c2 側 +)、内接線が θ = acos((r1+r2)/d) で c2 側は逆方向(-)。現状のコードは、前半ブロックが条件 d ≥ r1+r2・θ = acos((r1+r2)/d) と内接線の構成なのに c2 側が +、後半ブロックが条件 d ≥ |r1-r2|・θ = acos((r1-r2)/d) と外接線の構成なのに c2 側が -。r1 = r2 = 1、d = 4 で検証すると、前半は直線 y = ±0.866(中心からの距離 0.866 ≠ 1)、後半は (0,±1)-(4,∓1) を結ぶ交差線(距離 0.894 ≠ 1)で、4本とも接線でない。修正は cx2 / cy2 の符号を2ブロック間で入れ替えること。そうすると各ブロックの条件・θ・符号が内接線/外接線として整合する(External / Internal のコメントは逆になる)。接する場合(break の分岐)は2接点が一致し長さ 0 の線分が返る点も doc に書くとよい。
