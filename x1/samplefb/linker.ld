OUTPUT_FORMAT("elf32-littlearm", "elf32-bigarm",
	      "elf32-littlearm")
OUTPUT_ARCH(arm)
ENTRY(main)

SECTIONS
{
    .text 0x90000000:
    {
        *(.text.init)
        *(.text)
        *(.text.*)
    }
}
