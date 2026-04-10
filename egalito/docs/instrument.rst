Instruction-Level Instrumentation
=================================

Introduction
------------

This section describes general considerations that are necessary when adding
instrumentation instructions within existing code. We aim to cover both 64-bit
x86_64 and aarch64.

Instruction Insertion
---------------------

Egalito allows instructions to be added at any point within existing basic
blocks. Instructions are usually added with the ``ChunkMutator`` class. Note
that when inserting instructions before an existing instruction, an important
consideration is whether any incoming jumps to that existing instruction should
run the instrumentation or not. For example, instrumentation intended to be
executed once upon function entry (like stack XOR) should not be targeted by
jumps like a loop back to the first instruction; for this case, use
``insertBefore``. Conversely, any instrumentation tied to individual
instructions should use ``insertBeforeJumpTo`` so that incoming jumps will not
skip the instrumentation.

Be warned that ``insertBeforeJumpTo`` works by swapping the
InstructionSemantics inside individual Instructions. This is so that any
incoming jump references will continue to refer to the first instruction in the
block. This means that any ``Instruction *`` pointer to the insert point will
actually point at the newly inserted instruction (now the first in the block)
after a call to ``insertBeforeJumpTo``. It may be better to use insertion
functions that take an array of instructions if multiple insertions are needed,
to avoid this situation.

Egalito also provides other functions in ``ChunkMutator`` like ``insertAfter``,
``append``, etc. Basic blocks are not split automatically based on user
modifications. For this, see the ``SplitBasicBlocks`` pass. Also, if enough
instructions are inserted and short 1-byte jump instructions are used on
x86_64, the jumps me no longer be able to reach the target blocks. Run the
``PromoteJumpsPass`` to deal with this.

Simple Call Instrumentation
---------------------------

The architecture calling convention specifies what happens to each register at
function call boundaries. *Callee-saved* registers must be preserved across the
function call, and the calling function may depend on this. But *caller-saved*
registers are allowed to be overwritten by the calling function. If a
caller-saved register is not used as a parameter, it is essentially safe to
clobber the value of the register and use it for instrumentation code. On
x86_64, we frequently use ``%r10`` and ``%r11`` for this purpose. For example,
we could place the following code before a call or at the beginning of a
function to implement stack XOR, and not worry about saving and restoring
``%r11``::

    mov %fs:0x28, %r11
    xor (%rsp), %r11

Unfortunately, saving and restoring registers on x86_64 is complicated by leaf
functions. If a function does not call any other functions, the compiler may
opt to not create a stack frame and instead use positive offsets from the stack
pointer, accessing the 128 bytes beyond the top of the stack that are
guaranteed by the architecture to always be available. This is called the red
zone, and Egalito has an analysis to determine if a given function uses it.

When a function is using the red zone, an inserted push instruction will
overwrite existing data. One alternative is to use a thread-local storage
location to spill an existing register. Another alternative is to use data-flow
analysis to see if any registers are unused. On aarch64, we implemented a
register reservation pass which can reserve a register for used by
instrumentation code throughout a function. If necessary, the pass will expand
the stack frame and spill other uses of registers. This code could be ported to
x86_64 but TLS accesses work well on that platform.

Besides the standard x86 callee-saved registers, the XMM registers like
``%xmm0`` are also supposed to be preserved across call boundaries. This is not
a problem if the instrumentation code does not use XMM registers, but XMM
registers cannot be stored on a stack unless it is 16-byte aligned. A segfault
will occur otherwise. Some functions in libc such as ``memcpy`` implementations
do put XMM registers on the stack. This boils down to needing to preserve the
16-byte stack alignment that the compiler would have ensured at call
boundaries: push an even number of 8-byte quadwords.

See the ``InstrumentCallsPass`` for an example of how to handle some of these
issues, and ``InstrumentInstructionPass`` for more sophisticated version.

Instrumentation at Jumps
------------------------

x86_64 uses jump instructions in many situations:

- to target basic blocks within a function (these are often 1-byte jumps which
  may need to be promoted to 4-byte jumps with ``PromoteJumpsPass``);
- for tail recursion, i.e. calling another function and having it inherit the
  current stack frame;
- for jump tables, using an indirect jump; and
- even for indirect tail recursion, though this appears only in hand-coded
  assembly e.g. libc low-level lock functions.

Egalito classifies each jump as internal (within a function) or external (tail
recursion), and additionally identifies all jump table invocations. This is
important because many types of instrumentation do not wish to operate on all
kinds of jumps.

For internal jumps and jump table invocations, the architecture calling
convention does not help identify any usable registers. Thus, it is usually
necessary to spill an existing registers a the stack (unless analysis finds an
unused register). This is subject to the same red zone caveats mentioned above.

Indirect Calls/Jumps
--------------------

For indirect calls and jumps, it may be desirable to perform some checks on the
final target address. This is relatively easy on RISC architectures, but on
x86_64, it is not so uncommon to see control flow instructions with complex
memory operands such as::

    jmpq *(%rax,%rbx,8)     ; typical PIC jump table
    callq *0x40(%rax)       ; typical C++ vtable

We provide passes to transform such values to move the target into ``%r11``,
along instrumentation to examine it, and then calling ``%r11``.

Instrumentation at Arbitrary Points
-----------------------------------

When adding instrumentation at arbitrary points in basic blocks, all the same
caveats apply. No registers can be assumed to be available; the red zone may
get in the way of spilling existing registers. The stack may not be 16-byte
aligned, particularly in the middle of a leaf function. Furthermore, the code
may rely on the ``rflags`` register to be preserved for conditionals. We
provide an ``InstrumentInstructionPass`` that allows a call instruction to be
inserted at any arbitrary point, taking care of most of these details.

See also ``pass/endbrenforce.cpp``, ``pass/syscallsandbox.cpp``,
``pass/retpoline.cpp``, ``pass/instrumentinstr.cpp`` for some inspiration.
