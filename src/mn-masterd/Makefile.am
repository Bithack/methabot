bin_PROGRAMS = mn-masterd

mn_masterd_SOURCES = \
	auth.c \
	conn.h \
	conf.h \
	conf.c \
	slave-conn.c \
	user-conn.c \
	main.c \
	master.h \
	server.c \
	slave-class.c \
	slave.h \
	methanol.h \
	server.h

AM_CFLAGS = -I/usr/include/mysql -I../libmetha/ -I../libmethaconfig/
AM_LDFLAGS = -lmysqlclient

mn_masterd_LDADD = ../libmethaconfig/libmethaconfig.la
