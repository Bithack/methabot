# Methabot Official Revival

We're reviving the classic super-fast web crawler Methabot. We'll clean stuff up and make available a simple and functional web crawler.

## Custom modules

See src/modules/example.c, build to a shared library:

```
$ gcc -std=gnu99 -fPIC -c example.c -o example.o
$ gcc example.o -shared -o example.so
```

Create a new config file "custom.conf":

```
include "default.conf"
load_module "/absolute/path/to/example.so"

filetype ["image"] {
    parser = "example";
}
```
