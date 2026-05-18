#!/usr/bin/env bash
set -e
cmake -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
cmake --build build
ln -sf build/compile_commands.json compile_commands.json
echo "Built ./build/hospital"
