CROSS_COMPILE:=arm-linux-gnueabihf-
AS:=$(CROSS_COMPILE)as
CC:=$(CROSS_COMPILE)gcc
STRIP:=$(CROSS_COMPILE)strip

.PHONY:
all: return.bin mrc.bin c/main.bin

.PHONY:
clean:
	@rm -f *.bin spray/*.bin c/*.bin c/*.elf injector/*.bin injector/*.elf

%.bin: %.S
	@$(AS) $<
	@./extract.py a.out $@
	@rm -f a.out

c/main.bin:
	@$(CC) -nostdlib -static -fPIC -mcpu=cortex-a7 c/start.S c/main.c
	@./extract.py a.out $@
	@rm -f a.out

spray/main.bin:
	@$(AS) spray/top.S
	@./extract.py -p a.out spray/top.bin
	@$(AS) spray/bottom.S
	@./extract.py -p a.out spray/bottom.bin
	@$(AS) spray/bottom_reset.S
	@./extract.py -p a.out spray/bottom_reset.bin
	@./spray/append_nop.py spray/top.bin spray/bottom.bin spray/bottom_reset.bin spray/main.bin 112 113
	@rm -f a.out

injector/AppMain.bin:
	@if [ "$(INJECTED_S)" = "" ]; then \
		echo "Please specify INJECTED_S."; \
		exit 1; \
	fi
	@$(AS) $(INJECTED_S) -o injector/injected.elf
	@./extract.py -p injector/injected.elf injector/injected.bin
	@$(AS) injector/disable_mmu.S -o injector/disable_mmu.elf
	@./extract.py -p injector/disable_mmu.elf injector/disable_mmu.bin
	@./injector/inject.py 0xf00000 0x700000 injector/disable_mmu.bin injector/injected.bin injector/AppMain.bin

injector/uboot.bin:
	@$(AS) injector/disable_mmu.S -o injector/disable_mmu.elf
	@./extract.py -p injector/disable_mmu.elf injector/disable_mmu.bin
	@./extract.py -p injector/disable_mmu.elf injector/disable_mmu.bin
	@./injector/inject.py 0xed0000 0x700000 injector/disable_mmu.bin u-boot.bin injector/uboot.bin
samplefb/AppMain.bin:
	@$(CC) -nostdlib -static -fPIC -marm -march=armv7-a+fp -O0 samplefb/main.c -o samplefb/AppMain.elf -T samplefb/linker.ld
	@./extract.py -p samplefb/AppMain.elf  samplefb/AppMain.bin
