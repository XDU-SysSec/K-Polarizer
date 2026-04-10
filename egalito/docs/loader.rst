Loader Mode and Code Generation
===============================

Running and Debugging the Loader
--------------------------------

Egalito's custom loader can be invoked with::

    $ cd src ; ./loader ex/hello ; cd -

Note that we have not been focusing on loader development and `ELF generation
mode <elfgen.html>`_ is generally more reliable. When debugging the loader,
make use of ``symbols.elf`` which Egalito generates at load-time::

    $ gdb -q --args ./loader ex/hello
    Reading symbols from ./loader...done.
    (gdb) l loader.cpp:172
    167	        EgalitoTLS::setSandbox(shufflingSandbox);
    168	        EgalitoTLS::setGSTable(gsTable);
    169	    }
    170	
    171	    // jump to the target program (never returns)
    172	    start2();
    173	    //_start2();
    174	}
    175	
    176	void EgalitoLoader::otherPasses() {
    (gdb) b loader.cpp:172
    Breakpoint 1 at 0x6013ac09: file load/loader.cpp, line 172.
    (gdb) r
    ...
    Breakpoint 1, EgalitoLoader::run (this=this@entry=0x7fffffffe480) at load/loader.cpp:172
    172	    start2();
    (gdb) si
    0x0000000040136e96 in ?? ()
    (gdb) add-symbol-file symbols.elf 0
    add symbol table from file "symbols.elf" at
    y
    	.text_addr = 0x0
    (y or n) y
    Reading symbols from symbols.elf...(no debugging symbols found)...done.
    (gdb) disass
    Dump of assembler code for function _start2$new:
    => 0x0000000040136e96 <+0>:	push   %rbx
       0x0000000040136e97 <+1>:	callq  *-0x2ec96abd(%rip)        # 0x114a03e0
       0x0000000040136e9d <+7>:	pop    %rbx
       0x0000000040136e9e <+8>:	mov    -0x2ec963c5(%rip),%rax        # 0x114a0ae0
       0x0000000040136ea5 <+15>:	mov    (%rax),%rsp
       0x0000000040136ea8 <+18>:	xor    %rdx,%rdx
       0x0000000040136eab <+21>:	mov    -0x2ec9646a(%rip),%rbx        # 0x114a0a48
       0x0000000040136eb2 <+28>:	jmpq   *(%rbx)
       0x0000000040136eb4 <+30>:	hlt    
    End of assembler dump.
    (gdb) 

We set a breakpoint at the place where Egalito first jumps to transformed code.
The dynamically-generated code within ``0x40000000`` was unknown until we told
gdb about the new symbols. ``_start2`` is the original code, ``_start2$new`` is
the transformed code; every symbol simply has ``$new`` appended. The first
indirect call invokes constructors, the last indirect jump targets the program
entry point ``_start$new``. You can set breakpoints e.g. at ``main$new``.

Runtime Code Generation
-----------------------

To perform runtime code generation, create a Sandbox to store new code. Then
use a backing to decide how that code is stored We provide ``MemoryBacking``
which is an mmap directly at the target address, useful for code generation
within the target process, and also ``MemoryBufferBacking`` which stores the
generated code in an ``std::string``, useful for writing code to a file or
generating code at an address that cannot otherwise be mapped (e.g. kernel code
generation). See ``ConductorSetup::makeLoaderSandbox`` for an example.

Now, assign codes to new addresses and then generate it. For these two steps,
see ``transform/generator.h``.
