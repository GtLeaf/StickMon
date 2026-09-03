// Host harness: encodes argv[1] with QrCodeGen and prints the module count
// per side followed by one line per row ('1' = dark module, '0' = light).
#include <cstdio>
#include <cstdlib>

#include "presentation/QrCodeGen.h"

int main(int argc, char** argv) {
    if (argc != 2) {
        std::fprintf(stderr, "usage: %s <payload>\n", argv[0]);
        return 2;
    }
    uint8_t modules[Stickmon::QrCodeGen::MAX_MATRIX_BYTES];
    const int size = Stickmon::QrCodeGen::encode(argv[1], modules,
                                                 sizeof(modules));
    std::printf("%d\n", size);
    for (int row = 0; row < size; ++row) {
        for (int column = 0; column < size; ++column) {
            std::putchar(modules[row * size + column] ? '1' : '0');
        }
        std::putchar('\n');
    }
    return size > 0 ? 0 : 1;
}
