mkdir -p build/terminal
cd  build/terminal
cmake ../.. -GNinja -DCMAKE_C_COMPILER_ID="Clang" -DCMAKE_CXX_COMPILER_ID="Clang"  
cmake --build . --parallel 8
