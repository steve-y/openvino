// Copyright (C) 2018-2021 Intel Corporation
// SPDX-License-Identifier: Apache-2.0
//

/**
 * @brief This is a header file for the OpenVINO Runtime RemoteContext class
 *
 * @file openvino/runtime/remote_context.hpp
 */
#pragma once

#include <map>
#include <memory>
#include <string>

#include "openvino/core/shape.hpp"
#include "openvino/core/type/element_type.hpp"
#include "openvino/runtime/common.hpp"
#include "openvino/runtime/parameter.hpp"
#include "openvino/runtime/remote_tensor.hpp"

namespace InferenceEngine {
class RemoteContext;
}  // namespace InferenceEngine

namespace ov {
namespace runtime {

class Core;
class ExecutableNetwork;

/**
 * @brief This class represents an abstraction
 * for remote (non-CPU) accelerator device-specific execution context.
 * Such context represents a scope on the device within which executable
 * networks and remote memory blobs can exist, function and exchange data.
 */
class OPENVINO_RUNTIME_API RemoteContext {
protected:
    std::shared_ptr<void> _so;                 //!< Reference to shared object that loaded implementation
    std::shared_ptr<ie::RemoteContext> _impl;  //!< Pointer to remote context implementation

    /**
     * @brief Constructs RemoteContext from the initialized std::shared_ptr
     * @param so Plugin to use. This is required to ensure that RemoteContext can work properly even if plugin
     * object is destroyed.
     * @param impl Initialized shared pointer
     */
    RemoteContext(const std::shared_ptr<void>& so, const std::shared_ptr<ie::RemoteContext>& impl);
    friend class ov::runtime::Core;
    friend class ov::runtime::ExecutableNetwork;

public:
    /**
     * @brief Checks openvino remote type
     * @param remote_context a remote context which type will be checked
     * @param type_info map with remote object runtime info
     * @throw Exception if type check with specified paramters is not pass
     */
    static void type_check(const RemoteContext& remote_context,
                           const std::map<std::string, std::vector<std::string>>& type_info = {});

    /**
     * @brief Default constructor
     */
    RemoteContext() = default;

    /**
     * @brief Checks if the RemoteContext object can be cast to the type T
     *
     * @tparam T Type to be checked. Must represent a class derived from the RemoteContext
     * @return true if this object can be dynamically cast to the type T*. Otherwise, false
     */
    template <typename T>
    bool is() const noexcept {
        static_assert(std::is_base_of<RemoteContext, T>::value,
                      "Could not check type that is not inherited from RemoteContext");
        try {
            T::type_check(*this);
        } catch (...) {
            return false;
        }
        return true;
    }

    /**
     * @brief Casts this RemoteContext object to the type T.
     *
     * @tparam T Type to cast to. Must represent a class derived from the RemoteContext
     * @return T object
     */
    template <typename T>
    const T as() const {
        static_assert(std::is_base_of<RemoteContext, T>::value,
                      "Could not check type that is not inherited from RemoteContext");
        T::type_check(*this);
        return *static_cast<const T*>(this);
    }

    /**
     * @brief Casts this RemoteContext object to the type T.
     *
     * @tparam T Type to cast to. Must represent a class derived from the RemoteContext
     * @return T object
     */
    template <typename T>
    operator T() const {
        return as<T>();
    }

    /**
     * @brief Returns name of the device on which underlying object is allocated.
     * Abstract method.
     * @return A device name string in fully specified format `<device_name>[.<device_id>[.<tile_id>]]`.
     */
    std::string get_device_name() const;

    /**
     * @brief Allocates memory tensor in device memory or wraps user-supplied memory handle
     * using the specified tensor description and low-level device-specific parameters.
     * Returns a pointer to the object which implements RemoteTensor interface.
     * @param type Defines the element type of the tensor
     * @param shape Defines the shape of the tensor
     * @param params Map of the low-level tensor object parameters.
     * @return A pointer to plugin object that implements RemoteTensor interface.
     */
    RemoteTensor create_tensor(const element::Type& type, const Shape& shape, const ParamMap& params = {});

    /**
     * @brief Returns a map of device-specific parameters required for low-level
     * operations with underlying object.
     * Parameters include device/context handles, access flags,
     * etc. Content of the returned map depends on remote execution context that is
     * currently set on the device (working scenario).
     * Abstract method.
     * @return A map of name/parameter elements.
     */
    ParamMap get_params() const;
};

}  // namespace runtime
}  // namespace ov
