#include "../src/libmetha/crawler.h"

char *test = 
    "<html>"
        "<body>"
            "<a href='http://test.com/test'></a>"
            "<textarea>hej</textarea>"
        "</body>"
    "</html>";

int main(void)
{
    crawler_t x;
    x.buf = test;
    x.buf_sz = strlen(test);
    lm_parse_html(&x, 0, 1);
    return 0;
}
