#include <iostream>
#include <vector>
#include <cassert>
using namespace std;

// FenwickTree2D
namespace titan23 {

  template<typename T>
  struct FenwickTree2D {
    int h, w;
    vector<T> _bit;

    FenwickTree2D() {}
    FenwickTree2D(int h, int w) : h(h+1), w(w+1) {
      
    }

  };
}  // namespace titan23
