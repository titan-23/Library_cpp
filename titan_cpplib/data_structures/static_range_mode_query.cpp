#include <vector>
#include <cassert>
#include <algorithm>
#include "titan_cpplib/algorithm/zaatsu.cpp"
using namespace std;

namespace titan23 {

// 静的な列に対する区間最頻値クエリ
// <構築 O(n√n), 空間 O(n), クエリ O(√n)>
// 参考: https://noshi91.hatenablog.com/entry/2020/10/26/140105
template<typename T>
class StaticRangeModeQuery {
private:
    int n, size;
    vector<int> a;
    int bucket_cnt;
    vector<vector<int>> idx;
    vector<int> inv_idx;
    titan23::Zaatsu<T> z;
    vector<vector<int>> data;
    vector<vector<pair<int, int>>> bucket_data;

    void _calc_all_blocks() {
        vector<int> freqs(z.len(), 0);
        for (int i = 0; i < bucket_cnt; ++i) {
            int freq = -1, val = -1;
            for (int j = i+1; j <= bucket_cnt; ++j) {
                for (auto &x : data[j-1]) {
                    freqs[x]++;
                    if (freqs[x] > freq) {
                        freq = freqs[x];
                        val = x;
                    }
                }
                bucket_data[i][j] = {freq, val};
            }
            for (int j = i+1; j <= bucket_cnt; ++j) {
                for (auto &x : data[j-1]) {
                    freqs[x] = 0;
                }
            }
        }
    }

    void _calc_index() {
        for (int i = 0; i < a.size(); ++i) {
            inv_idx[i] = idx[a[i]].size();
            idx[a[i]].emplace_back(i);
        }
    }

public:
    StaticRangeModeQuery() {}
    StaticRangeModeQuery(vector<T> A) {
        n = A.size();
        z = titan23::Zaatsu<T>(A);
        a.resize(A.size());
        for (int i = 0; i < n; ++i) {
            a[i] = z.to_zaatsu(A[i]);
        }
        size = (int)sqrt(n) + 1;
        bucket_cnt = (n + size - 1) / size;
        data.resize(bucket_cnt);
        for (int i = 0; i < bucket_cnt; ++i) {
            data[i] = vector<int>(a.begin()+i*size, a.begin()+min(n, (i+1)*size));
        }

        bucket_data.resize(bucket_cnt+1);
        for (int i = 0; i <= bucket_cnt; ++i) {
            bucket_data[i].resize(bucket_cnt+1, {0, -1});
        }
        _calc_all_blocks();

        idx.resize(z.len());
        inv_idx.resize(n, -1);
        _calc_index();
    }

    pair<T, int> mode(int l, int r) {
        // (最頻値, 頻度) のタプル
        assert(0 <= l && l < r && r <= n);
        int L = l, R = r;
        int k1 = l / size;
        int k2 = r / size;
        l -= k1 * size;
        r -= k2 * size;

        int freq = 0, val = -1;

        if (k1 == k2) {
            for (int i = L; i < R; ++i) {
                int x = a[i];
                int k = inv_idx[i];
                int freq_cand = freq + 1;
                while (k+freq_cand-1 < idx[x].size() && idx[x][k + freq_cand - 1] < R) {
                    freq = freq_cand;
                    val = x;
                    freq_cand++;
                }
            }
        } else {
            tie(freq, val) = bucket_data[k1 + 1][k2];

            // left
            for (int i = l; i < data[k1].size(); ++i) {
                int x = data[k1][i];
                int k = inv_idx[k1*size+i];
                int freq_cand = freq + 1;
                while (k+freq_cand-1 < idx[x].size() && idx[x][k+freq_cand-1] < R) {
                    freq = freq_cand;
                    val = x;
                    freq_cand++;
                }
            }

            // right
            for (int i = 0; i < r; ++i) {
                int x = data[k2][i];
                int k = inv_idx[k2 * size + i];
                int freq_cand = freq + 1;
                while (0 <= k-(freq_cand-1) && L <= idx[x][k-(freq_cand-1)]) {
                    freq = freq_cand;
                    val = x;
                    freq_cand++;
                }
            }
        }

        return {z.to_origin(val), freq};
    }
};
} // namespace titan23
