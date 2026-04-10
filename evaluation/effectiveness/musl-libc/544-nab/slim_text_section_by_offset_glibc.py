import mmap
import shutil
from elftools.elf.elffile import ELFFile

def parse_offset_pairs(offset_file):
    ranges = []
    with open(offset_file) as f:
        for line in f:
            line = line.strip().strip('()')
            if not line:
                continue
            start, end = line.split(',')
            start_off = int(start, 16)
            end_off = int(end, 16)
            ranges.append((start_off, end_off))
    return ranges

orig_file = 'libc.so.rewritten'
slimmed_file = 'libc.so.rewritten_slimmed'
worst_slimmed_file = "libc.so.worst_slimmed"

shutil.copyfile(orig_file, slimmed_file)

offset_ranges = parse_offset_pairs('address_pairs_unique.txt')

no_zero_offset = 0x18000
no_zero_size = 0x1000

# def func_reduction(offsets):
#     remained_num = len(offsets)
#     original_num = 2801 - 5
#     mandatory_num = 49
#     print("func reduction: ", (original_num - (remained_num + mandatory_num)) / original_num)

# def byte_reduction(offsets):
#     total_length = 0
#     for offset in offsets:
#         total_length += offset[1] - offset[0]
    
#     mandatory_length = 0x12cc08 - 0x12a000
#     original_length = 0x12cc08
#     print("code reduction: ", (original_length - (total_length + mandatory_length)) / original_length)

# # this function calculates the code reduction
# def calc_code_reduction(offsets):
#     func_reduction(offsets)
#     byte_reduction(offsets)

def gen_worst_file():
    shutil.copyfile(orig_file, worst_slimmed_file)

    with open(orig_file, 'rb') as f:
        elffile = ELFFile(f)
        text_section = elffile.get_section_by_name('.text')
        text_start_offset = text_section['sh_offset']
        text_section_size = text_section['sh_size']

    with open(worst_slimmed_file, 'rb+') as f:
        mmapped_file = mmap.mmap(f.fileno(), 0)

        original_text_content = bytearray(mmapped_file[text_start_offset:text_start_offset + text_section_size])

        for i in range(text_section_size):
            if (text_start_offset + i) < no_zero_offset + text_start_offset:
                mmapped_file[text_start_offset + i] = 0

        for start, end in offset_ranges:
            ALIGNMENT = 0x1000
            aligned_start = (start // ALIGNMENT) * ALIGNMENT
            aligned_end = ((end + ALIGNMENT - 1) // ALIGNMENT) * ALIGNMENT

            func_content = original_text_content[aligned_start:aligned_end]
            mmapped_file[text_start_offset + aligned_start:text_start_offset + aligned_end] = func_content

        mmapped_file.flush()
        mmapped_file.close()

# calc_code_reduction(offset_ranges)
# gen_worst_file()

with open(orig_file, 'rb') as f:
    elffile = ELFFile(f)
    text_start_offset = 0x18000
    text_section_size = 0x78000 - 0x18000

with open(slimmed_file, 'rb+') as f:
    mmapped_file = mmap.mmap(f.fileno(), 0)

    original_bin = bytearray(mmapped_file)

    for i in range(text_section_size):
        if (text_start_offset + i) >= text_start_offset + no_zero_size:
            mmapped_file[text_start_offset + i] = 0

    for start, end in offset_ranges:
        func_content = original_bin[start:end]
        mmapped_file[start:end] = func_content

    mmapped_file.flush()
    mmapped_file.close()

