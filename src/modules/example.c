#include <stdio.h>
#include <stdint.h>
#include <metha/module.h>

#undef PACKAGE_NAME
#undef PACKAGE_VERSION

#define PACKAGE_NAME "example"
#define PACKAGE_VERSION "0.0.0"

#define LOGI(X, ...) fprintf(stdout, PACKAGE_NAME "-" PACKAGE_VERSION ": " X, ## __VA_ARGS__)

static M_CODE init();
static M_CODE uninit();
static M_CODE example_parser(struct worker *w, struct iobuf *buf,
                             struct uehandle *ue_h, struct url *url,
                             struct attr_list *al);

lm_mod_properties = {
    .name      = PACKAGE_NAME,
    .version   = PACKAGE_VERSION,
    .init      = &init,
    .uninit    = &uninit,

    .num_parsers = 1,
    .parsers = {
        {
            .name = "example",
            .type = LM_WFUNCTION_TYPE_NATIVE,
            .purpose = LM_WFUNCTION_PURPOSE_PARSER,
            .fn.native_parser = &example_parser,
        },
    },
};

static M_CODE init()
{
    LOGI("initialized");
    return M_OK;
}

static M_CODE uninit()
{
    LOGI("uninit");
    return M_OK;
}

static M_CODE example_parser(struct worker *w, struct iobuf *buf,
                             struct uehandle *ue_h, struct url *url,
                             struct attr_list *al)
{
    return M_OK;
}

