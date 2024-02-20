#include <string>
using namespace std;

bool is_kaibun(string &s) {
  int n = (int)s.size();
  for (int i = 0; i < n/2; ++i) {
    if (s[i] != s[n-i-1]) return false;
  }
  return true;
}
