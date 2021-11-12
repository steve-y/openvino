// Copyright (C) 2018-2021 Intel Corporation
// SPDX-License-Identifier: Apache-2.0
//

/**
 * @brief This is a header file for the OpenVINO Runtime tensor API
 *
 * @file openvino/runtime/remote_tensor.hpp
 */
#pragma once

#include "openvino/runtime/parameter.hpp"
#include "openvino/runtime/tensor.hpp"

namespace ov {
namespace runtime {
class RemoteContext;

/**
 * @brief Remote memory access and interpretation API
 *
 * It can throw exceptions safely for the application, where it is properly handled.
 */
class OPENVINO_RUNTIME_API RemoteTensor : public Tensor {
    using Tensor::Tensor;
    friend class ov::runtime::RemoteContext;

public:
    /**
     * @brief Checks openvino remote type
     * @param tensor tensor which type will be checked
     * @param type_info map with remote object runtime info
     * @throw Exception if type check with specified paramters is not pass
     */
    static void type_check(const Tensor& tensor, const std::map<std::string, std::vector<std::string>>& type_info = {});

    void* data(const element::Type) = delete;

    template <typename T>
    T* data() = delete;

    /**
     * @brief Returns a map of device-specific parameters required for low-level
     * operations with underlying object.
     * Parameters include device/context/surface/buffer handles, access flags,
     * etc. Content of the returned map depends on remote execution context that is
     * currently set on the device (working scenario).
     * Abstract method.
     * @return A map of name/parameter elements.
     */
    runtime::ParamMap get_params() const;

    /**
     * @brief Returns name of the device on which underlying object is allocated.
     * Abstract method.
     * @return A device name string in fully specified format `<device_name>[.<device_id>[.<tile_id>]]`.
     */
    std::string get_device_name() const;
};
}  // namespace runtime
}  // namespace ov