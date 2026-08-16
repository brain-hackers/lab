CROSS_COMPILE := arm-linux-gnueabihf-
AS := $(CROSS_COMPILE)as
AR := $(CROSS_COMPILE)ar

TARGET := libbrain.a
OBJS := sys.o

ASFLAGS := -W
ARFLAGS := -crs

.PHONY:
all: $(TARGET)

.PHONY:
clean:
	-rm -f $(TARGET) *.o

%.o: %.s
	$(AS) $(ASFLAGS) $< -o $@

$(TARGET): $(OBJS)
	$(AR) $(ARFLAGS) $@ $^

