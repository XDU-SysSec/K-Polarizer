ELF Output Generation
=====================

Description
-----------

Egalito supports parsing ELF files, transforming them, and recreating new ELF
output files. As of this writing, ELF generation of userspace binaries is only
tested on x86_64. To generate an output binary with no transformations, try::

    $ ./etelf -m ../src/ex/hello hello && ./hello

To additionally harden the binary, try::

    $ ./etharden -m --cfi ../src/ex/hello hello && ./hello

Egalito has two main ELF generation modes, 1-1 mirrorgen (-m argument) and
uniongen (-u argument). Mirrorgen reads in one ELF (a single ``Module``) and
produces a corresponding output. Uniongen recursively reads in an ELF and all
dependencies (like ``parse2`` in etshell), and merges them all into a single
output. Conceptually, uniongen essentially turns a dynamically linked
executable into a statically linked one (for technical reasons, however, the
output still uses ``ld.so``).

Mirrorgen is generally the most reliable and can be applied to executables or
shared libraries. Since we do not generate symbol version structures, if using
a transformed shared library, the executable may also have to be transformed.
(We also have a helper program ``rmver`` which can modify the dynamic section
of an executable to remove references to versions.) Uniongen can produce
programs with better performance than the original, and produces self-contained
outputs, but does not support ``dlopen`` (e.g. ``libc`` opening ``libnss``).

Output Addresses
----------------

Egalito encodes a fair amount of meaning in the addresses in an output ELF.
Here is an example of the layout of a mirrorgen ELF::

    $ readelf -SW hello-mirrorgen
    Section Headers:
      [Nr] Name              Type            Address          Off    Size   ES Flg Lk Inf Al
      [ 0]                   NULL            0000000000000000 000000 000000 00      0   0  1
      [ 1] .interp           PROGBITS        0000000000200040 000040 00001c 00      0   0  1
      [ 2] .init_array       INIT_ARRAY      000000000020005c 00005c 000010 00  WA  0   0  1
      [ 3] .strtab           STRTAB          0000000000000000 00029c 0000a2 00      0   0  1
      [ 4] .shstrtab         STRTAB          0000000000000000 00033e 00009d 00      0   0  1
      [ 5] .dynstr           STRTAB          0000000000400000 001000 000091 00      0   0  1
      [ 6] .symtab           SYMTAB          0000000000400091 001091 000348 18      3  29  8
      [ 7] .dynsym           DYNSYM          00000000004003d9 0013d9 0000c0 18   A  5   1  8
      [ 8] .gnu.hash         GNU_HASH        0000000000400499 001499 0017dc 00   A  7   0  8
      [ 9] .rela.dyn         RELA            0000000000401c75 002c75 000108 18      7   0  8
      [10] .dynamic          DYNAMIC         0000000000401d7d 002d7d 0000f0 00      5   0  1
      [11] .g.got.plt        PROGBITS        0000000000500000 003000 000028 00  WA  0   0  1
      [12] .rela.plt         RELA            0000000000500028 003028 000018 18  AI  7   0  8
      [13] .plt              PROGBITS        0000000000600000 004000 000030 00  AX  0   0  1
      [14] .rodata           PROGBITS        0000000010000750 005750 000012 00   A  0   0  1
      [15] .got              PROGBITS        0000000010200fd0 005fd0 000030 00  WA  0   0  1
      [16] .got.plt          PROGBITS        0000000010201000 006000 000020 00  WA  0   0  1
      [17] .data             PROGBITS        0000000010201020 006020 000010 00  WA  0   0  1
      [18] .bss              PROGBITS        0000000010201030 006030 000008 00  WA  0   0  1
      [19] .text             PROGBITS        0000000040000000 007000 0001de 00  AX  0   0  1

All sections with addresses less than ``0x10000000`` are auto-generated from
scratch. For example, we re-create ``.init_array``, ``.strtab``, ``.gnu.hash``,
and ``.dynamic``, along with a new global offset table and procedure linkage
table in ``.g.got.plt`` and ``.plt``. These sections contain information from
all Modules in the case of uniongen. In both output modes, we place all code
into a new ``.text`` placed at ``0x400000000`` (original code is normally at
addresses beginning with ``0x4...`` but fewer digits).

All sections beginning with address ``0x10XXXXXX`` are from the first ELF
parsed, and were originally at address ``0xXXXXXX``. Similarly, in uniongen,
anything beginning with ``0x11XXXXXX`` is from the second ELF parsed, while
``0x12XXXXXX`` is from the third::

    $ readelf -SW hello-uniongen 
    Section Headers:
      [Nr] Name              Type            Address          Off    Size   ES Flg Lk Inf Al
      [ 0]                   NULL            0000000000000000 000000 000000 00      0   0  1
      [ 1] .interp           PROGBITS        0000000000200040 000040 00001c 00      0   0  1
      [ 2] .init_array       INIT_ARRAY      000000000020005c 00005c 000020 00  WA  0   0  1
      [ 3] .strtab           STRTAB          0000000000000000 000354 0117f5 00      0   0  1
      [ 4] .shstrtab         STRTAB          0000000000000000 011b49 00014b 00      0   0  1
      [ 5] .dynstr           STRTAB          0000000000400000 012000 000076 00      0   0  1
      [ 6] .symtab           SYMTAB          0000000000400076 012076 01ca40 18      3 2775  8
      [ 7] .dynsym           DYNSYM          000000000041cab6 02eab6 000090 18   A  5   1  8
      [ 8] .rela.dyn         RELA            000000000041cb46 02eb46 0001c8 18      7   0  8
      [ 9] .dynamic          DYNAMIC         000000000041cd0e 02ed0e 0000e0 00      5   0  1
      [10] .g.got.plt        PROGBITS        0000000000500000 02f000 000028 00  WA  0   0  1
      [11] .rela.plt         RELA            0000000000500028 02f028 000030 18  AI  7   0  8
      [12] .plt              PROGBITS        0000000000600000 030000 000030 00  AX  0   0  1
      [13] .rodata           PROGBITS        0000000010000750 031750 000012 00   A  0   0  1
      [14] .got              PROGBITS        0000000010200fd0 031fd0 000030 00  WA  0   0  1
      [15] .got.plt          PROGBITS        0000000010201000 032000 000020 00  WA  0   0  1
      [16] .data             PROGBITS        0000000010201020 032020 000010 00  WA  0   0  1
      [17] .bss              PROGBITS        0000000010201030 032030 000008 00  WA  0   0  1
      [18] __libc_freeres_fn PROGBITS        00000000111493e0 0333e0 000e08 00   A  0   0  1
      [19] __libc_thread_freeres_fn PROGBITS        000000001114a1f0 0341f0 000212 00   A  0   0  1
      [20] .rodata           PROGBITS        000000001114a420 034420 020a20 00   A  0   0  1
      [21] .tdata            PROGBITS        00000000113957c8 0557c8 000010 00  WA  0   0  1
      [22] __libc_subfreeres PROGBITS        00000000113957e0 0557e0 0000f8 00  WA  0   0  1
      [23] __libc_atexit     PROGBITS        00000000113958d8 0558d8 000008 00  WA  0   0  1
      [24] __libc_thread_subfreeres PROGBITS        00000000113958e0 0558e0 000020 00  WA  0   0  1
      [25] __libc_IO_vtables PROGBITS        0000000011395900 055900 000d68 00  WA  0   0  1
      [26] .data.rel.ro      PROGBITS        0000000011396680 056680 002520 00  WA  0   0  1
      [27] .got              PROGBITS        0000000011398d80 058d80 000280 00  WA  0   0  1
      [28] .got.plt          PROGBITS        0000000011399000 059000 000080 00  WA  0   0  1
      [29] .data             PROGBITS        0000000011399080 059080 001680 00  WA  0   0  1
      [30] .bss              PROGBITS        000000001139a700 05a700 004260 00  WA  0   0  1
      [31] .tdata            PROGBITS        00000000113957c8 05f7c8 000010 00 WAT  0   0  1
      [32] .tbss             PROGBITS        00000000113957d8 05f7d8 000000 00 WAT  0   0  1
      [33] .text             PROGBITS        0000000040000000 060000 125df0 00  AX  0   0  1

In this example, ``0x11XXXXXX`` is libc. Since we construct addresses in this
way, addresses of global variables or data sections can be easily mapped back
to the original ELFs. For example, this uniongen output contains a relocation
at ``0x11398df8`` for ``_rtld_global``. We can look at the original libc at
address ``0x398df8`` and will also find the same relocation. We completely
regenerate the relocation list, but since data layout is normally unchanged,
the parallel addressing helps when comparing an original and transformed ELF.

Note that Egalito can generate position-dependent outputs (uniongen only) or
position-independent outputs (default for mirrorgen and uniongen). The
addressing is normally the same in both cases.

