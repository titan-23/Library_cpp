#include <vector>
using namespace std;

// Zaatsu
namespace titan23 {

  template<typename T>
  struct Zaatsu {
   private:
    vector<T> _to_origin;
    int _n;

   public:
    Zaatsu(vector<T> &used_items) {
      _to_origin = used_items;
      sort(_to_origin.begin(), _to_origin.end());
      _to_origin.erase(unique(_to_origin.begin(), _to_origin.end()), _to_origin.end());
      _n = (int)_to_origin.size();
    }

    int len() const {
      return _n;
    }

    int to_zaatsu(const T &x) const {
      return lower_bound(_to_origin.begin(), _to_origin.end(), x) - _to_origin.begin();
    }

    T to_origin(const int &x) const {
      return _to_origin[x];
    }
  };
}  // namespace titan23
