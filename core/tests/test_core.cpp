#include <gtest/gtest.h>

#include "simcore/components.hpp"
#include "simcore/engine.hpp"

using namespace simcore;

namespace {
World straight_world(double len = 100.0) {
  std::vector<Vec2> pts;
  for (double x = 0; x <= len; x += 2.0) pts.push_back({x, 0.0});
  return World{Route::from_points(pts), {}, 10.0};
}

json straight_scenario() {
  json nodes = json::array(), edges = json::array();
  for (int i = 0; i <= 100; ++i) {
    nodes.push_back({{"id", i}, {"x", 2.0 * i}, {"y", 0.0}});
    if (i) edges.push_back({{"from", i - 1}, {"to", i}});
  }
  return {{"id", "t1"},
          {"seed", 7},
          {"map", {{"nodes", nodes}, {"edges", edges}}},
          {"ego_start", {{"x", 0.0}, {"y", 0.5}, {"yaw", 0.0}, {"v", 0.0}}},
          {"goal", {{"x", 200.0}, {"y", 0.0}}},
          {"speed_limit", 10.0},
          {"actors", json::array()}};
}
}  // namespace

TEST(SensorModel, ObservationShapeAndValues) {
  World w = straight_world();
  SensorModel sensor(w, 0.0, 1);
  Perception p = sensor.observe({10.0, 1.0, 0.0, 5.0}, 0);
  EXPECT_EQ(p.obs.size(), 8u);
  EXPECT_NEAR(p.obs[0], 0.5, 1e-9);         // 1 m left of the route, scaled by 1/2
  EXPECT_NEAR(p.obs[5], 5.0 / 15.0, 1e-9);  // speed
  EXPECT_NEAR(p.goal_dist, 90.0, 1e-9);
}

TEST(MlpPolicy, ForwardMatchesKnownVector) {
  json w1 = json::array();
  for (int r = 0; r < 2; ++r) {
    json row = json::array();
    for (int c = 0; c < 8; ++c) row.push_back(r == c ? 1.0 : 0.0);
    w1.push_back(row);
  }
  json w2 = json::parse("[[1,0],[0,1]]");
  json l0 = {{"W", w1}, {"b", json::parse("[0,0]")}};
  json l1 = {{"W", w2}, {"b", json::parse("[0,0]")}};
  json weights = {{"layers", json::array({l0, l1})}, {"out_scale", json::parse("[0.25,15.0]")}};
  MlpPolicy mlp(weights);
  Obs o{0.1, 0.2, 0, 0, 0, 0, 0, 0};
  Action a = mlp.infer(o);
  EXPECT_NEAR(a.curvature, 0.25 * std::tanh(0.1), 1e-12);
  EXPECT_NEAR(a.speed, 15.0 * std::tanh(0.2), 1e-12);
}

TEST(AStarRouter, FindsShortestRoute) {
  MapGraph m;
  m.nodes = {{0, 0}, {5, 0}, {10, 0}, {5, 8}, {10, 8}};
  m.edges = {{0, 1}, {1, 2}, {0, 3}, {3, 4}, {4, 2}};
  Route r = AStarRouter().plan(m, {0, 0, 0, 0}, {10, 0});
  ASSERT_EQ(r.pts.size(), 3u);
  EXPECT_NEAR(r.length(), 10.0, 1e-9);
}

TEST(PidController, AcceleratesTowardsTargetSpeed) {
  VehicleParams p;
  KinematicBicycle car(p);
  car.reset({0, 0, 0, 0});
  PidController pid(car, p, 0.0, 1);
  Command c = pid.compute({0.0, 10.0}, car.state(), 0);
  EXPECT_GT(c.accel, 0.0);
  EXPECT_NEAR(c.steer, 0.0, 1e-12);
}

TEST(KinematicBicycle, IntegratesStraightLine) {
  VehicleParams p;
  KinematicBicycle car(p);
  car.reset({0, 0, 0, 10.0});
  for (int i = 0; i < 20; ++i) car.step({0.0, 0.0});
  EXPECT_NEAR(car.state().x, 10.0, 1e-9);
  EXPECT_NEAR(car.state().y, 0.0, 1e-12);
}

TEST(Engine, DigestIsDeterministic) {
  json cfg = {{"mode", "closed_loop"},
              {"horizon_s", 10.0},
              {"obs_noise", 0.002},
              {"components", {{"policy", "expert"}, {"controller", "pid"}, {"vehicle", "platform"}}},
              {"platform", {{"name", "car"}, {"type", "bicycle"}}}};
  Engine e1(cfg.dump()), e2(cfg.dump());
  json a = e1.run(straight_scenario()), b = e2.run(straight_scenario());
  EXPECT_EQ(a["digest"].get<std::string>().size(), 64u);
  EXPECT_EQ(a["digest"], b["digest"]);
  EXPECT_GT(a["metrics"]["route_completion_pct"].get<double>(), 0.0);
}
