# hash_dict.cpp

## ハッシュ関数

- USE_HASH_FUNC=true では、128 bit の積の上位と下位の xor を求めます。wyhash の mum と同じ方法です。
- 1 回の乗算で全ビットが混ざります。上位ビットだけが異なる鍵や連番の鍵でもハッシュ値は偏りません。

## get の構造化束縛

- get では構造化束縛に const を付けません。
- `const auto [pos, exist] = get_pos(key)` と記述すると、GCC は見つかった場合と見つからなかった場合の分岐を合流させます。命令数は 2 割ほど増加します。
- GCC 15、-O2 で確認しました。
