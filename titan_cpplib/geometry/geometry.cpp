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
bool almostEqual(Real a, Real b) { return abs(a - b) < EPS; }
bool lessThan(Real a, Real b) { return a < b && !almostEqual(a, b); }
bool greaterThan(Real a, Real b) { return a > b && !almostEqual(a, b); }
bool lessThanOrEqual(Real a, Real b) { return a < b || almostEqual(a, b); }
bool greaterThanOrEqual(Real a, Real b) { return a > b || almostEqual(a, b); }
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
  /// @brief p1 と p2 を端点とするベクトルとの外積を返す
  Real cross(const Point& p1, const Point& p2) const { return (p1.x - x) * (p2.y - y) - (p1.y - y) * (p2.x - x); }
  /// @brief ２乗ノルムを返す
  Real norm() const { return x * x + y * y; }
  /// @brief ユークリッドノルムを返す
  Real abs() const { return sqrt(norm()); }
  /// @brief 偏角を返す
  Real arg() const { return atan2(y, x); }
  bool operator==(const Point& p) const { return almostEqual(x, p.x) && almostEqual(y, p.y); }
  friend istream& operator>>(istream& is, Point& p) { return is >> p.x >> p.y; }
};
struct Line {
  Point a, b;
  Line() = default;
  Line(const Point& _a, const Point& _b) : a(_a), b(_b) {}
  /// @brief 直線 Ax+By=C を定義する
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
struct Segment : Line {
  Segment() = default;
  using Line::Line;
};
struct Circle {
  Point center;  ///< 中心
  Real r;  ///< 半径
  Circle() = default;
  Circle(Real x, Real y, Real r) : center(x, y), r(r) {}
  Circle(Point _center, Real r) : center(_center), r(r) {}
  bool operator==(const Circle& C) const { return center == C.center && r == C.r; }
  friend istream& operator>>(istream& is, Circle& C) { return is >> C.center >> C.r; }
};
/// @brief 3点の進行方向
enum Orientation {
  COUNTER_CLOCKWISE,  ///< 反時計回り
  CLOCKWISE,  ///< 時計回り
  ONLINE_BACK, ONLINE_FRONT, ON_SEGMENT
};
/// @brief 3点 p0, p1, p2 の進行方向を返す
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
/// @brief ベクトル p の直線 p1, p2 への正射影ベクトルを返す
Point projection(const Point& p1, const Point& p2, const Point& p) {
  Point base = p2 - p1;
  Real r = (p - p1).dot(base) / base.norm();
  return p1 + base * r;
}
/// @brief ベクトル p の直線 l への正射影ベクトルを返す
Point projection(const Line& l, const Point& p) {
  Point base = l.b - l.a;
  Real r = (p - l.a).dot(base) / base.norm();
  return l.a + base * r;
}
/// @brief ベクトル p の直線 p1, p2 に対する鏡像ベクトルを返す
Point reflection(const Point& p1, const Point& p2, const Point& p) {
  Point proj = projection(p1, p2, p);
  return proj * 2 - p;
}
/// @brief ベクトル p の直線 l に対する鏡像ベクトルを返す
Point reflection(const Line& l, const Point& p) {
  Point proj = projection(l, p);
  return proj * 2 - p;
}
/// @brief 直線の平行判定
bool isParallel(const Line& l1, const Line& l2) { return almostEqual((l1.b - l1.a).cross(l2.b - l2.a), 0); }
/// @brief 直線の直交判定
bool isOrthogonal(const Line& l1, const Line& l2) { return almostEqual((l1.b - l1.a).dot(l2.b - l2.a), 0); }
/// @brief 線分の平行判定
bool isParallel(const Segment& l1, const Segment& l2) { return almostEqual((l1.b - l1.a).cross(l2.b - l2.a), 0); }
/// @brief 線分の直交判定
bool isOrthogonal(const Segment& l1, const Segment& l2) { return almostEqual((l1.b - l1.a).dot(l2.b - l2.a), 0); }
/// @brief 直線と線分の平行判定
bool isParallel(const Line& l1, const Segment& l2) { return almostEqual((l1.b - l1.a).cross(l2.b - l2.a), 0); }
/// @brief 直線と線分の直交判定
bool isOrthogonal(const Line& l1, const Segment& l2) { return almostEqual((l1.b - l1.a).dot(l2.b - l2.a), 0); }
/// @brief 線分と直線の平行判定
bool isParallel(const Segment& l1, const Line& l2) { return almostEqual((l1.b - l1.a).cross(l2.b - l2.a), 0); }
/// @brief 線分と直線の直交判定
bool isOrthogonal(const Segment& l1, const Line& l2) { return almostEqual((l1.b - l1.a).dot(l2.b - l2.a), 0); }
/// @brief 点が直線上にあるか判定
bool isPointOnLine(const Point& p, const Line& l) { return almostEqual((l.b - l.a).cross(p - l.a), 0.0); }
/// @brief 点が線分上にあるか判定
bool isPointOnSegment(const Point& p, const Segment& s) {
  return lessThanOrEqual(min(s.a.x, s.b.x), p.x) &&
        lessThanOrEqual(p.x, max(s.a.x, s.b.x)) &&
        lessThanOrEqual(min(s.a.y, s.b.y), p.y) &&
        lessThanOrEqual(p.y, max(s.a.y, s.b.y)) &&
        almostEqual((s.b - s.a).cross(p - s.a), 0.0);
}
/// @brief 直線の交差判定
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
/// @brief 線分の交点を返す
Point getIntersection(const Segment& s1, const Segment& s2) {
  assert(isIntersecting(s1, s2));
  auto cross = [](Point p, Point q) { return p.x * q.y - p.y * q.x; };
  Point base = s2.b - s2.a;
  Real d1 = abs(cross(base, s1.a - s2.a));
  Real d2 = abs(cross(base, s1.b - s2.a));
  Real t = d1 / (d1 + d2);
  return s1.a + (s1.b - s1.a) * t;
}
/// @brief 点と線分の距離を返す
Real distancePointToSegment(const Point& p, const Segment& s) {
  Point proj = projection(s.a, s.b, p);
  if (isPointOnSegment(proj, s)) return (p - proj).abs();
  else return min((p - s.a).abs(), (p - s.b).abs());
}
/// @brief 線分と線分の距離を返す
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
/// @brief 多角形が凸か判定
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
// 凸多角形に対する点の包含関係 / 2: pを含む, 1: pが辺上, 0: それ以外
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
/// @brief 点が凸多角形の辺上に存在するか判定
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
/// @brief 点が多角形の内部に存在するか判定（辺上は含まない）
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
/// @brief 凸包を求める
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
/// @brief 凸包の直径を求める
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
/// @brief 凸包を直線で切断して左側を返す
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
/// @brief 最近点対の距離を求める
/// @note points は x 座標でソートされている必要がある
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
/// @brief 線分の交差数を数える
int countIntersections(vector<Segment> segments) {
  struct Event {
    Real x;
    int type;  // 0:horizontal start,1:vertical,2:horizontal end
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
      // Horizontal segment
      Real y = seg.a.y;
      Real x1 = min(seg.a.x, seg.b.x);
      Real x2 = max(seg.a.x, seg.b.x);
      events.emplace_back(x1, 0, y, y);
      events.emplace_back(x2, 2, y, y);
    } else {
      // Vertical segment
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
/// @brief 2つの円の交点の個数を返す
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
/// @brief 内接円を求める
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
/// @brief 外接円を求める
Circle getCircumCircle(const Point& A, const Point& B, const Point& C) {
  Real D = 2 * (A.x * (B.y - C.y) + B.x * (C.y - A.y) + C.x * (A.y - B.y));
  Real Ux = ((A.x * A.x + A.y * A.y) * (B.y - C.y) + (B.x * B.x + B.y * B.y) * (C.y - A.y) + (C.x * C.x + C.y * C.y) * (A.y - B.y)) / D;
  Real Uy = ((A.x * A.x + A.y * A.y) * (C.x - B.x) + (B.x * B.x + B.y * B.y) * (A.x - C.x) + (C.x * C.x + C.y * C.y) * (B.x - A.x)) / D;
  Point center(Ux, Uy);
  Real radius = (center - A).abs();
  return Circle{center, radius};
}
/// @brief 円と直線の交点を求める
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
/// @brief 2つの円の交点を求める
vector<Point> getCirclesIntersect(const Circle& c1, const Circle& c2) {
  Real x1 = c1.center.x, y1 = c1.center.y, r1 = c1.r;
  Real x2 = c2.center.x, y2 = c2.center.y, r2 = c2.r;
  Real d = sqrt((x2 - x1) * (x2 - x1) + (y2 - y1) * (y2 - y1));
  if (d > r1 + r2 || d < abs(r1 - r2)) return {};  // No intersection
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
/// @brief 点から引ける円の接線の接点を求める
vector<Point> getTangentLinesFromPoint(const Circle& c, const Point& p) {
  Real cx = c.center.x, cy = c.center.y, r = c.r;
  Real px = p.x, py = p.y;
  Real dx = px - cx;
  Real dy = py - cy;
  Real d = (p - c.center).abs();
  if (lessThan(d, r))
    return {};  // No tangents if the point is inside the circle
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
/// @brief 2つの円の共通接線を求める
vector<Segment> getCommonTangentsLine(const Circle& c1, const Circle& c2) {
  Real x1 = c1.center.x, y1 = c1.center.y, r1 = c1.r;
  Real x2 = c2.center.x, y2 = c2.center.y, r2 = c2.r;
  Real dx = x2 - x1;
  Real dy = y2 - y1;
  Real d = sqrt(dx * dx + dy * dy);
  vector<Segment> tangents;
  // Coincident circles(infinite tangents)
  if (almostEqual(d, 0) && almostEqual(r1, r2)) return tangents;
  // External tangents
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
  // Internal tangents
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
