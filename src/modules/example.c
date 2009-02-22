/** 
 * Example module
 **/

#include "../libmetha/module.h"

static M_CODE example_init();
static M_CODE example_uninit();
static M_CODE example_parser(iohandle_t *io_h, uehandle_t *ue_h, url_t *url);

PROPERTIES = {
    .name      = "Example Module",
    .version   = "1.0.0",
    .on_load   = &example_init,
    .on_unload = &example_uninit,

    .num_parsers = 1,
    .parsers = {
        LM_DEFINE_PARSER("example", &example_parser),
    },
};

static M_CODE
example_init()
{
    lm_message("Example Module 1.0.0 Initialized");
    return M_OK;
}

static M_CODE
example_uninit()
{
    lm_message("Example Module 1.0.0 ");
    return M_OK;
}

/** 
 * This example parser will look for http-URLs in plain text.
 * It will separate urls from other text by whitespaces, so it 
 * is not very fault-tolerant, just an example of how one should 
 * build a module with a parser.
 **/
static M_CODE
example_parser(iohandle_t *io_h, uehandle_t *ue_h, url_t *url)
{
    /** 
     * Download the given URL
     **/
    if (lm_io_get(io_h, url) == M_OK) {
        char *p = io_h->buf.ptr;
        char *e = p+io_h->buf.sz;
        char *s;

        for (;p<e;p++) {
            if (strncasecmp(p, "http://", 7) == 0) {
                /* find the end of the url */
                for (s=p;s<e;s++) {
                    if (isspace(*s)) {
                        /* we found the end, add it to the urlengine */
                        ue_add(ue_h, p, s-p);
                        p=s;
                        break;
                    }
                }
            }
        }
    }

    return M_OK;
}

