/// https://github.com/titan-23/Library_cpp/blob/main/titan_cpplib/others/io.cpp
#pragma once

#include <bits/stdc++.h>
#include <unistd.h>
using namespace std;

namespace titan23 {

/// @brief 標準入力を 2^20 バイトずつ読み込んで解析する入力 / cin や scanf と併用しない
/// 参考: https://qiita.com/nojima/items/57b9d39d7d73362ac883
class Scanner {
private:
    vector<char> buffer;
    ssize_t n_written;
    ssize_t n_read;

    void do_read() {
        ssize_t r = read(0, &buffer[0], buffer.size());
        if (r < 0) throw runtime_error(strerror(errno));
        n_written = r;
        n_read = 0;
    }

    inline int next_char() {
        ++n_read;
        if (n_read == n_written) { do_read(); }
        return current_char();
    }

    inline int current_char() {
        return (n_read == n_written) ? EOF : buffer[n_read];
    }

public:
    Scanner(): buffer(1<<20) { do_read(); }

    /// @brief 空白を読み飛ばし、64 bit 符号付き整数を 1 つ読む
    int64_t read_int64() {
        int64_t ret = 0, sgn = 1;
        int ch = current_char();
        while (isspace(ch)) { ch = next_char(); }
        if (ch == '-') { sgn = -1; ch = next_char(); }
        for (; isdigit(ch); ch = next_char())
            ret = (ret * 10) + (ch - '0');
        return sgn * ret;
    }

    /// @brief 空白を読み飛ばし、1 文字読む
    char read_char() {
        int ch = current_char();
        while (isspace(ch)) {
            ch = next_char();
        }
        next_char();
        return ch;
    }

    /// @brief 空白を読み飛ばし、次の空白までの文字列を読む
    string read_string() {
        string ret;
        int ch = current_char();
        while (isspace(ch)) {
            ch = next_char();
        }
        while (!isspace(ch) && ch != EOF) {
            ret += ch;
            ch = next_char();
        }
        return ret;
    }
} scanner;
} // namespace titan23

/// @brief titan23::scanner から整数を 1 つ読む
int64_t read_int64() { return titan23::scanner.read_int64(); }
