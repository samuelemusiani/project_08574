#!/bin/bash

FILE="./kernel.core.uriscv"

tmp1=$(cat "$FILE" | head -c $((2 *4096)) | tail -c 4096 | head -c 32 | tail -c 4 | od -t x | head -n 1 | awk '{print $2}')

tmp2=$(cat "$FILE" | head -c $((2 *4096)) | tail -c 4096 | head -c 44 | tail -c 4 | od -t x | head -n 1 | awk '{print $2}')

echo "$tmp1" "$tmp2" | dc -e '16o16i?+p'
