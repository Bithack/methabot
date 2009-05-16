
#include "../libmetha/mtrie.h"
#include "../libmetha/url.h"
#include <stdio.h>

const char *list[] = {
"1.gravatar.com/avatar/3b2e4f727eb92ab022d158e0d6dc27ee?s=32&d=http%3A%2F%2F1.gravatar.com%2Favatar%2Fad516503a11cd5ca435acc9bb6523536%3Fs%3D32AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAa"
"1.gravatar.com/avatar/bdbcccb1efc666478334defb4f00402b?s=32&d=http%3A%2F%2F1.gravatar.com%2Favatar%2Fad516503a11cd5ca435acc9bb6523536%3Fs%3D32",
"1.gravatar.com/avatar/bdbcccb1efc666478334defb4f00402b?s=32&d=http%3A%2F%2F1.gravatar.com%2Favatar%2Fad516503a11cd5ca435acc9bb6523536%3Fs%3D32",
"1.gravatar.com/avatar/f0daaa8c2eebc7b83c22c36cad1faad4?s=32&d=http%3A%2F%2F1.gravatar.com%2Favatar%2Fad516503a11cd5ca435acc9bb6523536%3Fs%3D32",
"1.gravatar.com/avatar/384448292a195e2c71f6d3deb5ee8f17?s=32&d=http%3A%2F%2F1.gravatar.com%2Favatar%2Fad516503a11cd5ca435acc9bb6523536%3Fs%3D32",
"1.gravatar.com/avatar/5bcf09e09c8bf7a7f372f4fa415be085?s=32&d=http%3A%2F%2F1.gravatar.com%2Favatar%2Fad516503a11cd5ca435acc9bb6523536%3Fs%3D32",
"1.gravatar.com/avatar/553e18aef69b5e714fc0752a3acd53d7?s=32&d=http%3A%2F%2F1.gravatar.com%2Favatar%2Fad516503a11cd5ca435acc9bb6523536%3Fs%3D32",
"1.gravatar.com/avatar/3b2e4f727eb92ab022d158e0d6dc27ee?s=32&d=http%3A%2F%2F1.gravatar.com%2Favatar%2Fad516503a11cd5ca435acc9bb6523536%3Fs%3D32"
};

int main(void)
{
    mtrie_t *m = mtrie_create();
    int x,y;

    url_t url;

    for (x=0; x<sizeof(list)/sizeof(char*); x++) {
        url.str = list[x];
        url.sz  = strlen(list[x]);
        url.host_o = 0;
        if (mtrie_tryadd(m, &url))
            printf("adding \"%s\":success\n", list[x]);
        else
            printf("adding \"%s\":failed\n", list[x]);
    }
    mtrie_destroy(m);
    return 0;
}

