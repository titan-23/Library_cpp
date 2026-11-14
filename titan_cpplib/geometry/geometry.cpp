/// https://github.com/titan-23/Library_cpp/blob/main/titan_cpplib/geometry/geometry.cpp
#pragma once

#include <iostream>
#include <vector>
#include <set>
#include <string>
#include <utility>
#include <algorithm>
#include <cmath>
#include <climits>
#include <cassert>
using namespace std;

using Real = long double;
const Real EPS = 1e-9;
// 以下の比較関数は、差が EPS 未満の 2 値を等しいとみなす
bool almostEqual(Real a, Real b) { return abs(a - b) < EPS; }
bool lessThan(Real a, Real b) { return a < b && !almostEqual(a, b); }
bool greaterThan(Real a, Real b) { return a > b && !almostEqual(a, b); }
bool lessThanOrEqual(Real a, Real b) { return a < b || almostEqual(a, b); }
bool greaterThanOrEqual(Real a, Real b) { return a > b || almostEqual(a, b); }
/// @brief 2 次元の点またはベクトル
struct Point {
  Real x, y;
  Point() = default;
  Point(Real x, Real y) : x(x), y(y) {}
  Point operator+(const Point& p) const { return Point(x + p.x, y + p.y); }
  Point operator-(const Point& p) const { return Point(x - p.x, y - p.y); }
  Point operator*(Real k) const { return Point(x * k, y * k); }
  Point operator/(Real k) const { return Point(x / k, y / k); }
  /// @brief p との内積を返す
  Real dot(const Point& p) const { return x * p.x + y * p.y; }
  /// @brief p との外積を返す
  Real cross(const Point& p) const { return x * p.y - y * p.x; }
  /// @brief 自身から p1 へのベクトルと、自身から p2 へのベクトルの外積を返す
  Real cross(const Point& p1, const Point& p2) const { return (p1.x - x) * (p2.y - y) - (p1.y - y) * (p2.x - x); }
  /// @brief 2 乗ノルムを返す
  Real norm() const { return x * x + y * y; }
  /// @brief ユークリッドノルムを返す
  Real abs() const { return sqrt(norm()); }
  /// @brief 偏角を返す
  Real arg() const { return atan2(y, x); }
  bool operator==(const Point& p) const { return almostEqual(x, p.x) && almostEqual(y, p.y); }
  friend istream& operator>>(istream& is, Point& p) { return is >> p.x >> p.y; }
};
/// @brief 2 点 a, b を通る直線
struct Line {
  Point a, b;
  Line() = default;
  Line(const Point& _a, const Point& _b) : a(_a), b(_b) {}
  /// @brief 直線 Ax + By = C を構築する / A と B の両方が 0 ではいけない
  Line(const Real& A, const Real& B, const Real& C) {
    if (almostEqual(A, 0)) {
      assert(!almostEqual(B, 0));
      a = Point(0, C / B);
      b = Point(1, C / B);
    } else if (almostEqual(B, 0)) {
      a = Point(C / A, 0);
      b = Point(C / A, 1);
    } else if (almostEqual(C, 0)) {
      a = Point(0, C / B);
      b = Point(1, (C - A) / B);
    } else {
      a = Point(0, C / B);
      b = Point(C / A, 0);
    }
  }
  bool operator==(const Line& l) const { return a == l.a && b == l.b; }
  friend istream& operator>>(istream& is, Line& l) { return is >> l.a >> l.b; }
};
/// @brief 2 点 a, b を端点とする線分
struct Segment : Line {
  Segment() = default;
  using Line::Line;
};
/// @brief 円
struct Circle {
  Point center;  ///< 中心
  Real r;  ///< 半径
  Circle() = default;
  Circle(Real x, Real y, Real r) : center(x, y), r(r) {}
  Circle(Point _center, Real r) : center(_center), r(r) {}
  bool operator==(const Circle& C) const { return center == C.center && r == C.r; }
  friend istream& operator>>(istream& is, Circle& C) { return is >> C.center >> C.r; }
};
/// @brief 3 点の位置関係
enum Orientation {
  COUNTER_CLOCKWISE,  ///< 反時計回り
  CLOCKWISE,  ///< 時計回り
  ONLINE_BACK, ONLINE_FRONT, ON_SEGMENT
};
/// @brief 3 点 p0, p1, p2 の位置関係を返す
/// 一直線上では、p2 が p0 の後ろなら ONLINE_BACK、p1 の先なら ONLINE_FRONT、線分 p0p1 上なら ON_SEGMENT
Orientation ccw(const Point& p0, const Point& p1, const Point& p2) {
  Point a = p1 - p0;
  Point b = p2 - p0;
  Real cross_product = a.cross(b);
  if (greaterThan(cross_product, 0)) return COUNTER_CLOCKWISE;
  if (lessThan(cross_product, 0)) return CLOCKWISE;
  if (lessThan(a.dot(b), 0)) return ONLINE_BACK;
  if (lessThan(a.norm(), b.norm())) return ONLINE_FRONT;
  return ON_SEGMENT;
}
/// @brief Orientation を文字列に変換する
string orientationToString(Orientation o) {
  switch (o) {
    case COUNTER_CLOCKWISE: return "COUNTER_CLOCKWISE";
    case CLOCKWISE: return "CLOCKWISE";
    case ONLINE_BACK: return "ONLINE_BACK";
    case ONLINE_FRONT: return "ONLINE_FRONT";
    case ON_SEGMENT: return "ON_SEGMENT";
    default: return "UNKNOWN";
  }
}
/// @brief 点 p を直線 p1p2 へ正射影した点を返す
Point projection(const Point& p1, const Point& p2, const Point& p) {
  Point base = p2 - p1;
  Real r = (p - p1).dot(base) / base.norm();
  return p1 + base * r;
}
/// @brief 点 p を直線 l へ正射影した点を返す
Point projection(const Line& l, const Point& p) {
  Point base = l.b - l.a;
  Real r = (p - l.a).dot(base) / base.norm();
  return l.a + base * r;
}
/// @brief 直線 p1p2 に関する点 p の鏡像を返す
Point reflection(const Point& p1, const Point& p2, const Point& p) {
  Point proj = projection(p1, p2, p);
  return proj * 2 - p;
}
/// @brief 直線 l に関する点 p の鏡像を返す
Point reflection(const Line& l, const Point& p) {
  Point proj = projection(l, p);
  return proj * 2 - p;
}
/// @brief 2 直線が平行か判定する
bool isParallel(const Line& l1, const Line& l2) { return almostEqual((l1.b - l1.a).cross(l2.b - l2.a), 0); }
/// @brief 2 直線が直交するか判定する
bool isOrthogonal(const Line& l1, const Line& l2) { return almostEqual((l1.b - l1.a).dot(l2.b - l2.a), 0); }
/// @brief 2 線分が平行か判定する
bool isParallel(const Segment& l1, const Segment& l2) { return almostEqual((l1.b - l1.a).cross(l2.b - l2.a), 0); }
/// @brief 2 線分が直交するか判定する
bool isOrthogonal(const Segment& l1, const Segment& l2) { return almostEqual((l1.b - l1.a).dot(l2.b - l2.a), 0); }
/// @brief 直線と線分が平行か判定する
bool isParallel(const Line& l1, const Segment& l2) { return almostEqual((l1.b - l1.a).cross(l2.b - l2.a), 0); }
/// @brief 直線と線分が直交するか判定する
bool isOrthogonal(const Line& l1, const Segment& l2) { return almostEqual((l1.b - l1.a).dot(l2.b - l2.a), 0); }
/// @brief 線分と直線が平行か判定する
bool isParallel(const Segment& l1, const Line& l2) { return almostEqual((l1.b - l1.a).cross(l2.b - l2.a), 0); }
/// @brief 線分と直線が直交するか判定する
bool isOrthogonal(const Segment& l1, const Line& l2) { return almostEqual((l1.b - l1.a).dot(l2.b - l2.a), 0); }
/// @brief 点 p が直線 l 上にあるか判定する
bool isPointOnLine(const Point& p, const Line& l) { return almostEqual((l.b - l.a).cross(p - l.a), 0.0); }
/// @brief 点 p が線分 s 上にあるか判定する
bool isPointOnSegment(const Point& p, const Segment& s) {
  return lessThanOrEqual(min(s.a.x, s.b.x), p.x) &&
        lessThanOrEqual(p.x, max(s.a.x, s.b.x)) &&
        lessThanOrEqual(min(s.a.y, s.b.y), p.y) &&
        lessThanOrEqual(p.y, max(s.a.y, s.b.y)) &&
        almostEqual((s.b - s.a).cross(p - s.a), 0.0);
}
/// @brief 2 線分が交差するか判定する / 端点での接触も交差とみなす
bool isIntersecting(const Segment& s1, const Segment& s2) {
  Point p0p1 = s1.b - s1.a, p0p2 = s2.a - s1.a, p0p3 = s2.b - s1.a, p2p3 = s2.b - s2.a, p2p0 = s1.a - s2.a, p2p1 = s1.b - s2.a;
  Real d1 = p0p1.cross(p0p2), d2 = p0p1.cross(p0p3), d3 = p2p3.cross(p2p0), d4 = p2p3.cross(p2p1);
  if (lessThan(d1 * d2, 0) && lessThan(d3 * d4, 0)) return true;
  if (almostEqual(d1, 0.0) && isPointOnSegment(s2.a, s1)) return true;
  if (almostEqual(d2, 0.0) && isPointOnSegment(s2.b, s1)) return true;
  if (almostEqual(d3, 0.0) && isPointOnSegment(s1.a, s2)) return true;
  if (almostEqual(d4, 0.0) && isPointOnSegment(s1.b, s2)) return true;
  return false;
}
/// @brief 2 線分の交点を返す / 2 線分は平行でなく、交差している必要がある
Point getIntersection(const Segment& s1, const Segment& s2) {
  assert(isIntersecting(s1, s2));
  auto cross = [](Point p, Point q) { return p.x * q.y - p.y * q.x; };
  Point base = s2.b - s2.a;
  Real d1 = abs(cross(base, s1.a - s2.a));
  Real d2 = abs(cross(base, s1.b - s2.a));
  Real t = d1 / (d1 + d2);
  return s1.a + (s1.b - s1.a) * t;
}
/// @brief 点 p と線分 s の距離を返す
Real distancePointToSegment(const Point& p, const Segment& s) {
  Point proj = projection(s.a, s.b, p);
  if (isPointOnSegment(proj, s)) return (p - proj).abs();
  else return min((p - s.a).abs(), (p - s.b).abs());
}
/// @brief 2 線分の距離を返す
Real distanceSegmentToSegment(const Segment& s1, const Segment& s2) {
  if (isIntersecting(s1, s2)) return 0.0;
  return min({distancePointToSegment(s1.a, s2),
        distancePointToSegment(s1.b, s2),
        distancePointToSegment(s2.a, s1),
        distancePointToSegment(s2.b, s1)});
}
/// @brief 多角形の面積を返す
Real getPolygonArea(const vector<Point>& points) {
  int n = points.size();
  Real area = 0.0;
  rep(i, n) {
    int j = (i + 1) % n;
    area += points[i].x * points[j].y;
    area -= points[i].y * points[j].x;
  }
  return abs(area) / 2.0;
}
/// @brief 多角形が凸か判定する
bool isConvex(const vector<Point>& points) {
  int n = points.size();
  bool has_positive = false, has_negative = false;
  rep(i, n) {
    int j = (i + 1) % n;
    int k = (i + 2) % n;
    Point a = points[j] - points[i];
    Point b = points[k] - points[j];
    Real cross_product = a.cross(b);
    if (greaterThan(cross_product, 0)) has_positive = true;
    if (lessThan(cross_product, 0)) has_negative = true;
  }
  return !(has_positive && has_negative);
}
/// @brief 凸多角形 hull と点 p の包含関係を返す / 2: p を内部に含む, 1: p が辺上, 0: それ以外 / O(log n)
/// hull の頂点は反時計回りに並べておく必要がある
int convexPolygonContainsPoint(const vector<Point>& hull, const Point& p) {
  int n = hull.size();
  if (n == 0) return 0;
  if (n == 1) {
    if (almostEqual(hull[0].x, p.x) && almostEqual(hull[0].y, p.y)) return 1;
    return 0;
  }
  if (n == 2) {
    Segment s(hull[0], hull[1]);
    if (isPointOnSegment(p, s)) return 1;
    return 0;
  }
  Real b1 = (hull[1]-hull[0]).cross(p-hull[0]);
  Real b2 = (hull.back()-hull[0]).cross(p-hull[0]);
  if (lessThan(b1, 0) || greaterThan(b2, 0)) return 0;
  int l = 1, r = n-1;
  while (r - l > 1) {
    int mid = (l + r) / 2;
    Real c = (p-hull[0]).cross(hull[mid]-hull[0]);
    (greaterThanOrEqual(c, 0) ? r : l) = mid;
  }
  Real v = (hull[l]-p).cross(hull[r]-p);
  if (almostEqual(v, 0)) return 1;
  if (greaterThan(v, 0)) return (almostEqual(b1, 0) || almostEqual(b2, 0) ? 1 : 2);
  return 0;
}
/// @brief 点 p が多角形の辺上にあるか判定する
bool isPointOnPolygon(const vector<Point>& polygon, const Point& p) {
  int n = polygon.size();
  rep(i, n) {
    Point a = polygon[i];
    Point b = polygon[(i + 1) % n];
    Segment s(a, b);
    if (isPointOnSegment(p, s)) return true;
  }
  return false;
}
/// @brief 点 p が多角形の内部にあるか判定する
/// 辺上の点では辺の向きによって結果が変わるので、先に isPointOnPolygon で調べる
bool isPointInsidePolygon(const vector<Point>& polygon, const Point& p) {
  int n = polygon.size();
  bool inPolygon = false;
  rep(i, n) {
    Point a = polygon[i];
    Point b = polygon[(i + 1) % n];
    if (greaterThan(a.y, b.y)) swap(a, b);
    if (lessThanOrEqual(a.y, p.y) && lessThan(p.y, b.y) && greaterThan((b - a).cross(p - a), 0)) inPolygon = !inPolygon;
  }
  return inPolygon;
}
/// @brief 凸包を反時計回りで返す / include_collinear が true なら辺上の点も残す / points は並べ替えられる
vector<Point> convexHull(vector<Point>& points, bool include_collinear = false) {
  int n = points.size();
  if (n <= 1) return points;
  sort(points.begin(), points.end(), [](const Point& l, const Point& r) -> bool {
    if (almostEqual(l.y, r.y)) return lessThan(l.x, r.x);
    return lessThan(l.y, r.y);
  });
  if (n == 2) return points;
  vector<Point> upper = {points[0], points[1]}, lower = {points[0], points[1]};
  for (int i = 2; i < n; i++) {
    while (upper.size() >= 2 && ccw(upper.end()[-2], upper.end()[-1], points[i]) != CLOCKWISE) {
      if (ccw(upper.end()[-2], upper.end()[-1], points[i]) == ONLINE_FRONT && include_collinear) break;
      upper.pop_back();
    }
    upper.push_back(points[i]);
    while (lower.size() >= 2 && ccw(lower.end()[-2], lower.end()[-1], points[i]) != COUNTER_CLOCKWISE) {
      if (ccw(lower.end()[-2], lower.end()[-1], points[i]) == ONLINE_FRONT && include_collinear) break;
      lower.pop_back();
    }
    lower.push_back(points[i]);
  }
  reverse(upper.begin(), upper.end());
  upper.pop_back();
  lower.pop_back();
  lower.insert(lower.end(), upper.begin(), upper.end());
  return lower;
}
/// @brief 凸包の直径を返す
Real convexHullDiameter(const vector<Point>& hull) {
  int n = hull.size();
  if (n == 1) return 0;
  int k = 1;
  Real max_dist = 0;
  rep(i, n) {
    while (true) {
      int j = (k + 1) % n;
      Point dist1 = hull[i] - hull[j], dist2 = hull[i] - hull[k];
      max_dist = max(max_dist, dist1.abs());
      max_dist = max(max_dist, dist2.abs());
      if (dist1.abs() > dist2.abs()) k = j;
      else break;
    }
    Point dist = hull[i] - hull[k];
    max_dist = max(max_dist, dist.abs());
  }
  return max_dist;
}
/// @brief 凸包 g を直線 l で切断し、l の左側を返す
vector<Point> cutPolygon(const vector<Point>& g, const Line& l) {
  auto isLeft = [](const Point& p1, const Point& p2, const Point& p) -> bool { return (p2 - p1).cross(p - p1) > 0; };
  vector<Point> newPolygon;
  int n = g.size();
  rep(i, n) {
    const Point& cur = g[i];
    const Point& next = g[(i + 1) % n];
    if (isLeft(l.a, l.b, cur)) newPolygon.push_back(cur);
    if ((isLeft(l.a, l.b, cur) && !isLeft(l.a, l.b, next)) || (!isLeft(l.a, l.b, cur) && isLeft(l.a, l.b, next))) {
      Real A1 = (next - cur).cross(l.a - cur);
      Real A2 = (next - cur).cross(l.b - cur);
      Point intersection = l.a + (l.b - l.a) * (A1 / (A1 - A2));
      newPolygon.push_back(intersection);
    }
  }
  return newPolygon;
}
/// @brief points[l, r) の最近点対の距離を返す
/// points は x 座標の昇順に並べておく必要がある / 処理後の points[l, r) は y 座標の昇順になる
Real closestPair(vector<Point>& points, int l, int r) {
  if (r - l <= 1) return numeric_limits<Real>::max();
  int mid = (l + r) >> 1;
  Real x = points[mid].x;
  Real d = min(closestPair(points, l, mid), closestPair(points, mid, r));
  auto iti = points.begin(), itl = iti + l, itm = iti + mid, itr = iti + r;
  inplace_merge(itl, itm, itr, [](const Point& lhs, const Point& rhs) -> bool {
    return lessThan(lhs.y, rhs.y);
  });
  vector<Point> nearLine;
  for (int i = l; i < r; i++) {
    if (greaterThanOrEqual(fabs(points[i].x - x), d)) continue;
    int sz = nearLine.size();
    for (int j = sz - 1; j >= 0; j--) {
      Point dv = points[i] - nearLine[j];
      if (dv.y >= d) break;
      d = min(d, dv.abs());
    }
    nearLine.push_back(points[i]);
  }
  return d;
}
/// @brief 水平な線分と垂直な線分の交差数を数える / 各線分は軸に平行である必要がある
/// 同じ y 座標の水平な線分どうしが重なると正しく数えられない
int countIntersections(vector<Segment> segments) {
  struct Event {
    Real x;
    int type;  // 0: 水平な線分の始点, 1: 垂直な線分, 2: 水平な線分の終点
    Real y1, y2;
    Event(Real x, int type, Real y1, Real y2) : x(x), type(type), y1(y1), y2(y2) {}
    bool operator<(const Event& other) const {
      if (x == other.x) return type < other.type;
      return x < other.x;
    }
  };
  vector<Event> events;
  sort(segments.begin(), segments.end(), [](const Segment& lhs, const Segment& rhs) -> bool {
    return lessThan(min(lhs.a.x, lhs.b.x), min(rhs.a.x, rhs.b.x));
  });
  for (const auto& seg : segments) {
    if (seg.a.y == seg.b.y) {
      // 水平な線分
      Real y = seg.a.y;
      Real x1 = min(seg.a.x, seg.b.x);
      Real x2 = max(seg.a.x, seg.b.x);
      events.emplace_back(x1, 0, y, y);
      events.emplace_back(x2, 2, y, y);
    } else {
      // 垂直な線分
      Real x = seg.a.x;
      Real y1 = min(seg.a.y, seg.b.y);
      Real y2 = max(seg.a.y, seg.b.y);
      events.emplace_back(x, 1, y1, y2);
    }
  }
  sort(events.begin(), events.end());
  set<Real> activeSegments;
  int intersectionCount = 0;
  for (const auto& event : events) {
    if (event.type == 0) {
      activeSegments.insert(event.y1);
    } else if (event.type == 2) {
      activeSegments.erase(event.y1);
    } else if (event.type == 1) {
      auto lower = activeSegments.lower_bound(event.y1);
      auto upper = activeSegments.upper_bound(event.y2);
      intersectionCount += distance(lower, upper);
    }
  }
  return intersectionCount;
}
/// @brief 2 円の共通接線の本数を返す / 4: 離れている, 3: 外接, 2: 2 点で交わる, 1: 内接, 0: 一方が他方を含む
int countCirclesIntersection(const Circle& c1, const Circle& c2) {
  Real d =
    sqrt((c1.center.x - c2.center.x) * (c1.center.x - c2.center.x) +
        (c1.center.y - c2.center.y) * (c1.center.y - c2.center.y));
  Real r1 = c1.r, r2 = c2.r;
  if (greaterThan(d, r1 + r2)) return 4;
  else if (almostEqual(d, r1 + r2)) return 3;
  else if (greaterThan(d, fabs(r1 - r2))) return 2;
  else if (almostEqual(d, fabs(r1 - r2))) return 1;
  else return 0;
}
/// @brief 三角形 ABC の内接円を返す
Circle getInCircle(const Point& A, const Point& B, const Point& C) {
  Real a = (B - C).abs();
  Real b = (A - C).abs();
  Real c = (A - B).abs();
  Real s = (a + b + c) / 2;
  Real area = sqrt(s * (s - a) * (s - b) * (s - c));
  Real r = area / s;
  Real cx = (a * A.x + b * B.x + c * C.x) / (a + b + c);
  Real cy = (a * A.y + b * B.y + c * C.y) / (a + b + c);
  return Circle{Point(cx, cy), r};
}
/// @brief 三角形 ABC の外接円を返す
Circle getCircumCircle(const Point& A, const Point& B, const Point& C) {
  Real D = 2 * (A.x * (B.y - C.y) + B.x * (C.y - A.y) + C.x * (A.y - B.y));
  Real Ux = ((A.x * A.x + A.y * A.y) * (B.y - C.y) + (B.x * B.x + B.y * B.y) * (C.y - A.y) + (C.x * C.x + C.y * C.y) * (A.y - B.y)) / D;
  Real Uy = ((A.x * A.x + A.y * A.y) * (C.x - B.x) + (B.x * B.x + B.y * B.y) * (A.x - C.x) + (C.x * C.x + C.y * C.y) * (B.x - A.x)) / D;
  Point center(Ux, Uy);
  Real radius = (center - A).abs();
  return Circle{center, radius};
}
/// @brief 円 c と直線 p1p2 の交点を返す / 接する場合は同じ点を 2 つ返す / (x, y) の昇順
/// 円と直線は共有点を持つ必要がある
vector<Point> getCircleLineIntersection(const Circle& c, Point p1, Point p2) {
  Real cx = c.center.x, cy = c.center.y, r = c.r;
  Real dx = p2.x - p1.x;
  Real dy = p2.y - p1.y;
  Real a = dx * dx + dy * dy;
  Real b = 2 * (dx * (p1.x - cx) + dy * (p1.y - cy));
  Real c_const = (p1.x - cx) * (p1.x - cx) + (p1.y - cy) * (p1.y - cy) - r * r;
  Real discriminant = b * b - 4 * a * c_const;
  vector<Point> res;
  if (almostEqual(discriminant, 0)) {
    Real t = -b / (2 * a);
    Real ix = p1.x + t * dx;
    Real iy = p1.y + t * dy;
    res.emplace_back(ix, iy);
    res.emplace_back(ix, iy);
  } else if (discriminant > 0) {
    Real sqrt_discriminant = sqrt(discriminant);
    Real t1 = (-b + sqrt_discriminant) / (2 * a);
    Real t2 = (-b - sqrt_discriminant) / (2 * a);
    Real ix1 = p1.x + t1 * dx;
    Real iy1 = p1.y + t1 * dy;
    Real ix2 = p1.x + t2 * dx;
    Real iy2 = p1.y + t2 * dy;
    res.emplace_back(ix1, iy1);
    res.emplace_back(ix2, iy2);
  }
  if (almostEqual(res[0].x, res[1].x)) {
    if (greaterThan(res[0].y, res[1].y)) swap(res[0], res[1]);
  } else if (greaterThan(res[0].x, res[1].x)) {
    swap(res[0], res[1]);
  }
  return res;
}
/// @brief 2 円の交点を返す / 交わらなければ空 / (x, y) の昇順
vector<Point> getCirclesIntersect(const Circle& c1, const Circle& c2) {
  Real x1 = c1.center.x, y1 = c1.center.y, r1 = c1.r;
  Real x2 = c2.center.x, y2 = c2.center.y, r2 = c2.r;
  Real d = sqrt((x2 - x1) * (x2 - x1) + (y2 - y1) * (y2 - y1));
  if (d > r1 + r2 || d < abs(r1 - r2)) return {};  // 交点なし
  Real a = (r1 * r1 - r2 * r2 + d * d) / (2 * d);
  Real h = sqrt(r1 * r1 - a * a);
  Real x0 = x1 + a * (x2 - x1) / d;
  Real y0 = y1 + a * (y2 - y1) / d;
  Real rx = -(y2 - y1) * (h / d);
  Real ry = (x2 - x1) * (h / d);
  Point p1(x0 + rx, y0 + ry);
  Point p2(x0 - rx, y0 - ry);
  vector<Point> res;
  res.push_back(p1);
  res.push_back(p2);
  if (almostEqual(res[0].x, res[1].x)) {
    if (greaterThan(res[0].y, res[1].y)) swap(res[0], res[1]);
  } else if (greaterThan(res[0].x, res[1].x)) {
    swap(res[0], res[1]);
  }
  return res;
}
/// @brief 点 p から円 c へ引いた接線の接点を返す / p が円の内部なら空、円周上なら p のみ / (x, y) の昇順
vector<Point> getTangentLinesFromPoint(const Circle& c, const Point& p) {
  Real cx = c.center.x, cy = c.center.y, r = c.r;
  Real px = p.x, py = p.y;
  Real dx = px - cx;
  Real dy = py - cy;
  Real d = (p - c.center).abs();
  if (lessThan(d, r))
    return {};  // 点が円の内部なら接線はない
  else if (almostEqual(d, r))
    return {p};
  Real a = r * r / d;
  Real h = sqrt(r * r - a * a);
  Real cx1 = cx + a * dx / d;
  Real cy1 = cy + a * dy / d;
  vector<Point> res;
  res.emplace_back(cx1 + h * dy / d, cy1 - h * dx / d);
  res.emplace_back(cx1 - h * dy / d, cy1 + h * dx / d);
  if (almostEqual(res[0].x, res[1].x)) {
    if (greaterThan(res[0].y, res[1].y)) swap(res[0], res[1]);
  } else if (greaterThan(res[0].x, res[1].x)) {
    swap(res[0], res[1]);
  }
  return res;
}
/// @brief 2 円の共通接線を求める / 各線分の a は c1 上の接点 / a の (x, y) の昇順
/// b は c2 上の接点にならないので使わない
vector<Segment> getCommonTangentsLine(const Circle& c1, const Circle& c2) {
  Real x1 = c1.center.x, y1 = c1.center.y, r1 = c1.r;
  Real x2 = c2.center.x, y2 = c2.center.y, r2 = c2.r;
  Real dx = x2 - x1;
  Real dy = y2 - y1;
  Real d = sqrt(dx * dx + dy * dy);
  vector<Segment> tangents;
  // 2 円が一致すると共通接線は無数にある
  if (almostEqual(d, 0) && almostEqual(r1, r2)) return tangents;
  // 共通内接線。d >= r1 + r2 のとき存在する
  if (greaterThanOrEqual(d, r1 + r2)) {
    Real a = atan2(dy, dx);
    for (int sign : {-1, 1}) {
      Real theta = acos((r1 + r2) / d);
      Real cx1 = x1 + r1 * cos(a + sign * theta);
      Real cy1 = y1 + r1 * sin(a + sign * theta);
      Real cx2 = x2 + r2 * cos(a + sign * theta);
      Real cy2 = y2 + r2 * sin(a + sign * theta);
      tangents.emplace_back(Segment{Point(cx1, cy1), Point(cx2, cy2)});
      if (almostEqual(d, r1 + r2)) break;
    }
  }
  // 共通外接線。d >= |r1 - r2| のとき存在する
  if (greaterThanOrEqual(d, fabs(r1 - r2))) {
    Real a = atan2(dy, dx);
    for (int sign : {-1, 1}) {
      Real theta = acos((r1 - r2) / d);
      Real cx1 = x1 + r1 * cos(a + sign * theta);
      Real cy1 = y1 + r1 * sin(a + sign * theta);
      Real cx2 = x2 - r2 * cos(a + sign * theta);
      Real cy2 = y2 - r2 * sin(a + sign * theta);
      tangents.emplace_back(Segment{Point(cx1, cy1), Point(cx2, cy2)});
      if (almostEqual(d, fabs(r1 - r2))) break;
    }
  }
  sort(tangents.begin(), tangents.end(), [&](const Segment& s1, const Segment& s2) {
    if (almostEqual(s1.a.x, s2.a.x))
      return lessThan(s1.a.y, s2.a.y);
    else
      return lessThan(s1.a.x, s2.a.x);
  });
  return tangents;
}
