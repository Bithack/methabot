#include "../src/libmetha/metha.h"

const char *hej[] = {
    "http://microsoft.se/",
    "http://microsoft.se/xdsa",
    "http://microsoft.se/sddsuia",
};
    
int
main(int argc, char **argv)
{
    char test[256];
    metha_t m;
    io_t io;

    lm_init_io(&io, &m);
    io.verbose = 1;
    lm_iothr_launch(&io);
    iohandle_t *handle = lm_iohandle_obtain(&io);

    int x;
    for (x=0;x<sizeof(hej)/sizeof(char*); x++) {
        url_t u;
        u.str = hej[x];
        lm_multipeek_add(handle, &u);
    }

/*    while (fgets(test, 256, stdin) != 0) {
        test[strlen(test)-1] = '\0';
        lm_multipeek_add(handle, test);
    }
    */
    CURL *done;
    while (done = lm_multipeek_wait(handle)) {
        char *type;
        curl_easy_getinfo(done, CURLINFO_CONTENT_TYPE, &type);
        printf("Done: %s\n", type);
    }
    lm_iothr_stop(&io);
    lm_uninit_io(&io);
    printf("exit\n");
    return 0;
}

