#!/bin/bash

if [ $# -ne 1 ]; then
  echo "Usage: $0 <input_file>"
  exit 1
fi

input_file="$1"
unique_output_file="address_pairs_unique.txt"

mapfile -t unique_addr_pairs < <(
awk '
/^\[[ 0-9]+\.[0-9]+\] Dependency: \(0x[0-9a-f]+, 0x[0-9a-f]+\)$/ {
    if (match($0, /\(0x[0-9a-f]+, 0x[0-9a-f]+\)/)) {
        addr_pair = substr($0, RSTART, RLENGTH)
        if (!seen[addr_pair]++) {
            print addr_pair
        }
    }
}
' "$input_file"
)

printf "%s\n" "${unique_addr_pairs[@]}" > "$unique_output_file"

global_addr_pairs="${unique_addr_pairs[*]}"


export global_addr_pairs

echo "global_addr_pairs :"
echo "$global_addr_pairs"

