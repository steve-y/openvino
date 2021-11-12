# Copyright (C) 2018-2021 Intel Corporation
# SPDX-License-Identifier: Apache-2.0

from typing import Optional
import numpy as np

from openvino.impl import Node
from openvino.utils.decorators import nameable_op
from openvino.utils.node_factory import NodeFactory
from openvino.utils.types import (
    as_node,
    NodeInput,
)


def _get_node_factory(opset_version: Optional[str] = None) -> NodeFactory:
    """Return NodeFactory configured to create operators from specified opset version."""
    if opset_version:
        return NodeFactory(opset_version)
    else:
        return NodeFactory()
