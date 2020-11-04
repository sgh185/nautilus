ENTRY(__nk_exec_entry)

SECTIONS
{
    . = 0x0;  /* we will relocate in all cases */


    .text : ALIGN(0x1000) {
        *(.mbhdr)
	. = ALIGN(0x1000);
	/* The actual code begins at 0x1000 / offset 0x2000 into file */
    	_loadStart = .;
        *(.text*)
        *(.gnu.linkonce.t*)
    }

    .data : ALIGN(0x1000) {
        *(.data*)
        *(.gnu.linkonce.d*)
    }
    
    .rodata : ALIGN(0x1000) {
        *(.rodata*)
        *(.gnu.linkonce.r*)
    }

    /* This order may cause problems, but who knows? */
    .dynamic        : { *(.dynamic) }
    
    .got : ALIGN(0x1000) {
        *(.got*)
        *(.gnu.linkconce.got*)
    }


    /* Added this to silence the seemingly-benign linker error when including libc -ARN */
    .tdata : ALIGN(0x1000) {
        *(.tdata .tdata.* .gnu.linkonce.td.*)
    }

    _loadEnd = .; 
    
    .bss : ALIGN(0x1000) {
        *(COMMON)
        *(.bss*)
        *(.gnu.linkonce.b*)
    }

    _bssEnd = .; 

    __preinit_array_start = .;
    __preinit_array_end = .;
    __init_array_start = .;
    __init_array_end = .;
    __fini_array_start = .;
    __fini_array_end = .;

    heap_start = .;
    
    /DISCARD/ :
    {
        *(.comment)
        /* *(.eh_frame) */
	
    }
}

