confdir = $(datadir)/metha
conf_DATA = \
		conf/default.conf \
		conf/video.conf \
		conf/audio.conf \
		conf/image.conf \
		conf/archive.conf \
		conf/google.conf \
		conf/meta.conf \
		conf/title.conf \
		conf/ftp.conf \
		conf/validate.conf \
		conf/wikipedia.conf \
		conf/xmlsource.conf \
		conf/source.conf \
		conf/youtube.conf \
		conf/css.conf \
		conf/feed.conf


scriptsdir = $(datadir)/metha/scripts
scripts_DATA = \
		scripts/html.js \
		scripts/google.js \
		scripts/youtube.js \
		scripts/misc.js \
		scripts/validate.js \
		scripts/wikipedia.js

EXTRA_DIST = $(conf_DATA) $(scripts_DATA) \
			 src/modules/lmm_mysql/js.c \
			 src/modules/lmm_mysql/lmm_mysql.c \
			 src/modules/lmm_mysql/js.h

SUBDIRS = \
	src

