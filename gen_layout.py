import re
import copy
from elftools.elf.elffile import ELFFile
# from elftools.elf.relocation import RelocationSection
# from elftools.elf.sections import SymbolTableSection

func_to_range_map = dict()
duplicate_funcs = set()
llvm_opt_libc_funcs = set()

# assume number of matched items == 1
def re_matches(pattern, str):
    res = re.findall(pattern, str)
    if len(res) == 1:
        return res[0]
    elif len(res) == 0:
        return None
    else:
        # print "illegal length of re matches"
        exit()

def process_duplicate_funcs():
    clusters = []
    with open('disjoints') as fp:
        cls = []
        for line in fp.readlines():
            for func in line.split('\t'):
                if func[-1] == '\n':
                    func = func[:-1]
                cls.append(func)
            clusters.append(cls)
    
    for cls in clusters:
        for idx, func in enumerate(cls):
            if '.' in func:
                cls[idx] = func[ :func.index('.') ]

# TODO in the binary multiple functions might share the same name, we have to consider this situation
def init_func_length():
    global func_to_range_map
    global duplicate_funcs
    global f2r_map_dup

    # get start address of .text
    fp_elf = open('libFLAC.so', 'rb')
    elf = ELFFile(fp_elf)
    text = elf.get_section_by_name('.text')
    text_va = text['sh_addr']
    fini = elf.get_section_by_name('.fini')
    fini_va = fini['sh_addr']

    prev_func_addr = 0
    prev_func_name = ''
    with open('libFLAC.so.dump') as fp:
        for line in fp.readlines():
            addr_pattern = r"([0-f]+) <[^>]+>:"
            func_addr = re_matches(addr_pattern, line)
            if func_addr is None:
                continue
            
            func_addr = int(func_addr, 16)
            if text_va <= func_addr < fini_va:
                fname_pattern = r"<([^>]+)>"
                fname = re_matches(fname_pattern, line)

                # preprocess fname
                if '.' in fname:
                    fname = fname[:fname.index('.')]

                if prev_func_name in func_to_range_map:
                    print ("warning, detecting multiple definition of: ", prev_func_name)
                    duplicate_funcs.add(prev_func_name)

                else:
                    func_to_range_map[prev_func_name] = (prev_func_addr, func_addr)
                prev_func_addr = func_addr
                prev_func_name = fname

    # remove the empty entry
    del func_to_range_map['']

    # do not forget the last function in .text
    func_to_range_map[prev_func_name] = (prev_func_addr, fini_va)

    # keep func_to_range_map a 1-to-1 mapping
    for func in duplicate_funcs:
        del func_to_range_map[func]

    return func_to_range_map 

def bin_IR_diff():
    global func_to_range_map
    # init IR funcs
    IR_funcs = set()
    # with open("deps") as fp:
    #     for line in fp.readlines():
    #         for func in line.split('\t'):
    #             if len(func) <= 1:
    #                 continue
    #             if func[-1] == '\n':
    #                 func = func[:-1]
    #             IR_funcs.add(func)
    with open("all_funcs") as fp:
        for line in fp.readlines():
            func = line[:-1]
            if '.' in func:
                func = func[ :func.index('.') ]
            IR_funcs.add(func)

    # init bin funcs
    bin_funcs = set()
    for func, range in func_to_range_map.items():
        if len(func) <= 1:
            print(func, [hex(x) for x in range])
        bin_funcs.add(func)

    print( 'IR - bin: ')
    print( IR_funcs - bin_funcs )

    print('bin - IR:')
    print( bin_funcs - IR_funcs )


# merge small clusters
def merge_small(disjoints):
    global func_to_range_map
    global duplicate_funcs
    global llvm_opt_libc_funcs

    disjoints = []
    funcs_disjoints = set()

    # init disjoints, and also some preprocess of fnames...
    with open("disjoints") as fp:
        for line in fp.readlines():
            cls = []
            for func in line.split('\t'):
                if '\n' in func:
                    func = func[:-1]
                if '\t' in func:
                    func = func[:-1]
                if len(func) == 0:
                    continue
                if '.' in func:
                    func = func[:func.index('.')]

                if func in duplicate_funcs:
                    continue

                funcs_disjoints.add(func)
                cls.append(func)
            disjoints.append(cls)


    merged_clusters = []
    previous_size = 0
    previous_cluster = []
    for cls in disjoints:
        current_cluster = []
        current_size = 0
        for func in cls:
            if func in func_to_range_map:
                frange = func_to_range_map[func]
                current_size += frange[1] - frange[0]
                current_cluster.append(func)
            # duplicate funcs? dead funcs? external funcs (e.g. __errno_location)?
            else:
                print("seems like we found a llvm optimized libc func", func)

        if current_size > 0x2000:
            merged_clusters.append(current_cluster)
            continue

        # FIXME: can we use 0x1000?
        if previous_size + current_size <= 0x2000:
            previous_cluster.extend(current_cluster)
            previous_size += current_size
        else:
            if len(previous_cluster) != 0:
                merged_clusters.append(previous_cluster)
                # merged_clusters.append(current_cluster)
            previous_cluster = current_cluster
            previous_size = current_size
            
    if len(previous_cluster) != 0:
        merged_clusters.append(previous_cluster)

    funcs_merged = set()
    with open('merged', 'w') as fp:
        for cls in merged_clusters:
            for func in cls:
                if func in duplicate_funcs:
                    continue
                funcs_merged.add(func)
                fp.write(func + '\t')
            fp.write('\n')

    llvm_opt_libc_funcs = funcs_disjoints - funcs_merged
    print( "llvm_opt_libc_funcs = ", llvm_opt_libc_funcs)

    return merged_clusters

def pre_process_deps():
    global llvm_opt_libc_funcs

    dep_map = dict()
    with open("deps") as fp:
        for line in fp.readlines():
            flist = []
            for func in line.split('\t'):
                if '\n' in func:
                    func = func[:-1]
                if '\t' in func:
                    func = func[:-1]
                if len(func) == 0:
                    continue
                if '.' in func:
                    func = func[:func.index('.')]
                flist.append(func)
            API_func = flist[0]

            # hacky, but LLVM optimization makes these external functions internal
            if API_func in llvm_opt_libc_funcs:
                continue
            dep_map[API_func] = (x for x in flist[1:] if x not in llvm_opt_libc_funcs)

    with open("dep.name", "w") as fp:
        for api, deps in dep_map.items():
            fp.write(api)
            for dep_func in deps:
                fp.write('\t' + dep_func)
            fp.write('\n')

def dump_duplicate_funcs():
    global duplicate_funcs

    with open("duplicate_funcs", "w") as fp:
        for dfunc in duplicate_funcs:
            fp.write(dfunc + '\n');        

if __name__ == '__main__':
    # these are default passes!
    init_func_length()
    merge_small([])
    pre_process_deps()
    dump_duplicate_funcs()

    # for cls in clss:
    #     csz = 0
    #     for func in cls:
    #         csz += func_to_range_map[func][1] - func_to_range_map[func][0]
    #     print(hex(csz))


    # process_duplicate_funcs()
    # bin_IR_diff()
    # AIO()
    # collect_dead_functions('libFLAC.so')
    # collect_werid_funcs()


    
