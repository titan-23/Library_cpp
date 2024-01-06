#include <iostream>
#include <set>
#include <vector>
using namespace std;

namespace titan23 {

  struct Osa_k {
    int n;
    vector<int> min_factor;

    Osa_k(const int n) : n(n), min_factor(n+1) {
      for (int i = 0; i <= n; ++i) {
        min_factor[i] = i;
      }
      for (int i = 2; i*i <= n; ++i) {
        if (min_factor[i] == i) {
          for (int j = 2; j <= n/i; ++j) {
            if (min_factor[i*j] > i) {
              min_factor[i*j] = i;
            }
          }
        }
      }
    }

    vector<int> p_factorization(int n) const {
      vector<int> ret;
      while (n > 1) {
        ret.emplace_back(min_factor[n]);
        n /= min_factor[n];
      }
      return ret;
    }
  };
}  // namespace titan23
