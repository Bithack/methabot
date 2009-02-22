
#include "../src/libmetha/metha.h"

int
main(int argc, char **argv)
{
    if (argc < 2)
        exit(1);
    metha_t *m = lm_create();
    lm_feed_binary(m, argv[1]);
    lm_destroy(m);
    return 0;
}
