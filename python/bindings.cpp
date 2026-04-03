// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 David Liptak

#include <pybind11/pybind11.h>
#include <pybind11/numpy.h>
#include <pybind11/stl.h>
#include "../HopfieldNetwork.h"

namespace py = pybind11;
using FloatArray = py::array_t<float, py::array::c_style | py::array::forcecast>;

PYBIND11_MODULE(_core, m)
{
    m.doc() = "HypercubeHopfield: modern Hopfield associative memory on hypercube graphs";

    py::enum_<UpdateMode>(m, "UpdateMode")
        .value("Async", UpdateMode::Async,
               "Sequential random-order updates. Guaranteed energy descent.")
        .value("Sync", UpdateMode::Sync,
               "Simultaneous double-buffered updates. Deterministic, GPU-portable.");

    py::class_<IHopfieldNetwork>(m, "_HopfieldNetwork")
        // ── Construction ──
        .def(py::init([](size_t dim, uint64_t seed, size_t reach,
                         float beta, float neighbor_fraction, float tolerance) {
            return CreateHopfieldNetwork(dim, seed, reach, beta,
                                         neighbor_fraction, tolerance);
        }),
            py::arg("dim"),
            py::arg("seed")              = 0ULL,
            py::arg("reach")             = 0ULL,
            py::arg("beta")              = 4.0f,
            py::arg("neighbor_fraction") = 1.0f,
            py::arg("tolerance")         = 1e-6f)

        // ── Pattern storage ──
        .def("store_pattern", [](IHopfieldNetwork& self, FloatArray pattern) {
            auto buf = pattern.request();
            if (buf.ndim != 1)
                throw std::invalid_argument(
                    "pattern must be 1D, got " + std::to_string(buf.ndim) + "D");
            size_t n = static_cast<size_t>(buf.size);
            if (n != self.NumVertices())
                throw std::invalid_argument(
                    "pattern size (" + std::to_string(n) + ") != num_vertices ("
                    + std::to_string(self.NumVertices()) + ")");
            self.StorePattern({static_cast<const float*>(buf.ptr), n});
        }, py::arg("pattern"),
           "Store a pattern (explicit storage, not Hebbian).")

        // ── Recall ──
        .def("recall", [](IHopfieldNetwork& self, FloatArray cue,
                          size_t max_steps, UpdateMode mode) {
            auto buf = cue.request();
            if (buf.ndim != 1)
                throw std::invalid_argument(
                    "cue must be 1D, got " + std::to_string(buf.ndim) + "D");
            size_t n = static_cast<size_t>(buf.size);
            if (n != self.NumVertices())
                throw std::invalid_argument(
                    "cue size (" + std::to_string(n) + ") != num_vertices ("
                    + std::to_string(self.NumVertices()) + ")");
            // Copy the input so we don't mutate the caller's array
            py::array_t<float> result(n);
            float* dst = result.mutable_data();
            const float* src = static_cast<const float*>(buf.ptr);
            std::copy(src, src + n, dst);

            RecallResult r;
            {
                py::gil_scoped_release release;
                r = self.Recall({dst, n}, max_steps, mode);
            }
            return py::make_tuple(result, r.steps, r.converged);
        }, py::arg("cue"), py::arg("max_steps") = 100,
           py::arg("mode") = UpdateMode::Sync,
           "Recall from a noisy cue. Returns (recalled_state, steps, converged).")

        // ── Energy ──
        // No GIL release: energy() reads the user's buffer directly (no copy),
        // so releasing the GIL would allow another thread to mutate the array.
        .def("energy", [](const IHopfieldNetwork& self, FloatArray state) {
            auto buf = state.request();
            if (buf.ndim != 1)
                throw std::invalid_argument(
                    "state must be 1D, got " + std::to_string(buf.ndim) + "D");
            size_t n = static_cast<size_t>(buf.size);
            if (n != self.NumVertices())
                throw std::invalid_argument(
                    "state size (" + std::to_string(n) + ") != num_vertices ("
                    + std::to_string(self.NumVertices()) + ")");
            auto e = self.Energy({static_cast<const float*>(buf.ptr), n});
            if (!e.has_value())
                throw py::value_error("no patterns stored");
            return e.value();
        }, py::arg("state"),
           "Compute modern Hopfield energy for the given state.")

        // ── Pattern management ──
        .def("get_pattern", [](const IHopfieldNetwork& self, size_t idx) {
            auto span = self.GetPattern(idx);
            py::array_t<float> arr(span.size());
            std::copy(span.begin(), span.end(), arr.mutable_data());
            return arr;
        }, py::arg("idx"),
           "Read back a stored pattern by index.")

        .def("pop_pattern", &IHopfieldNetwork::PopPattern,
             "Remove the most recently stored pattern.")

        .def("clear", &IHopfieldNetwork::Clear,
             "Remove all stored patterns and reset state.")

        // ── Read-only properties ──
        .def_property_readonly("dim", &IHopfieldNetwork::Dim)
        .def_property_readonly("num_vertices", &IHopfieldNetwork::NumVertices)
        .def_property_readonly("num_patterns", &IHopfieldNetwork::NumPatterns)
        .def_property_readonly("seed", &IHopfieldNetwork::Seed)
        .def_property_readonly("reach", &IHopfieldNetwork::Reach)
        .def_property_readonly("neighbor_fraction", &IHopfieldNetwork::NeighborFraction)
        .def_property_readonly("beta", &IHopfieldNetwork::Beta)
        .def_property_readonly("tolerance", &IHopfieldNetwork::Tolerance)
        ;
}
