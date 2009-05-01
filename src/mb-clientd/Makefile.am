bin_PROGRAMS=mb-clientd

mb_clientd_SOURCES = client.h \
					 ../mn-masterd/daemon.c \
					 ../mn-masterd/daemon.h \
					 slave.c \
					 main.c

AM_CFLAGS = -I../libmethaconfig/ -I../libmetha/

mb_clientd_LDADD = ../libmetha/libmetha.la ../libmethaconfig/libmethaconfig.la
