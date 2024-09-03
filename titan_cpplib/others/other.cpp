template<typename T, typename S>
T abs(const T &a, const S &b) { return a < b ? b-a : a-b; }

template<typename T, typename S>
T max(const T &a, const S &b) { return a < b ? b : a; }

template<typename T, typename S>
T min(const T &a, const S &b) { return a < b ? a : b; }
