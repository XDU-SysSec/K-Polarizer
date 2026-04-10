import subprocess
import sys

def run_ropgadget_and_get_count(binary, range_val, extra_args=[]):
    cmd = ["ROPgadget", "--binary", binary, "--range", range_val] + extra_args
    try:
        output = subprocess.check_output(cmd, stderr=subprocess.STDOUT, universal_newlines=True)
        for line in reversed(output.splitlines()):
            if line.startswith("Unique gadgets found:"):
                return int(line.split(":")[1].strip())
    except subprocess.CalledProcessError as e:
        print(f"[ERROR] Command failed: {' '.join(cmd)}")
        print(e.output)
    return None

def collect_gadget_data(binary, range_val):
    data = {}
    data["All"] = run_ropgadget_and_get_count(binary, range_val)
    data["NoSys"] = run_ropgadget_and_get_count(binary, range_val, ["--nosys"])
    data["NoROP"] = run_ropgadget_and_get_count(binary, range_val, ["--norop"])
    data["NoJOP"] = run_ropgadget_and_get_count(binary, range_val, ["--nojop"])

    if None not in data.values():
        data["SYS"] = data["All"] - data["NoSys"]
        data["ROP"] = data["All"] - data["NoROP"]
        data["JOP"] = data["All"] - data["NoJOP"]
    else:
        print(f"[ERROR] Missing data for binary {binary}")
    return data

def main():
    target_name = "libflac"
    range_val = "0x8898-0x52008"
    orig_bin = "libFLAC.so.12.0.0"
    slim_bin = "libFLAC_slimmed.so"
    worst_slim_bin = "libFLAC.so.worst_slimmed"


    print(f"=== Collecting data for original {target_name} binary ===")
    orig_data = collect_gadget_data(orig_bin, range_val)
    print(f"Original Gadget Counts ({target_name}): {orig_data}")

    print(f"\n=== Collecting data for slimmed {target_name} binary ===")
    slim_data = collect_gadget_data(slim_bin, range_val)
    print(f"Slimmed Gadget Counts ({target_name}): {slim_data}")


    print(f"\n=== Collecting data for worst slimmed {target_name} binary ===")
    worst_slim_data = collect_gadget_data(worst_slim_bin, range_val)
    print(f"Slimmed Gadget Counts ({target_name}): {worst_slim_data}")

    print(f"\n=== Gadget Reduction for {target_name} ===")
    for key in ["All", "SYS", "ROP", "JOP"]:
        if key in orig_data and key in slim_data:
            diff = orig_data[key] - slim_data[key]
            print(f"{key} reduced by: {diff}")
        else:
            print(f"[WARN] Missing {key} in data")

    print(f"\n=== Gadget Reduction for {target_name} (worst case) ===")
    for key in ["All", "SYS", "ROP", "JOP"]:
        if key in orig_data and key in worst_slim_data:
            diff = orig_data[key] - worst_slim_data[key]
            print(f"{key} reduced by: {diff}")
        else:
            print(f"[WARN] Missing {key} in data")

if __name__ == "__main__":
    main()

