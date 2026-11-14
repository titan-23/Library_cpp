# hash_dict.cpp

## ハッシュ関数

- USE_HASH_FUNC=true では、128 bit の積の上位と下位を xor する。wyhash の mum と同じ方法である。
- 1 回の乗算で全ビットが混ざる。上位ビットだけが変わる鍵や連番の鍵でも偏らない。

## get の構造化束縛

- get では構造化束縛に const を付けない。
- `const auto [pos, exist] = get_pos(key)` と書くと、GCC は見つかった場合と見つからなかった場合の分岐を合流させる。命令数は 2 割ほど増える。
- GCC 15、-O2 で確認した。
