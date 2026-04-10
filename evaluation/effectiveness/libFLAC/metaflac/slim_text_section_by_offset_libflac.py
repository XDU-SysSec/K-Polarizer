import sys
from elftools.elf.elffile import ELFFile
import mmap
import shutil

def get_base_vaddr(lib_path):
    with open(lib_path, 'rb') as f:
        elf = ELFFile(f)
        for seg in elf.iter_segments():
            if seg['p_type'] == 'PT_LOAD':
                return seg['p_vaddr']
    raise RuntimeError("No PT_LOAD segment found")

def parse_offset_pairs(offset_file, base_vaddr):
    ranges = []
    with open(offset_file) as f:
        for line in f:
            line = line.strip().strip('()')
            if not line:
                continue
            start, end = line.split(',')
            start_vaddr = int(start, 16) + base_vaddr
            end_vaddr = int(end, 16) + base_vaddr
            ranges.append((start_vaddr, end_vaddr))  # offset + base_vaddr = vaddr
    return ranges

def offset_in_any_range(addr, ranges):
    return any(start <= addr < end for start, end in ranges)

def slim_all_text_sections(lib_path, offset_file, output_file):
    base_vaddr = get_base_vaddr(lib_path)
    print(f"[INFO] First PT_LOAD VirtAddr = 0x{base_vaddr:x}")
    
    offset_ranges = parse_offset_pairs(offset_file, base_vaddr)

    with open(lib_path, 'rb') as f:
        elf = ELFFile(f)
        with open(lib_path, 'rb') as f2:
            full_data = bytearray(f2.read())

        for section in elf.iter_sections():
            name = section.name
            if not name.startswith('.text'):
                continue

            sec_offset = section['sh_offset']
            sec_addr = section['sh_addr']
            sec_size = section['sh_size']

            print(f"[INFO] Section {name}: addr=0x{sec_addr:x}, offset=0x{sec_offset:x}, size={sec_size}")

            section_data = bytearray(full_data[sec_offset:sec_offset + sec_size])

            for i in range(sec_size):
                vaddr = sec_addr + i

                # retian all mandatory functions
                rel_offset = vaddr - base_vaddr
                if rel_offset < 0xe000 or rel_offset >= 0x52000:
                    continue

                if not offset_in_any_range(vaddr, offset_ranges):
                    section_data[i] = 0

            full_data[sec_offset:sec_offset + sec_size] = section_data

    with open(output_file, 'wb') as f_out:
        f_out.write(full_data)

def func_reduction(offsets):
    remained_num = len(offsets)
    original_num = 500 - 91
    mandatory_num = 27
    print("func reduction: ", (original_num - (remained_num + mandatory_num) ) / original_num)

def byte_reduction(offsets):
    total_length = 0
    for offset in offsets:
        total_length += offset[1] - offset[0]
    mandatory_length = 0xe000 - 0x9000
    original_length = 0x52000 - 0x9000
    print("code reduction: ", (original_length - (total_length + mandatory_length) ) / original_length)

# this functions just computes the bytes/functions reduction, as presented in Table 1/2 in our paper
def calc_code_reduction(lib_path, offset_file):
    base_vaddr = get_base_vaddr(lib_path)
    offsets = parse_offset_pairs(offset_file, base_vaddr)
    func_reduction(offsets)
    byte_reduction(offsets)

def gen_worst_file(orig_file, offset_file):
    base_vaddr = get_base_vaddr(orig_file)
    offset_ranges = parse_offset_pairs(offset_file, base_vaddr)

    worst_slimmed_file = "libFLAC.so.worst_slimmed"
    shutil.copyfile(orig_file, worst_slimmed_file)
    
    text_start_offset = 0xe000
    text_section_size = 0x52000 - 0xe000

    with open(worst_slimmed_file, 'rb+') as f:
        mmapped_file = mmap.mmap(f.fileno(), 0)
        original_content = bytearray(mmapped_file)

        for i in range(text_section_size):
            mmapped_file[text_start_offset + i] = 0

        for start, end in offset_ranges:
            ALIGNMENT = 0x1000
            aligned_start = (start // ALIGNMENT) * ALIGNMENT
            aligned_end = ((end + ALIGNMENT - 1) // ALIGNMENT) * ALIGNMENT

            func_content = original_content[aligned_start:aligned_end]
            mmapped_file[aligned_start:aligned_end] = func_content

        mmapped_file.flush()
        mmapped_file.close()


if __name__ == "__main__":
    if len(sys.argv) != 4:
        print("Usage: python slim_all_text_sections.py <libfile> <offset_pairs_file> <output_file>")
        sys.exit(1)
    slim_all_text_sections(sys.argv[1], sys.argv[2], sys.argv[3])
    calc_code_reduction(sys.argv[1], sys.argv[2])
    gen_worst_file(sys.argv[1], sys.argv[2])
