#include <vector>
#include <string>
using namespace std;

namespace titan23 {
// z[i]:= sとs[i:]の最長共通接頭辞の長さ
vector<int> z_algorithm(const string &s) {
    int n = s.size();
    vector<int> z(n, 0);
    if (n == 0) return z;
    z[0] = n;
    int i = 1, j = 0;
    while (i < n) {
        while (i+j < n && s[j] == s[i+j]) j++;
        z[i] = j;
        if (j == 0) {
            i++;
            continue;
        }
        int k = 1;
        while (k < j && k+z[k] < j) {
            z[i+k] = z[k];
            k++;
        }
        i += k;
        j -= k;
    }
    return z;
}
} // namespace titan23
