#pragma once
#include <memory>
#include <Eigen/Dense>

#include "simcore/interfaces.hpp"
#include "simcore/util.hpp"

namespace simcore {

class SensorModel : public ISensorModel {
 public:
  SensorModel(const World& world, double noise, uint64_t seed)
      : world_(world), noise_(noise), rng_(seed) {}
  Perception observe(const State& s, int tick) override;
  void set_fault(const std::string& name, int tick) override;

 private:
  const World& world_;
  double noise_;
  Rng rng_;
  Obs last_{};
  int dropout_tick_{-1};
  int dropout_len_{20};
};

// Closed-form pure-pursuit + speed-profile expert on the observation vector.
class ExpertPolicy : public IPolicy {
 public:
  Action infer(const Obs& obs) const override;
};

// 2-layer MLP (8 -> H -> 2), tanh, inference with Eigen.
class MlpPolicy : public IPolicy {
 public:
  explicit MlpPolicy(const json& weights);
  Action infer(const Obs& obs) const override;

 private:
  Eigen::MatrixXd w1_, w2_;
  Eigen::VectorXd b1_, b2_;
  double k_scale_{0.25}, v_scale_{15.0};
};

class AStarRouter : public IRouter {
 public:
  Route plan(const MapGraph& map, const State& start, Vec2 goal) const override;
};

// Curvature -> steer, PID on speed.
class PidController : public IController {
 public:
  PidController(const IVehicleModel& vehicle, const VehicleParams& p, double action_noise,
                uint64_t seed)
      : vehicle_(vehicle), p_(p), noise_(action_noise), rng_(seed) {}
  Command compute(const Action& a, const State& s, int tick) override;

 private:
  const IVehicleModel& vehicle_;
  VehicleParams p_;
  double noise_;
  Rng rng_;
  double prev_steer_{0}, integ_{0}, prev_err_{0};
  bool first_{true};
};

// Replays the commands recorded in the on-road log.
class LogReplayController : public IController {
 public:
  explicit LogReplayController(const std::vector<LogEntry>& log) : log_(log) {}
  Command compute(const Action&, const State&, int tick) override;

 private:
  const std::vector<LogEntry>& log_;
};

// Kinematic bicycle with optional actuator latency (command FIFO).
class KinematicBicycle : public IVehicleModel {
 public:
  explicit KinematicBicycle(const VehicleParams& p) : p_(p) {}
  void reset(const State& s) override;
  const State& state() const override { return s_; }
  void step(const Command& c) override;
  double steer_from_curvature(double kappa, double) const override;

 private:
  VehicleParams p_;
  State s_;
  std::vector<Command> fifo_;  // oldest first
};

// Differential-drive robot; `steer` is yaw rate.
class DiffDrive : public IVehicleModel {
 public:
  explicit DiffDrive(const VehicleParams& p) : p_(p) {}
  void reset(const State& s) override { s_ = s; }
  const State& state() const override { return s_; }
  void step(const Command& c) override;
  double steer_from_curvature(double kappa, double v) const override;

 private:
  VehicleParams p_;
  State s_;
};

// Open-loop: the vehicle state is taken from the on-road log.
class LogReplayVehicle : public IVehicleModel {
 public:
  explicit LogReplayVehicle(const std::vector<LogEntry>& log) : log_(log) {}
  void reset(const State& s) override { s_ = s; idx_ = 0; }
  const State& state() const override { return s_; }
  void step(const Command&) override;
  double steer_from_curvature(double kappa, double) const override { return kappa; }

 private:
  const std::vector<LogEntry>& log_;
  State s_;
  size_t idx_{0};
};

}  // namespace simcore
