// #include "titan_cpplib/data_structures/max_heap.cpp"
#include <iostream>
#include <vector>
using namespace std;

// MaxHeap
namespace titan23 {

    template<typename T>
    class MaxHeap {
      public:
        vector<T> a;

      private:
        void _heapify() {
            for (int i = (int)a.size(); i >= 0; --i) {
                _down(i);
            }
        }

        void _down(int i) {
            int n = a.size();
            while (i<<1|1 < n) {
                int u = i*2+1, v = i*2+2;
                if (v < n && a[u] < a[v]) u = v;
                if (a[i] < a[u]) {
                    swap(a[i], a[u]);
                    i = u;
                } else {
                    break;
                }
            }
        }

        void _up(int i) {
            while (i > 0) {
                int p = (i - 1) >> 1;
                if (a[i] > a[p]) {
                    swap(a[i], a[p]);
                    i = p;
                } else {
                    break;
                }
            }
        }

      public:
        MaxHeap() {}
        MaxHeap(vector<T> a) : a(a) { _heapify(); }

        //! 最大の要素を返す / `O(1)`
        T get_max() const {
            return a[0];
        }

        //! 最大の要素を削除して返す / `O(logn)`
        T pop_max() {
            T res = a[0];
            a[0] = a.back();
            a.pop_back();
            _down(0);
            return res;
        }

        //! `key` を追加する / `O(logn)`
        void push(const T key) {
            a.emplace_back(key);
            _up(a.size() - 1);
        }

        //! `push` して `pop` する / `O(logn)`
        T pushpoop_max(const T key) {
            if (a[0] <= key) return key;
            T res = a[0];
            a[0] = key;
            _down(0);
            return res;
        }

        //! 最大の要素を `pop` して返し、新たに `key` を挿入する。 `pop -> push` と同じ / `O(logn)`
        T replace_max(const T key) {
            T res = a[0];
            a[0] = key;
            _down(0);
            return res;
        }

        //! 要素数を返す / `O(1)`
        int len() const {
            return (int)a.size();
        }

        //! 表示する
        void print() const {
            vector<T> b = a;
            sort(b.begin(), b.end());
            cout << '[';
            for (int i = 0; i < len()-1; ++i) {
                cout << b[i] << ", ";
            }
            if (!b.empty()) {
                cout << b.back();
            }
            cout << ']' << endl;
        }
    };
} // namespace titan23

