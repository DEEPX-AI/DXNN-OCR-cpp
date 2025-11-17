EMPTY_STRING := 
SPACE := $(EMPTY_STRING) $(EMPTY_STRING)

# sudo authorization needed for file write

####################
# [DRI] DevOps
WORKDIR := /root/workdir
####################

###### INPUT #######
# Interface with test framework
# If value of interface is not allowed, then identify exit code and raise an error like '@exit $(.SHELLSTATUS)'.
ARCH = x86_64
TARGET = ASIC
# MODEL = /mnt/regression_storage/dxnn_regr_data/M1_4K/1217/BGTest_1/BGTest_1.dxnn
# MODEL_INPUT_DATA = /mnt/regression_storage/dxnn_regr_data/M1_4K/1217/BGTest_1/gt/npu_0_input_0.bin
# MODEL_OUTPUT_DATA = /mnt/regression_storage/dxnn_regr_data/M1_4K/1217/BGTest_1/gt/npu_0_output_0_rt.bin

####################
# [DRI] RT
.PHONY: build check_arch config \

		verify_pybinding check_python_library check_x86 \
		run_device_interface run_device_test \
		run_model_inference model_artifact model_input_data model_output_data

SUPPORTED_CPU_ARCH := x86_64 aarch64 # value table : CPU ARCHITECTURE which needs to be tested on dxnn ci
PYTHON_MODULE := dx_engine

# install dependencies and onnxruntime library
install_dep: check_arch
	@echo ">> install dependencies"
	cd .. && ./install.sh --all --arch $(ARCH)

# build dx_rt
build: check_arch
	@echo ">> start build"
	cd .. && ./build.sh --clean --arch $(ARCH)

# build dx_rt in docker container
build_in_docker: check_arch
	@echo ">> start build"
	cd .. && ./build.sh --clean --docker --arch $(ARCH)

# build dx_rt in docker container with coverage
build_with_coverage_in_docker: check_arch
	@echo ">> start build with coverage"
	@echo ">> Set GCC coverage files"
	@export CXXFLAGS="--coverage -fprofile-arcs -ftest-coverage -g -O0 $$CXXFLAGS"; \
	export CFLAGS="--coverage -fprofile-arcs -ftest-coverage -g -O0 $$CFLAGS"; \
	export LDFLAGS="--coverage $$LDFLAGS"; \
	echo ">> Verify coverage environment variables:"; \
	echo "   CXXFLAGS: $$CXXFLAGS"; \
	echo "   CFLAGS: $$CFLAGS"; \
	echo "   LDFLAGS: $$LDFLAGS"; \
	if echo "$$CXXFLAGS" | grep -q "\-\-coverage.*\-fprofile\-arcs.*\-ftest\-coverage"; then \
		echo "   ✓ CXXFLAGS contains required coverage flags"; \
	else \
		echo "   ✗ CXXFLAGS missing coverage flags"; exit 1; \
	fi; \
	cd .. && ./build.sh --clean --docker --arch $(ARCH)

check_arch:
	@echo ">> check CPU architecture supported"
ifneq ($(filter $(ARCH),$(SUPPORTED_CPU_ARCH)),)
	@echo "$(ARCH) supported from $(SUPPORTED_CPU_ARCH)"
else
	@echo "ARCH $(ARCH) is not supported! Supported CPUs are $(SUPPORTED_CPU_ARCH)."
# when input error,
	$(shell exit 100)
	@exit $(.SHELLSTATUS)
endif

check_x86:
	@echo ">> check CPU architecture for pybinding"
ifneq ($(ARCH), x86_64)
	$(shell exit 100)
	@exit $(.SHELLSTATUS)
endif

config:
	@echo ">> make config"
ifeq ($(ARCH), x86_64)
	sed -i 's/option(USE_PYTHON \"Use Python\" OFF)/option(USE_PYTHON \"Use Python\" ON)/' ../cmake/dxrt.cfg.cmake
else
	sed -i 's/option(USE_PYTHON \"Use Python\" ON)/option(USE_PYTHON \"Use Python\" OFF)/' ../cmake/dxrt.cfg.cmake
ifeq ($(ARCH), riscv64)
	sed -i 's/-enx122//g' ../cmake/toolchain.riscv64.cmake
endif
endif

uninstall_python_package:
	@echo ">> uninstall python package"
	$(shell which pip) uninstall -y $(PYTHON_MODULE)

verify_pybinding: check_python_library
	@echo ">> verify pybinding"

check_python_library:
	@echo ">> check python library"
	if [ -f ../python_package/src/$(PYTHON_MODULE)/capi/*.so ]; then echo "python library existing"; else echo "python library not existing!"; exit 1; fi
	if [ $(shell $(shell which pip) list | grep -c $(shell echo $(PYTHON_MODULE) | sed 's/\_/\-/g')) = 1 ]; then echo "python package $(PYTHON_MODULE) installed"; else echo "python package $(PYTHON_MODULE) not installed!"; exit 1; fi
	$(shell which python) -c 'import $(PYTHON_MODULE)'
	@echo "done."



# execute dx_rt application
run_device_interface:
	@echo ">> execute dxrt-cli"
	cd .. && ./bin/dxrt-cli -s

run_device_test:
	@echo ">> execute dxrt_test"
	cd .. && ./bin/dxrt_test --gtest_filter=device.basic

run_model_inference: target model_artifact model_input_data model_output_data
	@echo ">> execute run_model"
	$(eval COMMAND = cd .. && ./bin/run_model -m $(MODEL))
ifdef MODEL_INPUT_DATA
	$(eval COMMAND += $(SPACE)-i $(MODEL_INPUT_DATA))
endif
ifdef MODEL_OUTPUT_DATA
	$(eval COMMAND += $(SPACE)-o $(MODEL_OUTPUT_DATA))
endif
ifndef MODEL_INPUT_DATA
ifndef MODEL_OUTPUT_DATA
	$(eval COMMAND += $(SPACE)-b)
endif
endif
	$(COMMAND)

target:
	@echo ">> check target - $(TARGET)"
ifneq ($(TARGET), ASIC)
	@echo "invalid type"
	@exit 1
endif

model_artifact:
ifndef MODEL
	@echo ">> needs model artifact"
	$(shell exit 100)
	@exit $(.SHELLSTATUS)
else
ifeq ($(suffix $(MODEL)), .dxnn)
	@echo ">> valid model artifact extension"
else
	@echo ">> invalid model artifact extension"
	$(shell exit 100)
	@exit $(.SHELLSTATUS)
endif
endif

model_input_data:
ifdef MODEL_INPUT_DATA
ifeq ($(suffix $(MODEL_INPUT_DATA)), .bin)
	@echo ">> valid model input data extension"
else
	@echo ">> invalid model input data extension"
	$(shell exit 100)
	@exit $(.SHELLSTATUS)
endif
endif

model_output_data:
ifdef MODEL_INPUT_DATA
ifdef MODEL_OUTPUT_DATA
ifeq ($(suffix $(MODEL_OUTPUT_DATA)), .bin)
	@echo ">> valid model output data extension"
else
	@echo ">> invalid model output data extension"
	$(shell exit 100)
	@exit $(.SHELLSTATUS)
endif
endif
endif

####################