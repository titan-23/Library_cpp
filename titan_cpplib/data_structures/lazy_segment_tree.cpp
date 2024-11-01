#include <vector>
#include <cassert>
using namespace std;

namespace titan23 {

    template <class T,
            T (*op)(T, T),
            T (*e)(),
            class F,
            T (*mapping)(F, T),
            F (*composition)(F, F),
            F (*id)()>
    class LazySegmentTree {
      private:
        int n, log, size;
        vector<T> data;
        vector<F> lazy;

        static int bit_length(const int x) {
            if (x == 0) return 0;
            return 32 - __builtin_clz(x);
        }

        void update(int k) {
            data[k] = op(data[k << 1], data[k << 1 | 1]);
        }

        void all_apply(int k, F f) {
            data[k] = mapping(f, data[k]);
            if (k >= size) return;
            lazy[k] = composition(f, lazy[k]);
        }

        void propagate(int k) {
            if (lazy[k] == id()) return;
            all_apply(k << 1, lazy[k]);
            all_apply(k << 1 | 1, lazy[k]);
            lazy[k] = id();
        }

        void upper_propagate(int l, int r) {
            for (int i = log; i > 0; --i) {
                if ((l>>i<<i) != l) {
                    propagate(l >> i);
                }
                if (((r>>i<<i) != r) && ( ((l>>i) != ((r-1)>>i)) || ((l>>i<<i) == l) )) {
                    propagate((r-1) >> i);
                }
            }
        }

      public:
        LazySegmentTree() : n(0), log(0), size(0) {}
        LazySegmentTree(int n) {
            this->n = n;
            this->log = bit_length(n-1);
            this->size = 1 << log;
            data.resize(this->size<<1, e());
            lazy.resize(this->size, id());
        }

        LazySegmentTree(const vector<T> a) {
            this->n = a.size();
            this->log = bit_length(n-1);
            this->size = 1 << log;
            data.resize(this->size<<1, e());
            for (int i = size; i < size+n; ++i) {
                data[i] = a[i-size];
            }
            for (int i = size-1; i > 0; --i) {
                data[i] = op(data[i<<1], data[i<<1|1]);
            }
            lazy.resize(this->size, id());
        }

        void apply_point(int k, F f) {
            k += size;
            for (int i = log; i > 0; --i) {
                propagate(k >> i);
            }
            data[k] = mapping(f, data[k]);
            for (int i = 1; i <= log; ++i) {
                update(k >> i);
            }
        }

        void apply(int l, int r, F f) {
            if (l == r || f == id()) return;
            l += size;
            r += size;
            upper_propagate(l, r);
            int l2 = l, r2 = r;
            while (l < r) {
                if (l & 1) {
                    all_apply(l, f);
                    l += 1;
                }
                if (r & 1) {
                    all_apply(r ^ 1, f);
                }
                l >>= 1;
                r >>= 1;
            };
            int l3 = l2, r3 = r2 - 1;
            for (int i = 1; i <= log; ++i) {
                l3 >>= 1;
                r3 >>= 1;
                if ((l3 << i) != l2) {
                    update(l3);
                }
                if ((((l3 << i) == l2) || (l3 != r3)) && ((r2>>i<<i) != r2)) {
                    update(r3);
                }
            }
        }

        void all_apply(F f) {
            lazy[1] = f == id() ? f : composition(f, lazy[1]);
        }

        T prod(int l, int r) {
            if (l == r) return e();
            l += size;
            r += size;
            upper_propagate(l, r);
            T lres = e(), rres = e();
            while (l < r) {
                if (l & 1) lres = op(lres, data[l++]);
                if (r & 1) rres = op(data[r^1], rres);
                l >>= 1;
                r >>= 1;
            }
            return op(lres, rres);
        }

        T all_prod() const {
            return data[1];
        }

        T get(int k) {
            k += size;
            for (int i = log; i > 0; --i) {
                propagate(k >> i);
            }
            return data[k];
        }

        void set(int k, T v) {
            k += size;
            for (int i = log; i > 0; --i) {
                propagate(k >> i);
            }
            data[k] = v;
            for (int i = 1; i <= log; ++i) {
                update(k >> i);
            }
        }

        template<typename Func>
        int max_right(int l, Func f) {
            assert(0 <= l <= n);
            if (l == size) return n;
            l += size;
            for (int i = log; i > 0; --i) {
                propagate(l>>i);
            }
            T s = e();
            while (true) {
                while (l & 1 == 0) {
                    l >>= 1;
                }
                if (!f(op(s, data[l]))) {
                    while (l < size) {
                        propagate(l);
                        l <<= 1;
                        if (f(op(s, data[l]))) {
                            s = op(s, data[l]);
                            l |= 1;
                        }
                    }
                    return l - size;
                }
                s = op(s, data[l]);
                l++;
                if (l & (-l) == l) break;
            }
            return n;
        }

        template<typename Func>
        int min_left(int r, Func f) {
            assert(0 <= r <= n);
            if (r == 0) return 0;
            r += size;
            for (int i = log; i > 0; --i) {
                propagate((r - 1) >> i);
            }
            T s = e();
            while (true) {
                r -= 1;
                while ((r > 1) && (r & 1)) {
                    r >>= 1;
                }
                if (!f(op(data[r], s))) {
                    while (r < size) {
                        propagate(r);
                        r = r << 1 | 1;
                        if (f(op(data[r], s))) {
                            s = op(data[r], s);
                            r ^= 1;
                        }
                    }
                    return r + 1 - size;
                }
                s = op(data[r], s);
                if (r & (-r) == r) break;
            }
            return 0;
        }

        vector<T> tovector() {
            for (int i = 1; i < size; ++i) {
                propagate(i);
            }
            vector<T> res(data.begin()+size, data.begin()+size+n);
            return res;
        }
    };
} // namespace titan23
