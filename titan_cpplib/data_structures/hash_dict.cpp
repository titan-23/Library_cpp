#include <vector>
#include <random>
#include <iostream>
#include <cassert>

using namespace std;

// HashDict
namespace titan23 {

    template<typename V>
    class HashDict {
      private:
        using u64 = unsigned long long;
        static constexpr const u64 K = 0x517cc1b727220a95;
        static constexpr const int M = 2;
        vector<u64> exist;
        vector<u64> keys;
        vector<V> vals;
        int msk, xor_;
        int size;

        int hash(const u64 &key) const {
            return (((((key>>32)&msk) ^ (key&msk) ^ xor_)) * (HashDict::K & msk)) & msk;
        }

        pair<int, bool> get_pos(const u64 &key) const {
            int h = hash(key);
            while (true) {
                if (!(exist[h>>6]>>(h&63)&1)) return {h, false};
                if (keys[h] == key) return {h, true};
                h = (h + 1) & msk;
            }
        }

        int bit_length(const int x) const {
            if (x == 0) return 0;
            return 32 - __builtin_clz(x);
        }

        void rebuild() {
            vector<u64> old_exist = exist;
            vector<u64> old_keys = keys;
            vector<V> old_vals = vals;
            exist.resize(HashDict::M*old_exist.size()+1);
            fill(exist.begin(), exist.end(), 0);
            keys.resize(HashDict::M*old_keys.size());
            vals.resize(HashDict::M*old_vals.size());
            size = 0;
            msk = (1<<bit_length(keys.size()-1))-1;
            random_device rd;
            mt19937 gen(rd());
            uniform_int_distribution<int> dis(0, msk);
            xor_ = dis(gen);
            for (int i = 0; i < (int)old_keys.size(); ++i) {
                if (old_exist[i>>6]>>(i&63)&1) {
                    set(old_keys[i], old_vals[i]);
                }
            }
        }

      public:
        HashDict() : exist(1, 0), keys(1), vals(1), msk(0), xor_(0), size(0) {}

        HashDict(const int n) {
            int s = 1<<bit_length(n);
            s *= HashDict::M;
            assert(s > 0);
            exist.resize((s>>6)+1, 0);
            keys.resize(s);
            vals.resize(s);
            msk = (1<<bit_length(keys.size()-1))-1;
            random_device rd;
            mt19937 gen(rd());
            uniform_int_distribution<int> dis(0, msk);
            xor_ = dis(gen);
            size = 0;
        }

        V get(const u64 key) const {
            const auto [pos, exist_res] = get_pos(key);
            if (!exist_res) return V();
            else return vals[pos];
        }

        V get(const u64 key, const V missing) const {
            const auto [pos, exist_res] = get_pos(key);
            if (!exist_res) return missing;
            else return vals[pos];
        }

        bool contains(const u64 key) const {
            return get_pos(key).second;
        }

        V operator[] (const u64 key) const {
            return get(key);
        }

        void set(const u64 key, const V val) {
            const auto [pos, is_exist] = get_pos(key);
            vals[pos] = val;
            if (!is_exist) {
                exist[pos>>6] |= 1ull<<(pos&63);
                keys[pos] = key;
                ++size;
                if (HashDict::M*size > keys.size()) {
                    rebuild();
                }
            }
        }

        //! keyがすでにあればtrue, なければ挿入してfalse / `O(1)`
        bool contains_set(const u64 key, const V val) {
            const auto [pos, is_exist] = get_pos(key);
            if (val < vals[pos]) {
                vals[pos] = val;
            } else {
                return false;
            }
            if (!is_exist) {
                exist[pos>>6] |= 1ull<<(pos&63);
                keys[pos] = key;
                ++size;
                if (HashDict::M*size > keys.size()) {
                    rebuild();
                }
                return false;
            }
            return true;
        }

        //! keyがすでにあればtrue, なければ挿入してfalse / `O(1)`
        bool contains_insert(const u64 key) {
            const auto [pos, is_exist] = get_pos(key);
            if (!is_exist) {
                exist[pos>>6] |= 1ull<<(pos&63);
                keys[pos] = key;
                ++size;
                if (HashDict::M*size > keys.size()) {
                    rebuild();
                }
                return false;
            }
            return true;
        }

        //! 全ての要素を削除する / `O(n/w)`
        void clear() {
            this->size = 0;
            fill(exist.begin(), exist.end(), 0);
        }

        int len() const {
            return size;
        }
    };
} // namespaced titan23
