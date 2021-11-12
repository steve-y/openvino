// Copyright (C) 2018-2021 Intel Corporation
// SPDX-License-Identifier: Apache-2.0
//

#include "mkldnn_deconv_node.h"
#include "mkldnn_eltwise_node.h"
#include "mkldnn_fake_quantize_node.h"
#include "mkldnn_input_node.h"
#include <mkldnn.hpp>
#include <string>
#include <vector>
#include <mkldnn_types.h>
#include <mkldnn_extension_utils.h>
#include "ie_parallel.hpp"
#include "utils/general_utils.h"
#include <ngraph/opsets/opset1.hpp>
#include <cpu/x64/cpu_isa_traits.hpp>
#include <nodes/common/cpu_memcpy.h>
#include <memory_desc/cpu_memory_desc_utils.h>
#include "memory_desc/dnnl_blocked_memory_desc.h"
#include "utils/cpu_utils.hpp"

using namespace mkldnn;
using namespace MKLDNNPlugin;
using namespace InferenceEngine;

bool MKLDNNDeconvolutionNode::isSupportedOperation(const std::shared_ptr<const ngraph::Node>& op, std::string& errorMessage) noexcept {
    try {
        if (isDynamicNgraphNode(op)) {
            errorMessage = "Doesn't support op with dynamic shapes";
            return false;
        }

        if (std::dynamic_pointer_cast<const ngraph::opset1::ConvolutionBackpropData>(op) == nullptr &&
                std::dynamic_pointer_cast<const ngraph::opset1::GroupConvolutionBackpropData>(op) == nullptr) {
            errorMessage = "Only opset1 ConvolutionBackpropData and GroupConvolutionBackpropData operations are supported";
            return false;
        }
        size_t ndims = op->get_input_shape(0).size();
        if ((ndims < 3) || (ndims > 5)) {
            errorMessage = "Only 3D, 4D and 5D blobs are supported as input";
            return false;
        }
    } catch (...) {
        return false;
    }
    return true;
}

MKLDNNDeconvolutionNode::MKLDNNDeconvolutionNode(const std::shared_ptr<ngraph::Node>& op,
                                                 const mkldnn::engine& eng, MKLDNNWeightsSharing::Ptr &cache) : MKLDNNNode(op, eng, cache) {
    internalBlobDesc.emplace_back([&](primitive_desc_iterator &primitive_desc_it, size_t idx) -> DnnlMemoryDescPtr {
        return MKLDNNExtensionUtils::makeDescriptor(primitive_desc_it.weights_desc(0));
    });
    std::string errorMessage;
    if (isSupportedOperation(op, errorMessage)) {
        errorPrefix = "Deconvolution node with name '" + getName() + "'";

        auto convBackprop = std::dynamic_pointer_cast<const ngraph::opset1::ConvolutionBackpropData>(op);
        auto groupConvBackprop = std::dynamic_pointer_cast<const ngraph::opset1::GroupConvolutionBackpropData>(op);
        const auto dataShape = op->get_input_shape(0);
        weightDims = op->get_input_shape(1);
        const auto outShape = op->get_shape();
        OC = outShape[1];
        IC = dataShape[1];

        if (convBackprop) {
            algorithm = DeconvolutionCommon;

            groupNum = 1;
            withGroups = false;

            for (int i = 0; i < convBackprop->get_strides().size(); i++) {
                stride.push_back(static_cast<ptrdiff_t>(convBackprop->get_strides()[i]));
            }
            for (int i = 0; i < convBackprop->get_dilations().size(); i++) {
                dilation.push_back(static_cast<ptrdiff_t>(convBackprop->get_dilations()[i]) - 1);
            }
            paddingL = convBackprop->get_pads_begin();
            paddingR = convBackprop->get_pads_end();
        } else if (groupConvBackprop) {
            algorithm = DeconvolutionGrouped;

            groupNum = weightDims[0];
            withGroups = groupNum > 1;
            isDW = withGroups && groupNum == OC && groupNum == IC;

            for (int i = 0; i < groupConvBackprop->get_strides().size(); i++) {
                stride.push_back(static_cast<ptrdiff_t>(groupConvBackprop->get_strides()[i]));
            }
            for (int i = 0; i < groupConvBackprop->get_dilations().size(); i++) {
                dilation.push_back(static_cast<ptrdiff_t>(groupConvBackprop->get_dilations()[i]) - 1);
            }
            paddingL = groupConvBackprop->get_pads_begin();
            paddingR = groupConvBackprop->get_pads_end();
        }
        for (int i = 0; i < dilation.size(); i++) {
            kernel.push_back(weightDims[withGroups + 2 + i]);
        }
    } else {
        IE_THROW(NotImplemented) << errorMessage;
    }
}

InferenceEngine::Blob::Ptr MKLDNNDeconvolutionNode::createWeiBlobAsIO(InferenceEngine::SizeVector dims) {
    auto constNode = std::dynamic_pointer_cast<MKLDNNInputNode>(getParentEdgeAt(1)->getParent());
    if (!constNode)
        IE_THROW() << "Cannot cast const input node for node " << getName() << ".";
    auto blb = constNode->getMemoryPtr();
    if (!blb)
        IE_THROW() << "Cannot get const weights blob for node " << getName() << ".";

    auto const blbSize = blb->GetSize();

    // WA: In int8 case, we are processing weights using internal blob.
    // So we disconnect constant node containing weights from the graph and then don't use it.
    if (getParentEdges().size() == 3) {
        removeEdge(getParentEdgeAt(2));
        inputShapes.erase(inputShapes.begin() + 2);
    }
    removeEdge(getParentEdgeAt(1));
    inputShapes.erase(inputShapes.begin() + 1);

    InferenceEngine::SizeVector dimsForBlockedDesc{dims};
    std::swap(dimsForBlockedDesc[withGroups + 0], dimsForBlockedDesc[withGroups + 1]);

    InferenceEngine::SizeVector orderForBlockedDesc;
    if (withGroups) {
        orderForBlockedDesc = {0, 2, 1};
    } else {
        orderForBlockedDesc = {1, 0};
    }
    for (int i = 2 + withGroups; i < dimsForBlockedDesc.size(); i++)
        orderForBlockedDesc.push_back(i);

    BlockingDesc blkDesc(dimsForBlockedDesc, orderForBlockedDesc);
    InferenceEngine::TensorDesc tensorDesc(MKLDNNExtensionUtils::DataTypeToIEPrecision(blb->GetDataType()), dims, blkDesc);

    Blob::Ptr internalBlob = InferenceEngine::make_shared_blob<int8_t>(tensorDesc);
    internalBlob->allocate();
    char *data = internalBlob->buffer();
    if (data == nullptr)
        IE_THROW(NotAllocated) << "Internal blob was not allocated for node " << getName() << ".";
    size_t intBuffSize = internalBlob->byteSize();

    size_t offset = blbSize;
    if (intBuffSize < offset) {
        IE_THROW() << "Cannot create internal buffer. Buffer can be overrun.";
    }
    cpu_memcpy_s(data, intBuffSize, blb->GetPtr(), blbSize);

    return internalBlob;
}

bool MKLDNNDeconvolutionNode::canBeExecutedInInt8() const {
    if (std::dynamic_pointer_cast<MKLDNNInputNode>(getParentEdgeAt(1)->getParent()) == nullptr) {
        return false;
    }

    // todo: [antonvor] added these checks to fix performance problems
    if (kernel.size() == 3)
        return false;
    if (!withGroups && stride.back() > 3)
        return false;
    if (!impl::cpu::x64::mayiuse(impl::cpu::x64::avx512_common)) {
        auto inDims = getOutputShapeAtPort(0).getStaticDims();
        // heuristicConst = 2^26
        // heuristicParam = IC^2 * SP
        auto heuristicConst = 67108864;
        auto heuristicParam = IC * IC;
        for (int i = 2; i < inDims.size(); i++)
            heuristicParam *= inDims[i];
        if (heuristicParam > heuristicConst)
            return false;
    }

    for (int i = 0; i < kernel.size(); i++) {
        if (kernel[i] < stride[i])
            return false;
    }

    // not supported in oneDNN
    int channelBlock = impl::cpu::x64::mayiuse(impl::cpu::x64::avx512_common) ? 16
            : impl::cpu::x64::mayiuse(impl::cpu::x64::avx2) ? 8 : 4;
    if (withGroups && !isDW && (IC % channelBlock != 0 || OC % channelBlock != 0))
        return false;
    if (!impl::cpu::x64::mayiuse(impl::cpu::x64::avx512_common) && stride.back() > 3)
        return false;

    InferenceEngine::Precision inPrecision = getOriginalInputPrecisionAtPort(0);
    auto inputDataType = MKLDNNExtensionUtils::IEPrecisionToDataType(inPrecision);

    InferenceEngine::Precision weiPrecision = getOriginalInputPrecisionAtPort(1);
    auto weightsDataType = MKLDNNExtensionUtils::IEPrecisionToDataType(weiPrecision);

    if (isDW && (inputDataType == dnnl_s8 || dilation.size() == 3))
        return false;

    return (inputDataType == dnnl_s8 || inputDataType == dnnl_u8) && weightsDataType == dnnl_s8;
}

bool MKLDNNDeconvolutionNode::canFuse(const MKLDNNNodePtr& node) const {
    if (canBeExecutedInInt8())
        return canFuseSimpleOperation(node);

    return (fusedWith.empty() && node->canBePerformedAsScaleShift(this));
}

void MKLDNNDeconvolutionNode::getSupportedDescriptors() {
    if (!descs_fwd.empty() && !descs_bwd.empty())
        return;

    isInt8 = canBeExecutedInInt8();

    InferenceEngine::Precision inPrecision = getOriginalInputPrecisionAtPort(0);
    InferenceEngine::Precision outPrecision = getOriginalOutputPrecisionAtPort(0);
    if (isInt8) {
        // TODO: We have to extend jit_avx512_core_x8s8s32x_deconv_fwd_kernel from oneDNN to support BF16 output data type
        if (InferenceEngine::Precision::BF16 == inPrecision)
            inPrecision = InferenceEngine::Precision::FP32;
        if (InferenceEngine::Precision::BF16 == outPrecision)
            outPrecision = InferenceEngine::Precision::FP32;
    } else {
        if (!one_of(inPrecision, InferenceEngine::Precision::FP32, InferenceEngine::Precision::BF16))
            inPrecision = InferenceEngine::Precision::FP32;
        if (!one_of(outPrecision, InferenceEngine::Precision::FP32, InferenceEngine::Precision::BF16))
            outPrecision = InferenceEngine::Precision::FP32;
    }
    auto inputDataType = MKLDNNExtensionUtils::IEPrecisionToDataType(inPrecision);
    auto outputDataType = MKLDNNExtensionUtils::IEPrecisionToDataType(outPrecision);
    if (inputDataType == memory::data_type::bf16 || outputDataType == memory::data_type::bf16)
       inputDataType = outputDataType = memory::data_type::bf16;
    if (!fusedWith.empty()) {
        outputDataType = MKLDNNExtensionUtils::IEPrecisionToDataType(fusedWith[fusedWith.size() - 1]->getOriginalOutputPrecisionAtPort(0));
    }

    if (getParentEdges().size() != 2 && getParentEdges().size() != 3)
        IE_THROW() << errorPrefix << " has incorrect number of input edges";
    if (getChildEdges().empty())
        IE_THROW() << errorPrefix << " has incorrect number of output edges";

    for (int i = 0; i < paddingR.size(); i++) {
        int with_group = getAlgorithm() == DeconvolutionGrouped ? 1 : 0;
        int krn = weightDims[with_group + 2 + i];
        int src = getOutputShapeAtPort(0).getStaticDims()[2 + i];
        int dst = getInputShapeAtPort(0).getStaticDims()[2 + i];

        krn = (krn - 1)*(dilation[i] + 1) + 1;
        int calc_dst = (src - krn + paddingL[i]) / stride[i] + 1;
        paddingR[i] = (dst - calc_dst) * stride[i];
    }

    if (isInt8) {
        //  WA: if int8 deconvolution is supported, we create internal weights blob in IO format
        std::swap(weightDims[withGroups + 0], weightDims[withGroups + 1]);
        internalBlobs.push_back(createWeiBlobAsIO(weightDims));
        auto format = getInputShapeAtPort(0).getRank() == 5 ? dnnl::memory::format_tag::ndhwc : dnnl::memory::format_tag::nhwc;
        MemoryDescPtr in_candidate = std::make_shared<DnnlBlockedMemoryDesc>(getInputShapeAtPort(0), inputDataType, format);
        MemoryDescPtr out_candidate = std::make_shared<DnnlBlockedMemoryDesc>(getOutputShapeAtPort(0), outputDataType, format);
        createDescriptor({in_candidate}, {out_candidate});
    } else {
        for (auto format : getAvailableFormatsForDims(getInputShapeAtPort(0))) {
            MemoryDescPtr in_candidate = std::make_shared<DnnlBlockedMemoryDesc>(getInputShapeAtPort(0), inputDataType, format);
            MemoryDescPtr out_candidate = std::make_shared<DnnlBlockedMemoryDesc>(getOutputShapeAtPort(0), outputDataType, format);
            createDescriptor({in_candidate}, {out_candidate});
        }
    }
    setPostOps(attr);
}

void MKLDNNDeconvolutionNode::setPostOps(mkldnn::primitive_attr &attr) {
    mkldnn::post_ops ops;

    for (auto &node : fusedWith) {
        auto* eltwiseNode = dynamic_cast<MKLDNNEltwiseNode *>(node.get());
        if (eltwiseNode) {
            // TODO [DS]: change to shape from memory
            constexpr int align = 16;
            eltwiseNode->appendPostOps(ops, getOutputShapeAtPort(0).getStaticDims(), align);
            continue;
        }
        auto* fakeQuantizeNode = dynamic_cast<MKLDNNFakeQuantizeNode *>(node.get());
        if (fakeQuantizeNode) {
            fakeQuantizeNode->appendPostOps(ops);
            continue;
        }
        IE_THROW() << "Fusing of " << NameFromType(node->getType()) << " operation to " << NameFromType(this->getType()) << " node is not implemented";
    }

    attr.set_post_ops(ops);
}

void MKLDNNDeconvolutionNode::filterSupportedPrimitiveDescriptors() {
    MKLDNNNode::filterSupportedPrimitiveDescriptors();
    filterSupportedDescriptors();
}

void MKLDNNDeconvolutionNode::filterSupportedDescriptors() {
    if (!inputMemoryFormatsFilter.empty() || !outputMemoryFormatsFilter.empty()) {
        if (inputMemoryFormatsFilter.size() > 1 || outputMemoryFormatsFilter.size() > 1) {
            IE_THROW() << "Incorrect number of input or output memory formats for Deconvolution node";
        }
        auto itd = descs.begin();
        while (itd != descs.end()) {
            bool isSuitableDesc = true;
            if (!inputMemoryFormatsFilter.empty()) {
                if (isInt8) {
                    auto src_tdesc = MKLDNNExtensionUtils::makeDescriptor(std::shared_ptr<dnnl::deconvolution_forward::desc>(*itd)->data.src_desc);
                    isSuitableDesc &= src_tdesc->isSame(inputMemoryFormatsFilter[0]);
                } else {
                    auto src_tdesc = MKLDNNExtensionUtils::makeDescriptor(std::shared_ptr<mkldnn::convolution_backward_data::desc>(*itd)->data.diff_src_desc);
                    isSuitableDesc &= src_tdesc->isSame(inputMemoryFormatsFilter[0]);
                }
            }
            if (!outputMemoryFormatsFilter.empty()) {
                if (isInt8) {
                    auto dst_tdesc = MKLDNNExtensionUtils::makeDescriptor(std::shared_ptr<mkldnn::deconvolution_forward::desc>(*itd)->data.dst_desc);
                    isSuitableDesc &= dst_tdesc->isSame(outputMemoryFormatsFilter[0]);
                } else {
                    auto dst_tdesc = MKLDNNExtensionUtils::makeDescriptor(std::shared_ptr<mkldnn::convolution_backward_data::desc>(*itd)->data.diff_dst_desc);
                    isSuitableDesc &= dst_tdesc->isSame(outputMemoryFormatsFilter[0]);
                }
            }
            if (!isSuitableDesc) {
                itd = descs.erase(itd);
            } else {
                itd++;
            }
        }
    }
}

bool MKLDNNDeconvolutionNode::created() const {
    return getType() == Deconvolution;
}

void MKLDNNDeconvolutionNode::createPrimitive() {
    if (prim)
        return;

    if (isInt8) {
        auto prim_desc = createPrimitiveDescriptor<deconvolution_forward::primitive_desc,
                deconvolution_forward::desc>(attr);

        prim.reset(new deconvolution_forward(prim_desc));

        auto src = getParentEdgesAtPort(0)[0]->getMemoryPtr()->GetPrimitive();
        auto dst = getChildEdgesAtPort(0)[0]->getMemoryPtr()->GetPrimitive();
        primArgs = {{DNNL_ARG_SRC, src}, {DNNL_ARG_WEIGHTS, internalBlobMemory[0]->GetPrimitive()}, {DNNL_ARG_DST, dst}};
    } else {
        auto prim_desc = createPrimitiveDescriptor<convolution_backward_data::primitive_desc,
                convolution_backward_data::desc, convolution_forward::primitive_desc>(attr);

        prim.reset(new convolution_backward_data(prim_desc));

        auto src = getParentEdgesAtPort(0)[0]->getMemoryPtr()->GetPrimitive();
        auto weights = getParentEdgeAt(1)->getMemory().GetPrimitive();
        auto dst = getChildEdgesAtPort(0)[0]->getMemoryPtr()->GetPrimitive();
        primArgs = {{DNNL_ARG_DIFF_DST, src}, {DNNL_ARG_WEIGHTS, weights}, {DNNL_ARG_DIFF_SRC, dst}};
    }
}

void MKLDNNDeconvolutionNode::createDescriptor(const std::vector<MemoryDescPtr> &inputDesc,
                                               const std::vector<MemoryDescPtr> &outputDesc) {
    const auto in_candidate = MemoryDescUtils::convertToDnnlBlockedMemoryDesc(*inputDesc[0]);
    const auto out_candidate = MemoryDescUtils::convertToDnnlBlockedMemoryDesc(*outputDesc[0]);

    // grouping and autoblicking is not compatible
    if ((withGroups && !isDW) && (in_candidate.blocksExtended() || out_candidate.blocksExtended()))
        return;

    auto convertDims = [] (const std::vector<ptrdiff_t>& orig_dims) {
        return memory::dims(orig_dims.begin(), orig_dims.end());
    };

    if (isInt8) {
        mkldnn::memory::desc wgh_candidate(MKLDNNExtensionUtils::convertToDnnlDims(weightDims), memory::data_type::s8, memory::format_tag::any);
        std::shared_ptr<mkldnn::deconvolution_forward::desc> deconv_desc;
        deconv_desc.reset(new deconvolution_forward::desc(prop_kind::forward_inference, mkldnn::algorithm::deconvolution_direct,
                                                          in_candidate.getDnnlDesc(), wgh_candidate, out_candidate.getDnnlDesc(),
                                                          convertDims(stride), convertDims(dilation),
                                                          convertDims(paddingL), convertDims(paddingR)));
        descs.emplace_back(deconv_desc);
    } else {
        mkldnn::memory::desc wgh_candidate(MKLDNNExtensionUtils::convertToDnnlDims(weightDims), in_candidate.getDataType(), memory::format_tag::any);
        for (auto alg : {mkldnn::algorithm::convolution_winograd, mkldnn::algorithm::convolution_direct}) {
            std::shared_ptr<mkldnn::convolution_forward::desc> conv_desc;
            conv_desc.reset(new convolution_forward::desc(prop_kind::forward_inference, alg,
                                                          out_candidate.getDnnlDesc(), wgh_candidate, in_candidate.getDnnlDesc(),
                                                          convertDims(stride),
                                                          convertDims(dilation),
                                                          convertDims(paddingL),
                                                          convertDims(paddingR)));

            std::shared_ptr<mkldnn::convolution_backward_data::desc> deconv_desc;
            deconv_desc.reset(new convolution_backward_data::desc(alg, out_candidate.getDnnlDesc(), wgh_candidate,
                                                                  in_candidate.getDnnlDesc(),
                                                                  convertDims(stride),
                                                                  convertDims(dilation),
                                                                  convertDims(paddingL),
                                                                  convertDims(paddingR)));
            descs_fwd.push_back(conv_desc);
            descs_bwd.push_back(deconv_desc);

            auto fwd_conv_pd = std::make_shared<convolution_forward::primitive_desc>(*conv_desc, getEngine(), true);
            if (fwd_conv_pd->get(true) == nullptr)
                continue;

            descs.emplace_back(deconv_desc, fwd_conv_pd);
        }
    }
}

std::shared_ptr<MemoryDesc> MKLDNNDeconvolutionNode::getSrcMemDesc(mkldnn::primitive_desc_iterator &primitive_desc_it, size_t idx) {
    if (idx == 2) {
        return std::make_shared<CpuBlockedMemoryDesc>(getOriginalInputPrecisionAtPort(2), Shape(getInputShapeAtPort(2).getStaticDims()));
    }

    auto desc = idx > 0 ? primitive_desc_it.weights_desc(idx - 1) : isInt8 ? primitive_desc_it.src_desc(idx) : primitive_desc_it.diff_dst_desc(idx);
    return MKLDNNExtensionUtils::makeDescriptor(desc);
}

std::shared_ptr<MemoryDesc> MKLDNNDeconvolutionNode::getDstMemDesc(mkldnn::primitive_desc_iterator &primitive_desc_it, size_t idx) {
    auto desc =  isInt8 ? primitive_desc_it.dst_desc(idx) : primitive_desc_it.diff_src_desc(idx);
    return MKLDNNExtensionUtils::makeDescriptor(desc);
}

InferenceEngine::Precision MKLDNNDeconvolutionNode::getRuntimePrecision() const {
    std::vector<InferenceEngine::Precision> inputPrecisions;
    // Don't take bias precision into account
    size_t inputsNumLimit = 2;
    for (size_t i = 0; i < std::min(getParentEdges().size(), inputsNumLimit); i++) {
        auto parentEdge = getParentEdgeAt(i);
        if (parentEdge && parentEdge->getStatus() == MKLDNNEdge::Status::Validated) {
            inputPrecisions.emplace_back(MKLDNNExtensionUtils::DataTypeToIEPrecision((parentEdge->getMemoryPtr()->GetDataType())));
        }
    }

    return getMaxPrecision(inputPrecisions);
}

REG_MKLDNN_PRIM_FOR(MKLDNNDeconvolutionNode, Deconvolution);
