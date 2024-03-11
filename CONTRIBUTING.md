Not much too it.

- Use the .clang-format to avoid format wars.
- Keep as many implementations in .cpp files as possible



Configured work for mac:

- Install brew LLVM, don't use macClang as c20 standards are not FULLY supported
- Export LLVM compiler
- Configured config.cpp to prevent buffer overflow on mac
'''
export CC=/opt/homebrew/opt/llvm/bin/clang
export CXX=/opt/homebrew/opt/llvm/bin/clang++
'''