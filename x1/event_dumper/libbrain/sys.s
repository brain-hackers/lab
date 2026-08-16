.text
.align 4

.macro thunk name, addr
.global \name
.type   \name, %function
\name:
    ldr     pc, [pc, #-4]
    .word   \addr
.endm

thunk malloc,   0x600063c4
thunk free,     0x600063c8
thunk realloc,  0x600063d0

thunk fopen,    0x60006400
thunk fclose,   0x60006404
thunk ftell,    0x60006408
thunk fread,    0x6000640c
thunk fwrite,   0x60006410
thunk feof,     0x60006418
thunk fflush,   0x6000641c
thunk fgetc,    0x60006420
thunk fgets,    0x60006424
thunk fputc,    0x60006428
thunk fputs,    0x6000642c
thunk ferror,   0x60006430
thunk remove,   0x60006434
thunk rename,   0x60006438
thunk mkdir,    0x6000643c
thunk stat,     0x60006444

thunk closedir, 0x6000644c
thunk readdir,  0x60006450
thunk opendir,  0x60006454

thunk printf,   0x60092004
thunk memset,   0x60092108
thunk memcpy,   0x60092118
