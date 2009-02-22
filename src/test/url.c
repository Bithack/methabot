#include <string.h>

#include "../src/libmetha/url.h"

/*char *test = "http://test.com/               /dsa/../t\nest. htm?aaa=b#x";*/
char *test = "http://img.4chan.org/b/res/";
char *test2 = "../imgboard.html";

int main(void)
{
    url_t url;
    url_t url2;
    lm_url_init(&url2);
    lm_url_init(&url);
    lm_url_set(&url, test, strlen(test));
    lm_url_dump(&url);
    lm_url_combine(&url2, &url, test2, strlen(test2));
    lm_url_dump(&url2);
    lm_url_uninit(&url);
    lm_url_uninit(&url2);
}
