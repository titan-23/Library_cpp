#include <string>
using namespace std;

bool is_kaibun(string &s) {
  for (int i = 0; i < s.size()/2; ++i) {
    if (s[i] != s[s.size()-i-1]) return false;
  }
  return true;
}
