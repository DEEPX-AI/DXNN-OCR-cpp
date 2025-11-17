#
# Copyright (C) 2018- DEEPX Ltd.
# All rights reserved.
#
# This software is the property of DEEPX and is provided exclusively to customers 
# who are supplied with DEEPX NPU (Neural Processing Unit). 
# Unauthorized sharing or usage is strictly prohibited by law.
#

from dataclasses import dataclass, field
from dx_engine import InferenceOption
from typing import List

@dataclass
class TestConfig:
    use_ort: bool = False
    sync_mode: bool = False
    batch_mode: bool = False
    iterations: int = 1
    verbose: bool = True
    log_enabled: bool = True
    performance_mode: bool = False
    debug_mode: bool = False
    save_debug_report: bool = False  # Save detailed debug analysis to JSON
    npu_bound: InferenceOption.BOUND_OPTION = InferenceOption.BOUND_OPTION.NPU_ALL
    devices: List[int] = field(default_factory=list)
    input_order: str = "random"
