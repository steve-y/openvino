// Copyright (C) 2018-2021 Intel Corporation
// SPDX-License-Identifier: Apache-2.0
//

#include "ngraph/visibility.hpp"

#ifdef ie_backend_EXPORTS  // defined if we are building the ie_backend as shared library
#    define IE_BACKEND_API OPENVINO_CORE_EXPORTS
#else
#    define IE_BACKEND_API OPENVINO_CORE_IMPORTS
#endif
