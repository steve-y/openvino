// Copyright (C) 2018-2021 Intel Corporation
// SPDX-License-Identifier: Apache-2.0
//

#pragma once

#include <list>

#include "ngraph/rt_info.hpp"
#include "openvino/core/layout.hpp"
#include "openvino/core/node.hpp"
#include "openvino/core/partial_shape.hpp"
#include "openvino/core/preprocess/color_format.hpp"
#include "openvino/core/preprocess/postprocess_steps.hpp"
#include "openvino/core/preprocess/preprocess_steps.hpp"
#include "tensor_name_util.hpp"

namespace ov {
namespace preprocess {

inline size_t get_and_check_width_idx(const Layout& layout, const PartialShape& shape) {
    OPENVINO_ASSERT(ov::layout::has_width(layout), "Layout ", layout.to_string(), " doesn't have `width` dimension");
    OPENVINO_ASSERT(shape.rank().is_static(), "Can't get shape width index for shape with dynamic rank");
    auto idx = ov::layout::width_idx(layout);
    if (idx < 0) {
        idx = shape.rank().get_length() + idx;
    }
    OPENVINO_ASSERT(idx >= 0 && shape.rank().get_length() > idx,
                    "Width dimension is out of bounds ",
                    std::to_string(idx));
    return idx;
}

inline size_t get_and_check_height_idx(const Layout& layout, const PartialShape& shape) {
    OPENVINO_ASSERT(ov::layout::has_height(layout), "Layout ", layout.to_string(), " doesn't have `height` dimension");
    OPENVINO_ASSERT(shape.rank().is_static(), "Can't get shape height index for shape with dynamic rank");
    auto idx = ov::layout::height_idx(layout);
    if (idx < 0) {
        idx = shape.rank().get_length() + idx;
    }
    OPENVINO_ASSERT(idx >= 0 && shape.rank().get_length() > idx,
                    "Height dimension is out of bounds ",
                    std::to_string(idx));
    return idx;
}

inline size_t get_and_check_channels_idx(const Layout& layout, const PartialShape& shape) {
    OPENVINO_ASSERT(ov::layout::has_channels(layout),
                    "Layout ",
                    layout.to_string(),
                    " doesn't have `channels` dimension");
    OPENVINO_ASSERT(shape.rank().is_static(), "Can't get shape channels index for shape with dynamic rank");
    auto idx = ov::layout::channels_idx(layout);
    if (idx < 0) {
        idx = shape.rank().get_length() + idx;
    }
    OPENVINO_ASSERT(idx >= 0 && shape.rank().get_length() > idx,
                    "Channels dimension is out of bounds ",
                    std::to_string(idx));
    return idx;
}

/// \brief Context passed to each pre/post-processing operation.
/// This is internal structure which is not shared to custom operations yet.
class PrePostProcessingContextBase {
public:
    explicit PrePostProcessingContextBase(Layout layout) : m_layout(std::move(layout)) {}

    const Layout& layout() const {
        return m_layout;
    }

    Layout& layout() {
        return m_layout;
    }

    // Final layout. Needed if user specified convert_layout without arguments
    // For preprocessing it is parameter's network layout
    // For post-processing it is result's tensor layout
    const Layout& target_layout() const {
        return m_target_layout;
    }

    Layout& target_layout() {
        return m_target_layout;
    }

    element::Type target_element_type() const {
        return m_target_element_type;
    }

    element::Type& target_element_type() {
        return m_target_element_type;
    }

protected:
    Layout m_layout;
    Layout m_target_layout;
    element::Type m_target_element_type;
};

/// \brief Preprocessing context passed to each preprocessing operation.
/// This is internal structure which is not shared to custom operations yet.
class PreprocessingContext : public PrePostProcessingContextBase {
public:
    explicit PreprocessingContext(const Layout& layout) : PrePostProcessingContextBase(layout) {}

    const PartialShape& network_shape() const {
        return m_network_shape;
    }

    PartialShape& network_shape() {
        return m_network_shape;
    }

    size_t get_network_height_for_resize() const {
        auto network_height_idx = get_and_check_height_idx(target_layout(), network_shape());
        OPENVINO_ASSERT(network_shape()[network_height_idx].is_static(),
                        "Dynamic resize: Network height dimension shall be static");
        return network_shape()[network_height_idx].get_length();
    }

    size_t get_network_width_for_resize() const {
        auto network_width_idx = get_and_check_width_idx(target_layout(), network_shape());
        OPENVINO_ASSERT(network_shape()[network_width_idx].is_static(),
                        "Dynamic resize: Network width dimension shall be static");
        return network_shape()[network_width_idx].get_length();
    }

    const ColorFormat& color_format() const {
        return m_color_format;
    }

    ColorFormat& color_format() {
        return m_color_format;
    }

private:
    PartialShape m_network_shape;
    Layout m_network_layout;
    ColorFormat m_color_format = ColorFormat::UNDEFINED;
};

using InternalPreprocessOp =
    std::function<std::tuple<std::vector<Output<Node>>, bool>(const std::vector<Output<Node>>& nodes,
                                                              const std::shared_ptr<Function>& function,
                                                              PreprocessingContext& context)>;

/// \brief PreProcessStepsImpl - internal data structure
class PreStepsList {
public:
    void add_scale_impl(const std::vector<float>& values);
    void add_mean_impl(const std::vector<float>& values);
    void add_convert_impl(const element::Type& type);
    void add_resize_impl(ResizeAlgorithm alg, int dst_height, int dst_width);
    void add_convert_layout_impl(const Layout& layout);
    void add_convert_layout_impl(const std::vector<uint64_t>& dims);
    void add_convert_color_impl(const ColorFormat& dst_format);
    void add_reverse_channels();

    const std::list<InternalPreprocessOp>& actions() const {
        return m_actions;
    }
    std::list<InternalPreprocessOp>& actions() {
        return m_actions;
    }

    PartialShape calculate_param_shape(const PartialShape& network_shape) const {
        if (network_shape.rank().is_dynamic()) {
            return network_shape;
        }

        std::vector<Dimension> old_dims(network_shape.rank().get_length());
        std::vector<Dimension> dims(network_shape.rank().get_length());
        for (size_t i = 0; i < network_shape.rank().get_length(); i++) {
            dims[i] = network_shape[i];
        }
        for (const auto& convert : m_layout_converts) {
            old_dims = dims;
            dims = std::vector<Dimension>(network_shape.rank().get_length());
            for (size_t i = 0; i < convert.size(); i++) {
                OPENVINO_ASSERT(convert[i] < dims.size(), "Convert dimension ", convert[i], " is out of bounds.");
                dims[convert[i]] = old_dims[i];
            }
        }
        return {dims};
    }

private:
    static std::tuple<std::vector<Output<Node>>, bool> reverse_channels(const std::vector<Output<Node>>& nodes,
                                                                        const std::shared_ptr<Function>& function,
                                                                        PreprocessingContext& context);

private:
    std::list<InternalPreprocessOp> m_actions;
    std::list<std::vector<uint64_t>> m_layout_converts;
};

class PreProcessSteps::PreProcessStepsImpl : public PreStepsList {};

//------ Post process -----
class PostprocessingContext : public PrePostProcessingContextBase {
public:
    explicit PostprocessingContext(const Layout& layout) : PrePostProcessingContextBase(layout) {}
};

using InternalPostprocessOp = std::function<std::tuple<ov::Output<ov::Node>, bool>(const ov::Output<ov::Node>& node,
                                                                                   PostprocessingContext& context)>;

/// \brief PostProcessStepsImpl - internal data structure
class PostStepsList {
public:
    void add_convert_impl(const element::Type& type);
    void add_convert_layout_impl(const Layout& layout);
    void add_convert_layout_impl(const std::vector<uint64_t>& dims);

    const std::list<InternalPostprocessOp>& actions() const {
        return m_actions;
    }
    std::list<InternalPostprocessOp>& actions() {
        return m_actions;
    }

private:
    std::list<InternalPostprocessOp> m_actions;
};

class PostProcessSteps::PostProcessStepsImpl : public PostStepsList {};

}  // namespace preprocess
}  // namespace ov
