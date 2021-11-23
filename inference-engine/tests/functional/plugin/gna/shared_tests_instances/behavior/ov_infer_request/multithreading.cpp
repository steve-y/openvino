// Copyright (C) 2018-2021 Intel Corporation
// SPDX-License-Identifier: Apache-2.0
//

#include "behavior/ov_infer_request/multithreading.hpp"

using namespace ov::test::behavior;
namespace {

const std::vector<std::map<std::string, std::string>> configs = {
        {{GNA_CONFIG_KEY(LIB_N_THREADS), "3"}}
};

INSTANTIATE_TEST_SUITE_P(smoke_BehaviorTests, OVInferRequestMultithreadingTests,
        ::testing::Combine(
                ::testing::Values(CommonTestUtils::DEVICE_GNA),
                ::testing::ValuesIn(configs)),
        OVInferRequestMultithreadingTests::getTestCaseName);
}  // namespace
