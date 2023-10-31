#include <vector>
#include <cassert>
using namespace std;

namespace titan23 {

  int randrange(int begin, int end) {
    assert(begin < end);
    return begin + rand() % (end - begin);
  }

  void shuffle(vector<int> &a) {
    int n = (int)a.size();
    for (int i = 0; i < n-1; ++i) {
      int j = randrange(i, n);
      swap(a[i], a[j]);
    }
  }

} // namespace titan23
