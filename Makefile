# mmWave Radar SDK - Build System
# Supports: AWR2944, IWR6843, AWR1843
# Compiler: TI ARM CGT (R5F), TI C6000 CGT (DSP)

PLATFORM    ?= awr2944
BUILD       ?= release
SDK_VERSION := 2.4.0

# Toolchain paths
TI_ARM_CGT  ?= /opt/ti/cgt-arm/20.2
TI_C6X_CGT  ?= /opt/ti/cgt-c6x/8.3
CC_R5F       = $(TI_ARM_CGT)/bin/armcl
CC_DSP       = $(TI_C6X_CGT)/bin/cl6x
LD_R5F       = $(TI_ARM_CGT)/bin/armlnk
AR           = $(TI_ARM_CGT)/bin/armar

# Build directories
BUILD_DIR   := build/$(PLATFORM)/$(BUILD)
OBJ_DIR     := $(BUILD_DIR)/obj
LIB_DIR     := $(BUILD_DIR)/lib

# Include paths
INCLUDES    := -Isdk/include -Isdk/hal -Isdk/middleware -Isdk/app_api \
               -Isdk/dsp -Isdk/error -Iplatform/common -Iplatform/$(PLATFORM)

# Compiler flags
CFLAGS_COMMON := -mv7R5 --code_state=32 --float_support=VFPv3D16 \
                 -DPLATFORM_$(shell echo $(PLATFORM) | tr a-z A-Z) \
                 -DSDK_VERSION=\"$(SDK_VERSION)\" $(INCLUDES)

ifeq ($(BUILD),debug)
    CFLAGS := $(CFLAGS_COMMON) -g --opt_level=0 -DDEBUG
else
    CFLAGS := $(CFLAGS_COMMON) --opt_level=3 -DNDEBUG
endif

# Source files
HAL_SRCS     := $(wildcard sdk/hal/*.c)
MW_SRCS      := $(wildcard sdk/middleware/*.c)
API_SRCS     := $(wildcard sdk/app_api/*.c)
DSP_SRCS     := $(wildcard sdk/dsp/*.c)
ERR_SRCS     := $(wildcard sdk/error/*.c)
PLAT_SRCS    := $(wildcard platform/$(PLATFORM)/*.c)

ALL_SRCS     := $(HAL_SRCS) $(MW_SRCS) $(API_SRCS) $(DSP_SRCS) $(ERR_SRCS) $(PLAT_SRCS)
ALL_OBJS     := $(ALL_SRCS:%.c=$(OBJ_DIR)/%.o)

# Linker script
LD_SCRIPT    := platform/$(PLATFORM)/$(PLATFORM)_r5f.lds

# Library output
SDK_LIB      := $(LIB_DIR)/libmmwave_sdk.a

.PHONY: all clean test_unit test_integration test_system examples

all: $(SDK_LIB)

$(SDK_LIB): $(ALL_OBJS) | $(LIB_DIR)
	@echo "[AR] Creating SDK library: $@"
	$(AR) r $@ $^

$(OBJ_DIR)/%.o: %.c | $(OBJ_DIR)
	@mkdir -p $(dir $@)
	@echo "[CC] $<"
	$(CC_R5F) $(CFLAGS) -c $< -o $@

$(OBJ_DIR) $(LIB_DIR):
	@mkdir -p $@

# Examples
examples: $(SDK_LIB)
	@for dir in examples/*/; do \
		echo "[BUILD] $$dir"; \
		$(MAKE) -C $$dir PLATFORM=$(PLATFORM) SDK_LIB=$(CURDIR)/$(SDK_LIB); \
	done

# Unit Tests (host-compiled with GCC for CI)
test_unit:
	@echo "Running unit tests..."
	gcc -DUNIT_TEST -Isdk/include -Iplatform/common \
		tests/unit/*.c sdk/dsp/range_fft.c sdk/dsp/cfar_detection.c \
		sdk/error/error_handler.c -lm -o $(BUILD_DIR)/test_unit
	$(BUILD_DIR)/test_unit

test_integration:
	@echo "Running integration tests (requires hardware)..."
	$(CC_R5F) $(CFLAGS) tests/integration/*.c $(ALL_SRCS) \
		-z $(LD_SCRIPT) -o $(BUILD_DIR)/test_integration.out

test_system:
	@echo "Running system tests (requires hardware)..."
	$(CC_R5F) $(CFLAGS) tests/system/*.c $(ALL_SRCS) \
		-z $(LD_SCRIPT) -o $(BUILD_DIR)/test_system.out

clean:
	rm -rf build/
