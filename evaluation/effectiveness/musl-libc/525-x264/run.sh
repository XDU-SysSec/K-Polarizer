#!/bin/bash


echo "Running for libc..."
./extract_dependencies.sh dmesg_output
python3 slim_text_section_by_offset_glibc.py
python3 ropgadget.py


