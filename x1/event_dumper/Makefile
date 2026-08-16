include lvgl/lvgl.mk

CROSS_COMPILE:=arm-none-eabi-
AS:=$(CROSS_COMPILE)as
CC:=$(CROSS_COMPILE)gcc
STRIP:=$(CROSS_COMPILE)strip
OBJCOPY:=$(CROSS_COMPILE)objcopy

CFLAGS = -static -fno-pic -fno-pie -no-pie -marm  -mcpu=cortex-a7 -O0 -I. -U__linux__ -fno-builtin-free
LDFLAGS = -nostdlib -marm -Llibbrain -lbrain -lgcc
LDFLAGS +=  -T linker.ld

SRCS = main.c
SRCS += $(CSRCS)

OBJS = $(SRCS:.c=.o)
OBJS := $(OBJS:.S=.o)

TARGET = AppMain.bin
LIBBRAIN = libbrain/libbrain.a

all: $(TARGET)

%.o: %.S
	$(CC) $(CFLAGS) -c $< -o $@

$(LIBBRAIN):
	$(MAKE) -C libbrain

$(TARGET): $(OBJS) $(LIBBRAIN)
	@$(CC) $(OBJS)  $(CFLAGS) $(LDFLAGS) -o AppMain.elf
	@$(OBJCOPY) -O binary AppMain.elf $(TARGET)

clean:
	rm -f $(OBJS) $(TARGET) AppMain.elf
	$(MAKE) -C libbrain clean
