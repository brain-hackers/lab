CROSS_COMPILE:=arm-linux-gnueabihf-
AS:=$(CROSS_COMPILE)as

.PHONY:
all: return.bin mrc.bin

%.bin: %.S
	@$(AS) $<
	@./extract.py a.out $@
	@rm -f a.out