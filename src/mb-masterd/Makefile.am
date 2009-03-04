bin_PROGRAMS = mb-masterd

mb_masterd_SOURCES = \
	conn.c \
	conn.h \
	main.c \
	master.h

AM_CFLAGS = -I/usr/include/mysql
AM_LDFLAGS = -lmysqlclient

mb_masterd_LDADD = ../libmetha/libmetha.la
