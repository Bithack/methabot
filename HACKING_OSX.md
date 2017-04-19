# OSX

```
$ brew install automake libtool gettext
$ brew link --force gettext
$ ./autogen.sh
```

NOTE: Methabot will run in "synchronous" mode until epoll is replaced with kqueue
