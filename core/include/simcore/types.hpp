#pragma once
#include <array>
#include <cmath>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include <nlohmann/json.hpp>

namespace simcore {

using json = nlohmann::json;

inline constexpr double kDt = 0.05;  // fixed timestep, seconds
inline constexpr double kPi = 3.14159265358979323846;
inline constexpr int kObsDim = 8;

inline double wrap_angle(double a) {
  while (a > kPi) a -= 2 * kPi;
  while (a <= -kPi) a += 2 * kPi;
  return a;
}

struct Vec2 {
  double x{0}, y{0};
};
struct State {
  double x{0}, y{0}, yaw{0}, v{0};
};
struct Command {
  double steer{0}, accel{0};
};
struct Action {
  double curvature{0}, speed{0};
};
using Obs = std::array<double, kObsDim>;

struct MapGraph {
  std::vector<Vec2> nodes;                 // node id == index
  std::vector<std::pair<int, int>> edges;  // directed
};

// Actor moving along the ego route: arc-length s0 + speed * t, offset laterally.
struct Actor {
  int id{0};
  double s0{0}, speed{0}, lateral{0};
};

struct LogEntry {
  State state;  // state at the start of the tick
  Command cmd;  // command issued at that tick
};

struct Scenario {
  std::string id;
  uint64_t seed{0};
  MapGraph map;
  State ego_start;
  Vec2 goal;
  double speed_limit{10};
  std::vector<Actor> actors;
  std::vector<LogEntry> log;
};

Scenario scenario_from_json(const json& j);

}  // namespace simcore
