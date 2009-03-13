bin_PROGRAMS = mb-masterd

mb_masterd_SOURCES = \
	conn.c \
	conn.h \
	conf.h \
	conf.c \
	main.c \
	master.h

AM_CFLAGS = -I/usr/include/mysql -I../libmetha/ -I../libmethaconfig/
AM_LDFLAGS = -lmysqlclient

mb_masterd_LDADD = ../libmethaconfig/libmethaconfig.la
