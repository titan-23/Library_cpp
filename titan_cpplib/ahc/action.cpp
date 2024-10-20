#include <iostream>
#include <cassert>
using namespace std;

// Action
enum class Action { U, R, D, L };
ostream& operator<<(ostream& os, const Action &action) {
    switch (action) {
        case Action::U: os << 'U'; break;
        case Action::R: os << 'R'; break;
        case Action::D: os << 'D'; break;
        case Action::L: os << 'L'; break;
        default: assert(false);
    }
    return os;
}

Action get_rev_action(const Action &action) {
    switch (action) {
        case Action::U: return Action::D;
        case Action::D: return Action::U;
        case Action::R: return Action::L;
        case Action::L: return Action::R;
    }
    assert(false);
}
