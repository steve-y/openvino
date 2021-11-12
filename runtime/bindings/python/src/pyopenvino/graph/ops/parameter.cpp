// Copyright (C) 2018-2021 Intel Corporation
// SPDX-License-Identifier: Apache-2.0
//

#include "openvino/op/parameter.hpp"

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include <string>

#include "openvino/core/node.hpp"
#include "openvino/core/partial_shape.hpp"  // ov::PartialShape
#include "pyopenvino/graph/ops/parameter.hpp"

namespace py = pybind11;

void regclass_graph_op_Parameter(py::module m) {
    py::class_<ov::op::v0::Parameter, std::shared_ptr<ov::op::v0::Parameter>, ov::Node> parameter(m, "Parameter");
    parameter.doc() = "openvino.impl.op.Parameter wraps ov::op::v0::Parameter";
    parameter.def("__repr__", [](const ov::Node& self) {
        std::string class_name = py::cast(self).get_type().attr("__name__").cast<std::string>();
        std::string shape = py::cast(self.get_output_partial_shape(0)).attr("__str__")().cast<std::string>();
        std::string type = self.get_element_type().c_type_string();
        return "<" + class_name + ": '" + self.get_friendly_name() + "' (" + shape + ", " + type + ")>";
    });

    parameter.def(py::init<const ov::element::Type&, const ov::Shape&>());
    parameter.def(py::init<const ov::element::Type&, const ov::PartialShape&>());
    //    parameter.def_property_readonly("description", &ov::op::v0::Parameter::description);

    parameter.def(
        "get_partial_shape",
        (const ov::PartialShape& (ov::op::v0::Parameter::*)() const) & ov::op::v0::Parameter::get_partial_shape);
    parameter.def("get_partial_shape",
                  (ov::PartialShape & (ov::op::v0::Parameter::*)()) & ov::op::v0::Parameter::get_partial_shape);
    parameter.def("set_partial_shape", &ov::op::v0::Parameter::set_partial_shape);
}
