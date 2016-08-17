AM_CFLAGS = ${shell pkg-config libmicrohttpd glib-2.0 --cflags}

bin_PROGRAMS = simplehttpd

simplehttpd_SOURCES = simplehttpd.c

simplehttpd_LDADD = -lmicrohttpd -lglib-2.0
