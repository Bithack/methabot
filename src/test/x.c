#include "../src/libmetha/mtrie.h"

int main(void)
{
    char out[64];
    unsigned char x;
    for (x=' '; x<128; x++)
        out[MTRIE_OFFS(x)] = x;
    printf("%s\n", out);
    return 0;
}
