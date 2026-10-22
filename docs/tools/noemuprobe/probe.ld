SECTIONS { . = 0x1020; .text : { start.o(.text) *(.text) *(.rodata*) } . = ALIGN(0x1000); .data : { *(.data) *(.bss) *(COMMON) } /DISCARD/ : { *(.comment) *(.note*) *(.eh_frame) } }
