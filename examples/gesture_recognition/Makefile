# =============================================================================
# Makefile - Gesture Recognition Example
# mmWave Radar SDK
# =============================================================================

SDK_ROOT    ?= ../..
SDK_INC      = $(SDK_ROOT)/sdk/include
SDK_API_INC  = $(SDK_ROOT)/sdk/app_api
SDK_LIB      = $(SDK_ROOT)/sdk/lib

CC       = arm-none-eabi-gcc
CFLAGS   = -mcpu=cortex-r5 -mfloat-abi=hard -mfpu=vfpv3-d16 \
           -O2 -g -Wall -Wextra -Werror \
           -DPLATFORM_AWR2944
INCLUDES = -I$(SDK_INC) -I$(SDK_API_INC) -I.
LDFLAGS  = -L$(SDK_LIB) -lmmwave_sdk -lm -T$(SDK_ROOT)/platform/awr2944/awr2944_r5f.lds

TARGET   = gesture_recognition.elf
SRCS     = main.c gesture_engine.c
OBJS     = $(SRCS:.c=.o)

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) $(OBJS) -o $@ $(LDFLAGS)
	@echo "Built $(TARGET) successfully."

%.o: %.c
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET)
