// Copyright (C) 2018-2021 Intel Corporation
// SPDX-License-Identifier: Apache-2.0
//

#include "cldnn_program.h"
#include "cldnn_common_utils.h"

#include "ngraph/op/convert.hpp"
#include "ngraph/op/convert_like.hpp"

#include "cldnn/primitives/reorder.hpp"

namespace CLDNNPlugin {

static void CreateConvertLikeOp(Program& p, const std::shared_ptr<ngraph::op::v1::ConvertLike>& op) {
    p.ValidateInputs(op, {2});
    auto inputPrimitives = p.GetInputPrimitiveIDs(op);
    std::string layerName = layer_type_name_ID(op);

    auto outDataType = DataTypeFromPrecision(op->get_input_element_type(1));

    auto reorderPrim = cldnn::reorder(layerName,
                                      inputPrimitives[0],
                                      cldnn::format::any,
                                      outDataType,
                                      std::vector<float>(),
                                      cldnn::reorder_mean_mode::subtract,
                                      op->get_friendly_name());
    p.AddPrimitive(reorderPrim);
    p.AddPrimitiveToProfiler(op);
}

static void CreateConvertOp(Program& p, const std::shared_ptr<ngraph::op::v0::Convert>& op) {
    p.ValidateInputs(op, {1});
    auto inputPrimitives = p.GetInputPrimitiveIDs(op);
    std::string layerName = layer_type_name_ID(op);

    auto outDataType = DataTypeFromPrecision(op->get_destination_type());

    auto reorderPrim = cldnn::reorder(layerName,
                                      inputPrimitives[0],
                                      cldnn::format::any,
                                      outDataType,
                                      std::vector<float>(),
                                      cldnn::reorder_mean_mode::subtract,
                                      op->get_friendly_name());

    p.AddPrimitive(reorderPrim);
    p.AddPrimitiveToProfiler(op);
}

REGISTER_FACTORY_IMPL(v0, Convert);
REGISTER_FACTORY_IMPL(v1, ConvertLike);

}  // namespace CLDNNPlugin
