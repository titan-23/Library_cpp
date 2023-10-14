bool is_kaibun(string s) {
  int n = s.size();
  for (int i = 0; i < n/2; ++i) {
    if (s[i] == s[n-i-1]) continue;
    return false;
  }
  return true;
}
