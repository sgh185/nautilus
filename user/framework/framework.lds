OUTPUT_FORMAT("elf64-x86-64", "elf64-x86-64",
              "elf64-x86-64")
OUTPUT_ARCH(i386:x86-64)
ENTRY(_start)


SECTIONS
{
    __naut_hash_begin = .;
    . = 0x0;  /* we will relocate in all cases */

    __ehdr_start = . - 0x1000;

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

    .rela.dyn       :
    {
      *(.rela.init)
      *(.rela.text .rela.text.* .rela.gnu.linkonce.t.*)
      *(.rela.fini)
      *(.rela.rodata .rela.rodata.* .rela.gnu.linkonce.r.*)
      *(.rela.data .rela.data.* .rela.gnu.linkonce.d.*)
      *(.rela.tdata .rela.tdata.* .rela.gnu.linkonce.td.*)
      *(.rela.tbss .rela.tbss.* .rela.gnu.linkonce.tb.*)
      *(.rela.ctors)
      *(.rela.dtors)
      *(.rela.got)
      *(.rela.bss .rela.bss.* .rela.gnu.linkonce.b.*)
      *(.rela.ldata .rela.ldata.* .rela.gnu.linkonce.l.*)
      *(.rela.lbss .rela.lbss.* .rela.gnu.linkonce.lb.*)
      *(.rela.lrodata .rela.lrodata.* .rela.gnu.linkonce.lr.*)
      *(.rela.ifunc)
    }
    .rela.plt       :
    {
      *(.rela.plt)
      *(.rela.iplt)
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
    __naut_hash_end = .;

    .naut_secure : {
        *(.naut_secure)
    }

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