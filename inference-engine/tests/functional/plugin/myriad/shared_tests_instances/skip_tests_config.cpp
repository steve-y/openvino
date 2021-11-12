// Copyright (C) 2018-2021 Intel Corporation
// SPDX-License-Identifier: Apache-2.0
//

#include <vector>
#include <string>

#include "functional_test_utils/skip_tests_config.hpp"

std::vector<std::string> disabledTestPatterns() {
    return {
        // Not supported activation types
        ".*ActivationLayerTest\\.CompareWithRefs/Tanh.*netPRC=FP32.*",
        ".*ActivationLayerTest\\.CompareWithRefs/Exp.*netPRC=FP32.*",
        ".*ActivationLayerTest\\.CompareWithRefs/Log.*netPRC=FP32.*",
        ".*ActivationLayerTest\\.CompareWithRefs/Sigmoid.*netPRC=FP32.*",
        ".*ActivationLayerTest\\.CompareWithRefs/Relu.*netPRC=FP32.*",
        // TODO: Issue: 26268
        ".*ConcatLayerTest.*axis=0.*",
        // TODO: Issue 31197
        R"(.*(IEClassBasicTestP).*smoke_registerPluginsXMLUnicodePath.*)",
        // TODO: Issue: 34348
        R"(.*IEClassGetAvailableDevices.*)",
        // TODO: Issue: 40473
        R"(.*TopKLayerTest.*mode=min.*sort=index.*)",
        // TODO: Issue: 42828
        R"(.*DSR_NonMaxSuppression.*NBoxes=(5|20|200).*)",
        // TODO: Issue: 42721
        R"(.*(DSR_GatherND).*)",
        // TODO: Issue 26090
        ".*DSR_GatherStaticDataDynamicIdx.*f32.*1.3.200.304.*",
        // TODO: Issue 47315
        ".*ProposalLayerTest.*",
        // TODO: Issue 51804
        ".*InferRequestPreprocessConversionTest.*oPRC=U8.*",
        // TODO: Issue 54163
        R"(.*ActivationLayerTest.*SoftPlus.*)",
        // TODO: Issue 54722
        R"(.*TS=\(\(16\.16\.96\)_\(96\)_\).*eltwiseOpType=FloorMod_secondaryInputType=PARAMETER_opType=VECTOR_netPRC=FP32.*)",
        // TODO: Issue 57108
        R"(.*QueryNetworkHETEROWithMULTINoThrow_V10.*)",
        R"(.*QueryNetworkMULTIWithHETERONoThrow_V10.*)",
        // TODO: Issue 58162
        R"(.*HoldersTestOnImportedNetwork\.CreateRequestWithCoreRemoved.*)",
        // TODO: Issue 58621
        R"(.*IEClassNetworkTestP\.LoadNetworkActualNoThrow.*)",
        R"(.*IEClassNetworkTestP\.LoadNetworkActualHeteroDeviceNoThrow.*)",
        // Not implemented yet:
        R"(.*Behavior.*ExecutableNetworkBaseTest.*canSetConfigToExecNet.*)",
        R"(.*Behavior.*ExecutableNetworkBaseTest.*canExport.*)",
        // TODO: CVS-65013
        R"(.*LoadNetworkCreateDefaultExecGraphResult.*)",
        // Not expected behavior
        R"(.*Behavior.*ExecNetSetPrecision.*canSetOutputPrecisionForNetwork.*U8.*)",
        R"(.*CoreThreadingTestsWithIterations.*)",
        R"(.*OVExecutableNetworkBaseTest.*CanSetConfigToExecNet.*)",
        R"(.*OVExecutableNetworkBaseTest.*canLoadCorrectNetworkToGetExecutableWithIncorrectConfig.*)",
        R"(.*OVClassNetworkTestP.*(SetAffinityWithConstantBranches|SetAffinityWithKSO).*)",
        // TODO: Issue: CVS-69640
        R"(.*EltwiseLayerTest.*OpType=Prod.*)",
        R"(.*EltwiseLayerTest.*OpType=SqDiff.*PARAMETER.*SCALAR.*)",
        R"(.*EltwiseLayerTest.*TS=\(\(16\.16\.96\)_\(96\)_\).*OpType=SqDiff.*)",
        R"(.*EltwiseLayerTest.*TS=\(\(52\.1\.52\.3\.2\)_\(2\)_\).*OpType=SqDiff.*)",
    };
}
