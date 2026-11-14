/// https://github.com/titan-23/Library_cpp/blob/main/titan_cpplib/alg/subset.cpp
// n 要素のうち大きさ k の部分集合 c を昇順に列挙する
int c = (1<<k)-1;
while (c < (1<<n)) {
  // ここで c を使う
  if (c == 0) break;
  int x = c&-c, y = c+x;
  c = (((c&~y)/x)>>1) | y;
}
// b の部分集合 a を降順に列挙する
for (int a = b; ; a = (a-1)&b) {
  // ここで a を使う
  if (!a) break;
}