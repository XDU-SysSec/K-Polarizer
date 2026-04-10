#! /usr/bin/python3

import sys

if __name__ == "__main__":
    offsets = set()

    benchmark = sys.argv[1]
    with open(benchmark + ".offsets") as fp:
        for line in fp.readlines():
            start = end = 0
            line.replace(" ", "")
            for i, ch in enumerate(line):
                if ch == '{':
                    start = i
                elif ch == '}':
                    end = i
                    fstart, fend = line[start + 1: end].split(',')
                    offsets.add( (int(fstart, 16), int(fend, 16)) )

    with open("../libc.so.rewritten", "rb") as fp:
        so_bin = fp.read()
        fp.seek(0)

        loaded_pages = set()
        loaded_pages.add(0x18)
        for op in offsets:
            fstart, fend = op
            fstart //= 0x1000
            fend = (fend - 1) // 0x1000
            for page in range(fstart, fend + 1):
                loaded_pages.add(page)

        new_text_bin = b''
        for page in range(0x18, 0x78):
            if page in loaded_pages:
                new_text_bin += so_bin[page * 0x1000: (page + 1) * 0x1000]
            else:
                new_text_bin += b'\x00' * 0x1000

        with open("libc.so." + benchmark, "wb") as fp2:
            fp2.write(so_bin[:0x18000] + new_text_bin + so_bin[0x78000:])

    total_size = 0
    for op in offsets:
        total_size += op[1] - op[0]
    # first .text has 0xc26 bytes of code
    print(total_size + 0xc26)
    # first .text has 54 functions
    print(len(offsets) + 54)
            


