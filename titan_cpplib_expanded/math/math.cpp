// #include "titan_cpplib/math/math.cpp"
// pow ----------------
long long pow_mod(long long a, long long b, const long long mod) {
  long long res = 1ll;
  while (b) {
    if (b & 1ll) {
      res = res * a % mod;
    }
    a = a * a % mod;
    b >>= 1ll;
  }
  return res;
}

long long pow(long long a, long long b) {
  long long res = 1ll;
  while (b) {
    if (b & 1ll) res *= a;
    a *= a;
    b >>= 1ll;
  }
  return res;
}
// pow ----------------

// factorial -----------------
long long factorial(long long x) {
  long long res = 1;
  for (long long i = 1ll; i <= x; ++i) {
    res *= i;
  }
  return res;
}
// factorial -----------------

// get_primelist -----------------
vector<int> get_primelist(int MAX) {
  vector<int> is_prime(MAX+1, 1);
  is_prime[0] = 0;
  is_prime[1] = 0;
  for (int i = 2; i < sqrt(MAX)+1; ++i) {
    if (!is_prime[i]) continue;
    for (int j = i+i; j < MAX+1; j+=i) {
      is_prime[j] = 0;
    }
  }
  return is_prime;
}
// get_primelist -----------------


bool is_primell(long long n) {
  if (n == 1) return false;
  if (n == 2) return true;
  if (!(n & 1ll)) return false;
  vector<long long> p = {2, 7, 61, 325, 9375, 28178, 450775, 9780504, 1795265022};
  long long d = n - 1;
  long long dd = d & (-d);
  long long cnt = 0;
  while (dd) {
    ++cnt;
    dd >>= 1ll;
  }
  d >>= cnt - 1ll;
  for (long long a : p) {
    if (n <= a) break;
    long long t = d;
    long long y = pow_mod(a, t, n);
    while (t != n-1ll && y != 1ll && y != n-1ll) {
      y = pow_mod(y, 2ll, n);
      t <<= 1ll;
    }
    if (y != n-1ll && (!(t&1ll))) return false;
  }
  return true;
}

