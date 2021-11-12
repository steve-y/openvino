# Copyright (C) 2021 Intel Corporation
# SPDX-License-Identifier: Apache-2.0

import numpy as np
import os
import pytest

from tests.test_inference_engine.helpers import model_path, read_image
from openvino import Core, Blob, TensorDesc, StatusCode


is_myriad = os.environ.get("TEST_DEVICE") == "MYRIAD"
test_net_xml, test_net_bin = model_path(is_myriad)


def test_get_perf_counts(device):
    ie_core = Core()
    net = ie_core.read_network(test_net_xml, test_net_bin)
    ie_core.set_config({"PERF_COUNT": "YES"}, device)
    exec_net = ie_core.load_network(net, device)
    img = read_image()
    request = exec_net.create_infer_request()
    td = TensorDesc("FP32", [1, 3, 32, 32], "NCHW")
    input_blob = Blob(td, img)
    request.set_input({"data": input_blob})
    request.infer()
    pc = request.get_perf_counts()
    assert pc["29"]["status"] == "EXECUTED"
    assert pc["29"]["layer_type"] == "FullyConnected"
    del exec_net
    del ie_core
    del net


@pytest.mark.skipif(os.environ.get("TEST_DEVICE", "CPU") != "CPU",
                    reason=f"Can't run test on device {os.environ.get('TEST_DEVICE', 'CPU')}, "
                           "Dynamic batch fully supported only on CPU")
@pytest.mark.skip(reason="Fix")
def test_set_batch_size(device):
    ie_core = Core()
    ie_core.set_config({"DYN_BATCH_ENABLED": "YES"}, device)
    net = ie_core.read_network(test_net_xml, test_net_bin)
    net.batch_size = 10
    data = np.ones(shape=net.input_info["data"].input_data.shape)
    exec_net = ie_core.load_network(net, device)
    data[0] = read_image()[0]
    request = exec_net.create_infer_request()
    request.set_batch(1)
    td = TensorDesc("FP32", [1, 3, 32, 32], "NCHW")
    input_blob = Blob(td, data)
    request.set_input({"data": input_blob})
    request.infer()
    assert np.allclose(int(round(request.output_blobs["fc_out"].buffer[0][2])), 1), \
        "Incorrect data for 1st batch"
    del exec_net
    del ie_core
    del net


@pytest.mark.skip(reason="Fix")
def test_set_zero_batch_size(device):
    ie_core = Core()
    net = ie_core.read_network(test_net_xml, test_net_bin)
    exec_net = ie_core.load_network(net, device)
    request = exec_net.create_infer_request()
    with pytest.raises(ValueError) as e:
        request.set_batch(0)
    assert "Batch size should be positive integer number but 0 specified" in str(e.value)
    del exec_net
    del ie_core
    del net


@pytest.mark.skip(reason="Fix")
def test_set_negative_batch_size(device):
    ie_core = Core()
    net = ie_core.read_network(test_net_xml, test_net_bin)
    exec_net = ie_core.load_network(net, device)
    request = exec_net.create_infer_request()
    with pytest.raises(ValueError) as e:
        request.set_batch(-1)
    assert "Batch size should be positive integer number but -1 specified" in str(e.value)
    del exec_net
    del ie_core
    del net


def test_blob_setter(device):
    ie_core = Core()
    net = ie_core.read_network(test_net_xml, test_net_bin)
    exec_net_1 = ie_core.load_network(network=net, device_name=device)

    net.input_info["data"].layout = "NHWC"
    exec_net_2 = ie_core.load_network(network=net, device_name=device)

    img = read_image()

    request1 = exec_net_1.create_infer_request()
    tensor_desc = TensorDesc("FP32", [1, 3, img.shape[2], img.shape[3]], "NCHW")
    img_blob1 = Blob(tensor_desc, img)
    request1.set_input({"data": img_blob1})
    request1.infer()
    res_1 = np.sort(request1.get_blob("fc_out").buffer)

    img = np.transpose(img, axes=(0, 2, 3, 1)).astype(np.float32)
    tensor_desc = TensorDesc("FP32", [1, 3, 32, 32], "NHWC")
    img_blob = Blob(tensor_desc, img)
    request = exec_net_2.create_infer_request()
    request.set_blob("data", img_blob)
    request.infer()
    res_2 = np.sort(request.get_blob("fc_out").buffer)
    assert np.allclose(res_1, res_2, atol=1e-2, rtol=1e-2)


def test_cancel(device):
    ie_core = Core()
    net = ie_core.read_network(test_net_xml, test_net_bin)
    exec_net = ie_core.load_network(net, device)
    img = read_image()
    td = TensorDesc("FP32", [1, 3, 32, 32], "NCHW")
    input_blob = Blob(td, img)
    request = exec_net.create_infer_request()

    def callback(req, code, array):
        array.append(42)

    data = []
    request.set_completion_callback(callback, data)
    request.set_input({"data": input_blob})
    request.async_infer()
    request.cancel()
    with pytest.raises(RuntimeError) as e:
        request.wait()
    assert "[ INFER_CANCELLED ]" in str(e.value)
    # check if callback has executed
    assert data == [42]

    request.async_infer()
    status = request.wait()
    assert status == StatusCode.OK
    assert data == [42, 42]
