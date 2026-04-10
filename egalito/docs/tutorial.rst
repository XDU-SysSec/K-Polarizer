Egalito tutorial
================

Setup
-----

Egalito currently runs on most Linux flavours, on 64-bit x86_64 and aarch64
architectures. To run Egalito, you will need a few dependencies (described in
the README), specifically as of this writing::

    $ sudo apt-get install make g++ libreadline-dev gdb lsb-release

Please also install debug packages for libc and libstdc++ (e.g., ``libc6-dbg``
and ``libstdc++6-7-dbg`` on Debian-derived systems). Although Egalito does not
need debug symbols in general, we require symbols for these packages to perform
loader emulation correctly::

    $ sudo apt-get install libc6-dbg libstdc++6-7-dbg  # names may differ

Now, obtain Egalito source code by cloning with the ``--recursive`` flag to
bring in all submodules::

    $ git clone git@github.com:columbia/egalito.git --recursive

Building and Running Egalito
----------------------------

To compile Egalito, simply run ``make`` from the root directory::

    $ make -j 8

Change 8 to your available number of CPU cores (though the codebase is large
enough that we recommend using at least 8 cores). For the first build, the
config target will create a default ``config/config.h`` based on your
architecture and Linux distribution (you may wish to edit this file). The dep
target will then compile all dependencies, including ``dep/rtld`` which must be
built on the same system that the Egalito loader will eventually be executed
on. For cross-compilation, be sure to generate the ``dep/rtld`` definitions on
the target system. Finally, make will build the main ``src`` directory,
followed by ``app`` and ``test``.

Egalito has three primary modes: mirror ELF generation, union ELF generation,
and loader mode. These modes can be tested with a hello world program::

    $ cd app ; ./etelf -m ../src/ex/hello hello ; ./hello ; cd -

    $ cd app ; ./etelf -u ../src/ex/hello hello ; ./hello ; cd -

    $ cd src ; ./loader ex/hello ; cd -

For detailed information about these modes, see `ELF generation <elfgen.html>`_
and `Loader details <loader.html>`_.

Applying Hardening
------------------

The ``etelf`` program mentioned earlier performs no-op transformations and is a
helpful reference for writing a standalone ELF hardening tool. To access the
hardening mechanisms within the Egalito codebase, try the ``etharden``
program::

    $ cd app ; ./etharden -m --cfi ../src/ex/hello hello ; ./hello ; cd -

This program allows you to specify multiple transformations to perform,
although be warned that many combinations are nonsensical.

Test Suite
----------

We have a small set of code-generation tests that can be run with::

    $ cd test/codegen ; make ; cd -

Our main integration tests, which can take a long time to run, are executed as
follows::

    $ cd test/script ; make ; cd -

We run a continuous integration system `here <http://ci.egalito.org:8010/>`_
where our test suite is executed on different platforms and architectures.

Introduction to the Egalito Shell
---------------------------------

The ``app`` directory contains the Egalito *shell*
(``etshell``), which allows fine-grained invocation of passes and other
functionality. For example, try the following (note that your system may have
different addresses or instructions)::

    $ ./etshell
    Welcome to the egalito shell version b8c5284. Type "help" for usage.
    egalito> parse ../src/ex/hello
    [...]
    egalito> modules
    module-(executable)
    egalito> functions3 module-(executable)
    0x000004e8 0x00000017 _init
    0x00000530 0x00000017 main
    0x00000550 0x0000002b _start
    0x00000580 0x00000040 deregister_tm_clones
    0x000005c0 0x00000050 register_tm_clones
    0x00000610 0x00000040 __do_global_dtors_aux
    0x00000650 0x00000010 frame_dummy
    0x00000660 0x00000065 __libc_csu_init
    0x000006d0 0x00000002 __libc_csu_fini
    0x000006d4 0x00000009 _fini
    egalito> disass main
    ---[main]---
    main/bb+0:
        0x00000530 <+  0>:  leaq         0x100001ad(%rip), %rdi    <.rodata>
        0x00000537 <+  7>:  subq         $8, %rsp
        0x0000053b <+ 11>:  (CALL)       0x510                     <puts@plt>
    main/bb+16:
        0x00000540 <+  0>:  xorl         %eax, %eax
        0x00000542 <+  2>:  addq         $8, %rsp
        0x00000546 <+  6>:  retq
    egalito> stackxor
    egalito> disass main
    ---[main]---
    main/bb+0:
        0x00000530 <+  0>:  movq         %fs:0x28, %r11
        0x00000539 <+  9>:  xorq         %r11, 0(%rsp)
        0x0000053d <+ 13>:  leaq         0x100001a0(%rip), %rdi    <.rodata>
        0x00000544 <+ 20>:  subq         $8, %rsp
        0x00000548 <+ 24>:  (CALL)       0x510                     <puts@plt>
    main/bb+29:
        0x0000054d <+  0>:  xorl         %eax, %eax
        0x0000054f <+  2>:  addq         $8, %rsp
        0x00000553 <+  6>:  movq         %fs:0x28, %r11
        0x0000055c <+ 15>:  xorq         %r11, 0(%rsp)
        0x00000560 <+ 19>:  retq
    egalito>

Here we parsed a simple hello world program, and examined the code for main
before and after running the stackxor hardening pass. You can see how some
instructions were inserted and addresses were automatically adapted. To avoid
confusion, Egalito will not reassign function addresses until you run the
``reassign`` command (and hence functions may overlap until then).

The shell provides ``parse``, which analyzes a single binary; ``parse2``, which
analyzes all library dependencies; and ``parse3``, which additionally analyzes
``libegalito.so`` and its dependencies. Since parsing libraries and large
programs can take several seconds, we provide Egalito *archives* or HOBBIT
files. Archives are a serialization of the Chunk structures and are quite
efficient. To see this in action::

    $ ./etshell
    egalito> parse3 ../src/ex/hello
    [...]
    egalito> modules
    module-(executable)
    module-(egalito)
    module-libc.so.6
    module-libdistorm3.so
    module-libpthread.so.0
    module-libstdc++.so.6
    module-libm.so.6
    module-libgcc_s.so.1
    egalito> archive hello.ega
    [...]
    egalito> quit
    $ ../src/loader hello.ega
    [...]
    Hello, World!

An archive can store transformed code, and you can repeatedly load
(``parse-archive``) and save (``archive``) an archive in the shell for repeated
transformations. Not all defenses and passes can be combined or support
archives, but if you are trying to do something reasonable and it does not
work, please file a bug report.

Creating a Tool with Egalito
----------------------------

We have an out-of-tree example app repo which includes Egalito as a submodule,
which is a good starting point for most people. You can also make modifications
within the Egalito source tree (probably create new passes inside
``src/pass/``) and add options to ``etharden`` or the Egalito shell
(``app/shell/disass.cpp``) to invoke the new functionality. 

To add functionality to loader mode, create new passes in ``src/pass``, and
then add invocations to ``EgalitoLoader::otherPasses()``. Then simply run
``./loader`` to invoke your new code. We provide an ``isFeatureEnabled`` which
checks if environment variables are set, allowing multiple defenses to co-exist
(e.g. ``EGALITO_DEBLOAT``, ``EGALITO_LOG_CALL``, ``EGALITO_USE_RETPOLINES``,
...).

Final note: running ``make`` inside the ``app`` directory will automatically
make the ``src`` directory too.

A Note about Logging
--------------------

We have a large number of debugging and log messages in Egalito, because its
operations are very complex and it can be hard to tell why a crash or invalid
transformation has occurred for a new binary. There is an environment variable
``EGALITO_DEBUG`` that controls which log messages will be printed (run
``./loader`` with no args to see help). To see fewer messages, try::

    $ EGALITO_DEBUG=/dev/null ./loader ex/hello

Messages are broken into named categories (see ``src/log/defaults.h``),
primarily based on src subdirectory names. To debug a particular component and
see more messages, try::

    $ EGALITO_DEBUG=chunk=20:disasm=20 ./loader ex/hello

There is also a ``log`` command in the shell which can set debugging levels.
Both this and the EGALITO_DEBUG mechanism control log messages at runtime. If a
log level is set to -1 in defaults.h, or if you run ``make release`` in src,
log messages will be removed at compile-time. This provides good performance
when Egalito is deployed in production settings or for performance evaluation.

Log messages are written as follows (requires ``log/log.h``)::

    LOG(1, "This is a C++ message, hello 0x" << std::hex << address);
    CLOG(1, "This is a C message, hello 0x%08x", address);
    LOG0(1, "With LOG0/CLOG0, no newline is printed: ");

The default log level for most categories is 10, so messages of level 9 or less
will be printed. Messages of level 0 are supposed to not be filtered out unless
removed at compile-time for a release build.

To add a new debug category for your Egalito-based app, use the following::

    #undef DEBUG_GROUP
    #define DEBUG_GROUP myapp
    #define D_myapp 9
    #include "log/log.h"

Getting Involved
----------------

Egalito is still rough around the edges and under development. If you are using
the code at all, please do join our mailing list and report bugs and become
involved in the project. Other resources are listed on `our main page
<http://egalito.org>`_.

Thanks, and happy recompiling! ~~
