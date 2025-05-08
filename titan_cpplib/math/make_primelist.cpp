#include <vector>
using namespace std;

const int MAX = 31624;  // 必要な最大数の平方根
int is_prime[MAX];
vector<int> primelist;

void make_primelist() {
    for (int i = 2; i < MAX; ++i) {
        is_prime[i] = 1;
    }
    for (int i = 2; i*i <= MAX; ++i) {
        if (!is_prime[i]) continue;
        for (int j = i+i; j <= MAX; j += i) {
            is_prime[j] = 0;
        }
    }
    for (int i = 2; i < MAX; ++i) {
        if (is_prime[i]) {
            primelist.emplace_back(i);
        }
    }
    return;
}

vector<int> factorization(int n) {
    vector<int> res;
    for (const int &p : primelist) {
        if (p * p > n) break;
        if (n % p == 0) {
            n /= p;
            while (n % p == 0) {
                n /= p;
            }
            res.emplace_back(p);
        }
    }
    if (n != 1) {
        res.emplace_back(n);
    }
    return res;
}
