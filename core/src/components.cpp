#include "simcore/components.hpp"

#include <algorithm>
#include <set>
#include <stdexcept>

namespace simcore {

namespace {
double clampd(double v, double lo, double hi) { return std::max(lo, std::min(hi, v)); }
constexpr double kLookaheadCurv = 10.0;  // m, window for the curvature-ahead feature
constexpr double kActorRange = 40.0;     // m
}  // namespace

// ---------------------------------------------------------------- scenario / params

Scenario scenario_from_json(const json& j) {
  Scenario sc;
  sc.id = j.at("id").get<std::string>();
  sc.seed = j.value("seed", uint64_t{0});
  for (const auto& n : j.at("map").at("nodes")) {
    size_t id = n.at("id").get<size_t>();
    if (id != sc.map.nodes.size()) throw std::runtime_error("map node ids must be 0..N-1 in order");
    sc.map.nodes.push_back({n.at("x").get<double>(), n.at("y").get<double>()});
  }
  for (const auto& e : j.at("map").at("edges"))
    sc.map.edges.emplace_back(e.at("from").get<int>(), e.at("to").get<int>());
  const auto& es = j.at("ego_start");
  sc.ego_start = {es.at("x"), es.at("y"), es.at("yaw"), es.value("v", 0.0)};
  sc.goal = {j.at("goal").at("x"), j.at("goal").at("y")};
  sc.speed_limit = j.value("speed_limit", 10.0);
  for (const auto& a : j.value("actors", json::array()))
    sc.actors.push_back({a.at("id"), a.at("s0"), a.at("speed"), a.value("lateral", 0.0)});
  for (const auto& l : j.value("onroad_log", json::array()))
    sc.log.push_back({{l.at("x"), l.at("y"), l.at("yaw"), l.at("v")}, {l.at("steer"), l.at("accel")}});
  return sc;
}

VehicleParams VehicleParams::from_json(const json& j) {
  VehicleParams p;
  p.name = j.value("name", p.name);
  p.type = j.value("type", p.type);
  p.wheelbase = j.value("wheelbase", p.wheelbase);
  p.max_steer = j.value("max_steer", p.max_steer);
  p.max_steer_rate = j.value("max_steer_rate", p.max_steer_rate);
  p.max_accel = j.value("max_accel", p.max_accel);
  p.max_brake = j.value("max_brake", p.max_brake);
  p.max_speed = j.value("max_speed", p.max_speed);
  p.latency_steps = j.value("latency_steps", p.latency_steps);
  const json pid = j.value("pid", json::object());
  p.kp = pid.value("kp", p.kp);
  p.ki = pid.value("ki", p.ki);
  p.kd = pid.value("kd", p.kd);
  p.i_max = pid.value("i_max", p.i_max);
  return p;
}

// ---------------------------------------------------------------- route / world

Route Route::from_points(std::vector<Vec2> pts) {
  if (pts.size() < 2) throw std::runtime_error("route needs at least 2 points");
  Route r;
  r.pts = std::move(pts);
  r.cum.assign(r.pts.size(), 0.0);
  for (size_t i = 0; i + 1 < r.pts.size(); ++i) {
    double dx = r.pts[i + 1].x - r.pts[i].x, dy = r.pts[i + 1].y - r.pts[i].y;
    r.cum[i + 1] = r.cum[i] + std::hypot(dx, dy);
    r.head.push_back(std::atan2(dy, dx));
  }
  return r;
}

Route::Proj Route::project(Vec2 p) const {
  double best = 1e18;
  Proj out;
  for (size_t i = 0; i + 1 < pts.size(); ++i) {
    double dx = pts[i + 1].x - pts[i].x, dy = pts[i + 1].y - pts[i].y;
    double len2 = dx * dx + dy * dy;
    double t = len2 > 0 ? clampd(((p.x - pts[i].x) * dx + (p.y - pts[i].y) * dy) / len2, 0.0, 1.0) : 0.0;
    double cx = pts[i].x + t * dx, cy = pts[i].y + t * dy;
    double d2 = (p.x - cx) * (p.x - cx) + (p.y - cy) * (p.y - cy);
    if (d2 < best) {
      best = d2;
      double cross = dx * (p.y - cy) - dy * (p.x - cx);
      out.s = cum[i] + t * std::sqrt(len2);
      out.lat = cross >= 0 ? std::sqrt(d2) : -std::sqrt(d2);
    }
  }
  return out;
}

double Route::heading_at(double s) const {
  if (s <= 0) return head.front();
  if (s >= length()) return head.back();
  size_t i = static_cast<size_t>(std::upper_bound(cum.begin(), cum.end(), s) - cum.begin()) - 1;
  return head[std::min(i, head.size() - 1)];
}

Vec2 Route::point_at(double s) const {
  if (s <= 0) return {pts.front().x + std::cos(head.front()) * s, pts.front().y + std::sin(head.front()) * s};
  if (s >= length()) {
    double e = s - length();
    return {pts.back().x + std::cos(head.back()) * e, pts.back().y + std::sin(head.back()) * e};
  }
  size_t i = static_cast<size_t>(std::upper_bound(cum.begin(), cum.end(), s) - cum.begin()) - 1;
  i = std::min(i, head.size() - 1);
  double seg = cum[i + 1] - cum[i];
  double t = seg > 0 ? (s - cum[i]) / seg : 0.0;
  return {pts[i].x + t * (pts[i + 1].x - pts[i].x), pts[i].y + t * (pts[i + 1].y - pts[i].y)};
}

Vec2 World::actor_pos(const Actor& a, int tick) const {
  double s = a.s0 + a.speed * tick * kDt;
  Vec2 p = route.point_at(s);
  double h = route.heading_at(s);
  return {p.x - std::sin(h) * a.lateral, p.y + std::cos(h) * a.lateral};
}

double World::actor_heading(const Actor& a, int tick) const {
  return route.heading_at(a.s0 + a.speed * tick * kDt);
}

// ---------------------------------------------------------------- sensor

Perception SensorModel::observe(const State& st, int tick) {
  Perception out;
  Route::Proj pr = world_.route.project({st.x, st.y});
  double hdg = world_.route.heading_at(pr.s);
  double he = wrap_angle(st.yaw - hdg);
  double s_far = std::min(pr.s + kLookaheadCurv, world_.route.length());
  double curv = wrap_angle(world_.route.heading_at(s_far) - hdg) / kLookaheadCurv;

  double best_d = kActorRange, best_rv = 0.0;
  double cy = std::cos(st.yaw), sy = std::sin(st.yaw);
  for (const Actor& a : world_.actors) {
    Vec2 p = world_.actor_pos(a, tick);
    double dx = p.x - st.x, dy = p.y - st.y;
    double fwd = dx * cy + dy * sy, left = -dx * sy + dy * cy;
    double d = std::hypot(dx, dy);
    if (fwd > 0 && std::abs(left) < 1.6 && d < best_d) {
      best_d = d;
      best_rv = st.v - a.speed * std::cos(world_.actor_heading(a, tick) - st.yaw);
    }
  }
  double goal_dist = world_.route.length() - pr.s;

  Obs o = {clampd(pr.lat / 2.0, -3, 3),
           he / 0.5,
           curv / 0.1,
           best_d / kActorRange,
           best_rv / 10.0,
           st.v / 15.0,
           world_.speed_limit / 15.0,
           std::min(goal_dist, 200.0) / 200.0};
  for (double& v : o) v += noise_ * rng_.normal();  // always drawn: stable RNG stream

  bool dropout = dropout_tick_ >= 0 && tick >= dropout_tick_ && tick < dropout_tick_ + dropout_len_;
  if (dropout) {
    o = last_;  // stale data: the sensor stopped delivering
  } else {
    last_ = o;
  }
  out.obs = o;
  out.lat = pr.lat;
  out.s = pr.s;
  out.goal_dist = goal_dist;
  return out;
}

void SensorModel::set_fault(const std::string& name, int tick) {
  if (name != "sensor_dropout") throw std::runtime_error("unknown fault: " + name);
  dropout_tick_ = tick;
}

// ---------------------------------------------------------------- policies

Action ExpertPolicy::infer(const Obs& o) const {
  double lat = o[0] * 2.0, he = o[1] * 0.5, ka = o[2] * 0.1, d = o[3] * kActorRange;
  double rv = o[4] * 10.0, v = o[5] * 15.0, lim = o[6] * 15.0, gd = o[7] * 200.0;

  // pure pursuit towards the point Ld ahead on the route (arc approximation)
  double ld = clampd(0.6 * v + 3.0, 3.0, 10.0);
  double dx = ld, dy = ka * ld * ld / 2.0 - lat;
  double ye = -std::sin(he) * dx + std::cos(he) * dy;
  double kappa = clampd(2.0 * ye / (dx * dx + dy * dy), -0.25, 0.25);

  double v_curve = std::sqrt(2.5 / std::max(std::abs(ka), 1e-3));
  double v_goal = 0.6 * std::max(gd - 0.5, 0.0);  // linear ramp: easy for the MLP to learn
  double target = std::min({lim, v_curve, v_goal});
  if (d < kActorRange - 1e-9) {
    double v_lead = v - rv;
    target = std::min(target, std::max(0.0, v_lead + 0.5 * (d - 10.0)));
  }
  return {kappa, std::max(0.0, target)};
}

MlpPolicy::MlpPolicy(const json& w) {
  auto load = [](const json& m) {
    Eigen::MatrixXd out(m.size(), m.at(0).size());
    for (size_t r = 0; r < m.size(); ++r)
      for (size_t c = 0; c < m[r].size(); ++c) out(r, c) = m[r][c].get<double>();
    return out;
  };
  auto vec = [](const json& v) {
    Eigen::VectorXd out(v.size());
    for (size_t i = 0; i < v.size(); ++i) out(i) = v[i].get<double>();
    return out;
  };
  const auto& l = w.at("layers");
  w1_ = load(l.at(0).at("W"));
  b1_ = vec(l.at(0).at("b"));
  w2_ = load(l.at(1).at("W"));
  b2_ = vec(l.at(1).at("b"));
  if (w1_.cols() != kObsDim || w2_.rows() != 2) throw std::runtime_error("MLP must be 8 -> H -> 2");
  const auto& sc = w.value("out_scale", json::array({0.25, 15.0}));
  k_scale_ = sc.at(0);
  v_scale_ = sc.at(1);
}

Action MlpPolicy::infer(const Obs& obs) const {
  Eigen::Map<const Eigen::Matrix<double, kObsDim, 1>> x(obs.data());
  Eigen::VectorXd h = (w1_ * x + b1_).array().tanh();
  Eigen::VectorXd y = w2_ * h + b2_;
  return {clampd(y(0) * k_scale_, -0.25, 0.25), std::max(0.0, y(1) * v_scale_)};
}

// ---------------------------------------------------------------- router

Route AStarRouter::plan(const MapGraph& map, const State& start, Vec2 goal) const {
  auto nearest = [&](double x, double y) {
    int best = 0;
    double bd = 1e18;
    for (size_t i = 0; i < map.nodes.size(); ++i) {
      double d = std::hypot(map.nodes[i].x - x, map.nodes[i].y - y);
      if (d < bd) { bd = d; best = static_cast<int>(i); }
    }
    return best;
  };
  const int n = static_cast<int>(map.nodes.size());
  int src = nearest(start.x, start.y), dst = nearest(goal.x, goal.y);
  std::vector<std::vector<int>> adj(n);
  for (const auto& e : map.edges) adj[e.first].push_back(e.second);
  auto dist = [&](int a, int b) {
    return std::hypot(map.nodes[a].x - map.nodes[b].x, map.nodes[a].y - map.nodes[b].y);
  };
  std::vector<double> g(n, 1e18);
  std::vector<int> prev(n, -1);
  std::set<std::pair<double, int>> open;  // ordered: deterministic tie-breaks
  g[src] = 0;
  open.insert({dist(src, dst), src});
  while (!open.empty()) {
    int u = open.begin()->second;
    open.erase(open.begin());
    if (u == dst) break;
    for (int v : adj[u]) {
      double ng = g[u] + dist(u, v);
      if (ng < g[v]) {
        open.erase({g[v] + dist(v, dst), v});
        g[v] = ng;
        prev[v] = u;
        open.insert({ng + dist(v, dst), v});
      }
    }
  }
  if (src != dst && prev[dst] < 0) throw std::runtime_error("router: no route to goal");
  std::vector<Vec2> pts;
  for (int u = dst; u >= 0; u = prev[u]) pts.push_back(map.nodes[u]);
  std::reverse(pts.begin(), pts.end());
  return Route::from_points(std::move(pts));
}

// ---------------------------------------------------------------- controllers

Command PidController::compute(const Action& a, const State& s, int) {
  double steer = vehicle_.steer_from_curvature(a.curvature, s.v);
  if (noise_ > 0) steer += noise_ * rng_.normal();
  steer = clampd(steer, -p_.max_steer, p_.max_steer);
  if (!first_) {
    double lim = p_.max_steer_rate * kDt;
    steer = clampd(steer, prev_steer_ - lim, prev_steer_ + lim);
  }
  prev_steer_ = steer;

  double target = clampd(a.speed, 0.0, p_.max_speed);
  double err = target - s.v;
  integ_ = clampd(integ_ + err * kDt, -p_.i_max, p_.i_max);
  double deriv = first_ ? 0.0 : (err - prev_err_) / kDt;
  prev_err_ = err;
  first_ = false;
  double acc = clampd(p_.kp * err + p_.ki * integ_ + p_.kd * deriv, -p_.max_brake, p_.max_accel);
  return {steer, acc};
}

Command LogReplayController::compute(const Action&, const State&, int tick) {
  if (tick >= 0 && static_cast<size_t>(tick) < log_.size()) return log_[tick].cmd;
  return {};
}

// ---------------------------------------------------------------- vehicle models

void KinematicBicycle::reset(const State& s) {
  s_ = s;
  fifo_.assign(static_cast<size_t>(p_.latency_steps), Command{});
}

void KinematicBicycle::step(const Command& c) {
  fifo_.push_back(c);
  Command applied = fifo_.front();
  fifo_.erase(fifo_.begin());
  double steer = clampd(applied.steer, -p_.max_steer, p_.max_steer);
  double acc = clampd(applied.accel, -p_.max_brake, p_.max_accel);
  State n = s_;
  n.x += s_.v * std::cos(s_.yaw) * kDt;
  n.y += s_.v * std::sin(s_.yaw) * kDt;
  n.yaw = wrap_angle(s_.yaw + s_.v / p_.wheelbase * std::tan(steer) * kDt);
  n.v = clampd(s_.v + acc * kDt, 0.0, p_.max_speed);
  s_ = n;
}

double KinematicBicycle::steer_from_curvature(double kappa, double) const {
  return std::atan(p_.wheelbase * kappa);
}

void DiffDrive::step(const Command& c) {
  double w = clampd(c.steer, -p_.max_steer, p_.max_steer);
  double acc = clampd(c.accel, -p_.max_brake, p_.max_accel);
  State n = s_;
  n.x += s_.v * std::cos(s_.yaw) * kDt;
  n.y += s_.v * std::sin(s_.yaw) * kDt;
  n.yaw = wrap_angle(s_.yaw + w * kDt);
  n.v = clampd(s_.v + acc * kDt, 0.0, p_.max_speed);
  s_ = n;
}

double DiffDrive::steer_from_curvature(double kappa, double v) const {
  return kappa * std::max(v, 0.3);
}

void LogReplayVehicle::step(const Command&) {
  if (log_.empty()) return;
  idx_ = std::min(idx_ + 1, log_.size() - 1);
  s_ = log_[idx_].state;
}

}  // namespace simcore
