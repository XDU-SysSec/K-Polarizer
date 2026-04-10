Directory Layout of Egalito
============================

In this section, we describe some relevant directories within the Egalito repository. The ``src`` directory contains the main source code of Egalito (meant to be built as a library); ``app`` contains some applications created using Egalito including ``etshell`` (a shell that can run Egalito's functionalities and passes) and ``etelf`` (a tool that for code transformation).

Core Directories
----------------

There are several subdirectories of interest within the ``src`` directory. Basic ones are :

- ``chunk`` : Contains the implementation of the `Chunk <chunk.html>`_ hierarchy. Every Chunk has a parent, and children (typically of a homogenous fixed type); many Chunks have a position. The smallest Chunk is an Instruction, and details of those are in the next subdirectory.

- ``instr`` : Contains implementations of different types of instructions including control flow instruction, linked instruction, isolated instruction etc.

- ``disasm`` : Low-level disassembly library integration, supporting Capstone (with Distorm3 on x86) and a RISC-V disassembler.

    - The most useful function is `Disassemble::instruction` in `src/disasm/disassemble.h <https://github.com/columbia/egalito/blob/master/src/disasm/disassemble.h>`_ (there are higher-level disassembly operations too, like functions).
    - There is also `Reassemble::instruction` in `src/disasm/reassemble.h <https://github.com/columbia/egalito/blob/master/src/disasm/reassemble.h>`_ (note: requires `USE_KEYSTONE=1` in the Makefile).

- ``elf``, ``dwarf`` : Lowest-level ELF file mapping. No dependency on other libraries like libelf/libdwarf.

Commonly-Used Directories
-------------------------

These subdirectories contain interfaces that are likely to be used by most users :

- ``pass`` : Contains built-in Egalito passes that can be run on the Chunk hierarchy. Many users will write their own separately, and particularly useful ones may be added here. For a simple example, check out `src/pass/stackxor.cpp <https://github.com/columbia/egalito/blob/master/src/pass/stackxor.cpp>`_.

- ``operation`` : Contains very useful classes for writing passes: ChunkFind, ChunkMutator, ChunkAddInline, etc.

- ``conductor`` : Contains the highest level interfaces (API) for the user. Particularly useful are Interface in `src/conductor/interface.h <https://github.com/columbia/egalito/blob/master/src/conductor/interface.h>`_ and ConductorSetup in `src/conductor/setup.h <https://github.com/columbia/egalito/blob/master/src/conductor/setup.h>`_.

Other Directories of Note
-------------------------

These subdirectories may be useful for some users :

- ``analysis`` : Contains analyses like control flow graph (CFG) generation, data flow analysis, usedef analysis, jump table detection etc.

- ``generate`` : ELF output file generation code.

- ``log`` : Contains implementation of Egalito logging (LOG, LOG0, ...) -- see also `the tutorial <tutorial.html#a-note-about-logging>`_.

- ``transform`` : Contains classes to direct output to files or in-memory (e.g. for the loader). To adjust code layout at a macro level, e.g. for JIT-shuffling, start here.

