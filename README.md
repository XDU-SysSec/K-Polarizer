# K-Polarizer

### Overview

This anonymous repository accompanies our paper: **One Size Does Fit All: Kernel-Assisted Fine-Grained Debloating and Layout Randomization for Shared Libraries.** 

### Prerequisite

- Install some dependencies
  
  ```shell
  sudo apt install make libelf-dev libssl-dev bison flex libncurses5-dev zstd
  ```
- Linux kernel must be built with ld.bfd, make sure you are using the bfd linker by the following command:
  
  ```shell
  readlink -e `which ld`
  ```
- If not , switch to the bfd linker
  
  ```shell
  sudo ln -sf /usr/bin/ld.bfd /usr/bin/ld
  ```

### Repository Structure

- linux-5.4.273: We have inserted hooks in `mm/memory.c`.

- SVF: This directory contains K-Polarizer's customized **SVF** pointer analysis framework, which we use to perform dependency analysis and clustering (K-Polarizer's code was added in `svf/lib/Graphs/PTACallGraph.cpp`).

- egalito: This directory contains the **Egalito** recompiler, which we use for binary reconstruction and metadata collection (K-Polarizer's code was added in `src/conductor/setup.cpp` and `src/transform/generator.cpp`).

- ko: This directory contains the source code for the loadable kernel module of K-Polarizer. While the code is documented, the documentation may be incomplete or insufficient in some areas.

### Building the Linux Kernel

- First, navigate to the kernel source
  
  ```shell
  cd linux-5.4.273/
  ```

- Use the following command for a much smaller and faster build (just use the default option).
  
  ```shell
  sudo make localmodconfig
  ```

- Compile and install the kernel.
  
  
  ```shell
  sudo make -j$(nproc)
  sudo make modules_install
  sudo make install
  ```

- Reboot and switch to the newly built kernel
  
  ```shell
  sudo reboot
  ```

### Dependency Analysis

First, navigate to the SVF directory and build SVF:

```shell
cd SVF/
# you might need to install Z3 and LLVM manually
export LLVM_DIR=/path/to/llvm
export Z3_DIR=/path/to/z3
source ./build.sh 
```

Then, using the following command to perform dependecy analysis and clustering:

```shell
cd Release-build/bin/
./wpa -nander -dump-callgraph -node-alloc-strat=dense libFLAC.so.bc
```

Dependencies and clusters would be stored in **deps** and **disjoints**, respectively.

### Binary Reconstruction

First, build Egalito (you may refer to [the offficial Egalito repository](https://github.com/columbia/egalito) for more detailed instructions):

```
sudo apt-get install make g++ libreadline-dev gdb lsb-release unzip
sudo apt-get install libc6-dbg libstdc++6-7-dbg
cd egalito/
make -j$(nproc)
```

Then, a binary can be transformed using the following command:

```shell
cd app/
./etetl -m -v libFLAC.so libFLAC.so.rewritten 2> libFLAC.fixup # -v for verbose output
```

This should output a reconstructed binary and all crucial metadata for K-Polarizer's runtime component.
