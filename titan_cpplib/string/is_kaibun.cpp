/// https://github.com/titan-23/Library_cpp/blob/main/titan_cpplib/string/is_kaibun.cpp
#pragma once

#include <string>
using namespace std;

bool is_kaibun(const string &s) {
    for (int i = 0; i < s.size()/2; ++i) {
        if (s[i] != s[(int)s.size()-i-1]) return false;
    }
    return true;
}
