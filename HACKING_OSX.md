# OSX

```
$ brew install automake libtool gettext
$ brew link --force gettext
$ ./autogen.sh
```

NOTE: Will not fully compile until epoll code is replaced with kqueue code.
