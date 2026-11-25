#include <optional>
#include <string>

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include "simcore/engine.hpp"
#include "simcore/util.hpp"

namespace py = pybind11;

PYBIND11_MODULE(simcore, m) {
  m.doc() = "Drive Sim Eval simulation core (C++20) exposed through pybind11";
  m.attr("__version__") = "0.1.0";
  m.def("sha256", &simcore::sha256_hex, py::arg("data"));

  py::class_<simcore::Engine>(m, "Engine")
      .def(py::init<const std::string&>(), py::arg("config_json"))
      .def("set_fault", &simcore::Engine::set_fault, py::arg("name"), py::arg("tick"))
      .def(
          "run",
          [](const simcore::Engine& e, const std::string& scenario_json, std::optional<uint64_t> seed) {
            std::string out;
            {
              py::gil_scoped_release release;
              out = e.run_str(scenario_json, seed).dump();
            }
            return py::module_::import("json").attr("loads")(out);
          },
          py::arg("scenario_json"), py::arg("seed") = py::none());
}
