#!/usr/bin/env bash
set -e


rm -rf build
mkdir build
cd build
cmake -G Ninja .. -DCMAKE_BUILD_TYPE=Release \
                  -DCMAKE_CXX_FLAGS="-static-libgcc -static-libstdc++ -fPIC" \
                  -DCMAKE_EXE_LINKER_FLAGS="-static-libgcc -static-libstdc++" \
                  -DCMAKE_SHARED_LINKER_FLAGS="-static-libgcc -static-libstdc++" 

ninja 
cd ..
cp build/bin/program-markers python_src/program_markers
cp setup.py.in setup.py
sed -i "s~THIS_DIR~$(pwd)~g" setup.py
python -m build
rm setup.py
