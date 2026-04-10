#!/bin/bash

echo "Running for libFLAC..."
./extract_dependencies.sh dmesg_output
python3 slim_text_section_by_offset_libflac.py libFLAC.so.12.0.0 address_pairs_unique.txt libFLAC_slimmed.so
python3 ropgadget.py libflac

