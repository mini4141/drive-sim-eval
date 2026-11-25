find_package(pybind11 CONFIG REQUIRED)
pybind11_add_module(simcore module.cpp)
target_link_libraries(simcore PRIVATE simcore_core)
install(TARGETS simcore LIBRARY DESTINATION .)
