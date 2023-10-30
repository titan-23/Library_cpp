#include <iostream>
#include <vector>
#include <set>
#include <cassert>
using namespace std;

namespace titan23 {

  struct RandomTree {
    int n;

    RandomTree(int n) : n(n) {}

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

    int indx(vector<int> &a, int x) {
      for (int i = 0; i < (int)a.size(); ++i) {
        if (a[i] == x) return i;
      }
      return -1;
    }

    vector<pair<int, int>> gen_random() {
      // https://speakerdeck.com/tsutaj/beerbash-lt-230711?slide=27
      
      assert(n >= 0);
      vector<pair<int, int>> edges;
      if (n <= 1) {
        return edges;
      }
      edges.reserve(n-1);
      vector<int> D(n, 1);
      vector<int> A(n-2, 0);
      for (int i = 0; i < n-2; ++i) {
        int v = randrange(0, n);
        D[v]++;
        A[i] = v;
      }
      multiset<pair<int, int>> mst;
      for (int i = 0; i < n; ++i) {
        mst.insert({D[i], i});
      }
      for (const int &a: A) {
        auto it = mst.begin();
        int d = it->first, v = it->second;
        mst.erase(it);
        assert(d == 1);
        edges.emplace_back(v, a);
        D[v]--;
        mst.erase({D[a], a});
        D[a]--;
        if (D[a] >= 1) {
          mst.insert({D[a], a});
        }
      }
      int u = indx(D, 1);
      D[u]--;
      int v = indx(D, 1);
      edges.emplace_back(u, v);
      assert(edges.size() == n-1);
      return edges;
    }

    vector<pair<int, int>> gen_path() {
      // ホントにランダム一葉???
      vector<int> p(n);
      for (int i = 0; i < n; ++i) {
        p[i] = i;
      }
      shuffle(p);
      vector<pair<int, int>> edges(n-1);
      for (int i = 0; i < n-1; ++i) {
        edges[i] = {p[i], p[i+1]};
      }
      return edges;
    }
  };
} // namespace titan23
