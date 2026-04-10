### Prerequisite

```shell
pip3 install pyelftools ROPgadget
```

### This directory contains the scripts for replicate K-POLARIZER's experiment

- **correctness.** This subdirectory contains the scripts to execute the entire flac test suites (since SPEC CPU2017 is commercial software, we cannot redistribuite it). 

To build flac, using the following command:
  ```shell
  cd correctness/libFLAC/flac
  mkdir build
  cd build
  cmake ..
  make -j$(nproc)
  ```
   After building flac, replace `flac/build/src/libFLAC/libFLAC.so.12.0.0` with the K-POLARIZER–transformed library, then run KP_test.sh to execute all libFLAC test suites. Ensure your environment can load the replaced `libFLAC.so.12.0.0` by setting the `LD_LIBRARY_PATH` environment variable. Update any hard-coded paths in the script as needed.

- **effectiveness.** This subdirectory contains scripts to replicate Table1/Table2 in our paper. For example, to see K-POLARIZER's effectiveness on `libFLAC` (using `flac` as the main executable), navigate to `libFLAC/flac/`, and run the bash script by `bash run.sh`. The results would be something like the following:
  
  ```shell
  func reduction:  0.293398533007335
  code reduction:  0.3384658604452055
  === Collecting data for original libflac binary ===
  Original Gadget Counts (libflac): {'All': 12463, 'NoSys': 12460, 'NoROP': 9689, 'NoJOP': 2814, 'SYS': 3, 'ROP': 2774, 'JOP': 9649}
  
  === Collecting data for slimmed libflac binary ===
  Slimmed Gadget Counts (libflac): {'All': 10274, 'NoSys': 10271, 'NoROP': 8016, 'NoJOP': 2286, 'SYS': 3, 'ROP': 2258, 'JOP': 7988}
  
  === Collecting data for worst slimmed libflac binary ===
  Slimmed Gadget Counts (libflac): {'All': 11943, 'NoSys': 11940, 'NoROP': 9236, 'NoJOP': 2747, 'SYS': 3, 'ROP': 2707, 'JOP': 9196}
  
  === Gadget Reduction for libflac ===
  All reduced by: 2189
  SYS reduced by: 0
  ROP reduced by: 516
  JOP reduced by: 1661
  
  === Gadget Reduction for libflac (worst case) ===
  All reduced by: 520
  SYS reduced by: 0
  ROP reduced by: 67
  JOP reduced by: 453
  ```
   these results should match the statistics presented in Table 2.

   **Notes**:
   The file `dmesg_output` is produced by running the executable with the K-POLARIZER kernel module installed (and compiled with debug mode by adding `#define KP_DEBUG` in `kp_print.c`); it logs runtime dependency information.
   The `glibc` and `musl-libc` directories follow the same pattern. For example, to run the `gcc` benchmark using `glibc`:
