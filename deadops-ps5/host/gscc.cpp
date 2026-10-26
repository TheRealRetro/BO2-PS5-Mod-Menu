// Host-side driver for DeadOps' T6 GSC compiler: compiles a .gsc file the
// same way the console build does, so scripts can be checked on the PC.
#include <cstdio>
#include <cstdlib>
#include <vector>

#include "../source/compiler/gsc_compile.hpp"

int main(int argc, char** argv)
{
    if (argc < 3) {
        std::fprintf(stderr, "usage: gscc <asset name> <file.gsc> [out.gscbin]\n");
        return 2;
    }
    std::FILE* f = std::fopen(argv[2], "rb");
    if (!f) { std::perror(argv[2]); return 2; }
    std::vector<char> src;
    char buf[65536];
    for (size_t n; (n = std::fread(buf, 1, sizeof(buf), f)) > 0;)
        src.insert(src.end(), buf, buf + n);
    std::fclose(f);

    size_t outLen = 0;
    const void* obj = GSC_CompileT6(argv[1], src.data(), src.size(), &outLen);
    if (!obj) {
        std::fprintf(stderr, "compile FAILED: %s\n", GSC_CompileError());
        return 1;
    }
    std::printf("compiled %s: %zu bytes, magic %016llx\n", argv[1], outLen,
                (unsigned long long)*(const unsigned long long*)obj);
    if (argc > 3) {
        std::FILE* o = std::fopen(argv[3], "wb");
        if (!o) { std::perror(argv[3]); return 2; }
        std::fwrite(obj, 1, outLen, o);
        std::fclose(o);
    }
    GSC_CompileShutdown();
    return 0;
}
