#pragma once
#include <string>

#include "simcore/types.hpp"

namespace simcore {

// Piecewise-linear route with arc-length parametrisation.
struct Route {
  std::vector<Vec2> pts;
  std::vector<double> cum;   // arc length at each point
  std::vector<double> head;  // heading of segment i (pts[i] -> pts[i+1])

  struct Proj {
    double s{0}, lat{0};
  };
  static Route from_points(std::vector<Vec2> pts);
  double length() const { return cum.empty() ? 0.0 : cum.back(); }
  Proj project(Vec2 p) const;  // lat > 0 means left of the route
  double heading_at(double s) const;
  Vec2 point_at(double s) const;  // extrapolates straight beyond both ends
};

struct World {
  Route route;
  std::vector<Actor> actors;
  double speed_limit{10};
  Vec2 actor_pos(const Actor& a, int tick) const;
  double actor_heading(const Actor& a, int tick) const;
};

struct Perception {
  Obs obs{};
  double lat{0}, s{0}, goal_dist{0};  // ground truth, for metrics
};

struct VehicleParams {
  std::string name{"car"};
  std::string type{"bicycle"};
  double wheelbase{2.8};
  double max_steer{0.6};       // rad (bicycle) or rad/s (diff drive)
  double max_steer_rate{0.8};  // rad/s (or rad/s^2)
  double max_accel{3.0};
  double max_brake{6.0};
  double max_speed{30.0};
  int latency_steps{0};
  double kp{1.2}, ki{0.1}, kd{0.05}, i_max{3.0};
  static VehicleParams from_json(const json& j);
};

class ISensorModel {
 public:
  virtual ~ISensorModel() = default;
  virtual Perception observe(const State& s, int tick) = 0;
  virtual void set_fault(const std::string& name, int tick) = 0;
};

class IPolicy {
 public:
  virtual ~IPolicy() = default;
  virtual Action infer(const Obs& obs) const = 0;
};

class IRouter {
 public:
  virtual ~IRouter() = default;
  virtual Route plan(const MapGraph& map, const State& start, Vec2 goal) const = 0;
};

class IController {
 public:
  virtual ~IController() = default;
  virtual Command compute(const Action& a, const State& s, int tick) = 0;
};

class IVehicleModel {
 public:
  virtual ~IVehicleModel() = default;
  virtual void reset(const State& s) = 0;
  virtual const State& state() const = 0;
  virtual void step(const Command& c) = 0;
  virtual double steer_from_curvature(double kappa, double v) const = 0;
};

}  // namespace simcore
