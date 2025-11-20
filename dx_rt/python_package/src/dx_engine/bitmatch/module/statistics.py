#
# Copyright (C) 2018- DEEPX Ltd.
# All rights reserved.
#
# This software is the property of DEEPX and is provided exclusively to customers 
# who are supplied with DEEPX NPU (Neural Processing Unit). 
# Unauthorized sharing or usage is strictly prohibited by law.
#

from dataclasses import dataclass, field
from typing import List

@dataclass
class TestStatistics:
    pass_count: int = 0
    total_count: int = 0
    failed_jobs: List[int] = field(default_factory=list)
    duration: float = 0.0
    latency_mean: float = 0.0
    latency_stddev: float = 0.0
    latency_CV: float = 0.0
    inf_time_mean: float = 0.0
    inf_time_stddev: float = 0.0
    inf_time_CV: float = 0.0