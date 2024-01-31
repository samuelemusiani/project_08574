#!/bin/bash

SRC_FILES=$(find phase1/ phase2/ -type f \( -iname \*.c -o -iname \*.h \))

echo "Fomatting files in src/ :"
for file in $SRC_FILES;
do 
  echo "Formatting $file"
  clang-format -i "$file";
done
