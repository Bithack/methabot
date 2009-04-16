bin_PROGRAMS = mb-slaved

AM_CFLAGS = -I/usr/include/mysql -I../libmetha/ -I../libmethaconfig/
AM_LDFLAGS = -lmysqlclient

mb_slaved_LDADD = ../libmethaconfig/libmethaconfig.la
mb_slaved_SOURCES = \
	main.c \
	client.c \
	../mb-masterd/server.c \
	master.c \
	client.h \
	slave.h

