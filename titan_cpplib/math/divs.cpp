#include <iostream>
#include <set>
#include <vector>
#include <map>
using namespace std;

// Osa_k
namespace titan23 {

  struct Osa_k {
    int n;
    vector<int> min_factor;

    Osa_k(const int n) : n(n), min_factor(n+1) {
      iota(min_factor.begin(), min_factor.end(), 0);
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

    map<int, int> p_factorization_map(int n) const {
      map<int, int> ret;
      while (n > 1) {
        ++ret[min_factor[n]];
        n /= min_factor[n];
      }
      return ret;
    }
  };
}  // namespace titan23
