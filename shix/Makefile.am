# $Id$ 

noinst_LIBRARIES = libshix.a
libshix_a_SOURCES = libshix.c regerror.c regexp.c regsub.c

#ifdef USE_MYSQL
LDADD = $(MYSQL_LIB) -lmysqlclient
#endif

AM_CFLAGS =-O2 -Wall

AM_CPPFLAGS = -I../include

#ifdef USE_MYSQL
AM_CPPFLAGS += $(MYSQL_INCLUDE)
#endif

#CFLAGS += -pedantic -w -W \
#  -Wall -Waggregate-return -Wcast-align -Wcast-qual \
#  -Wcomment -Wconversion -Werror -Wformat -Wimplicit \
#  -Wimplicit-int -Wimplicit-function-declaration -Winline \
#  -Wmissing-prototypes -Wmissing-declarations \
#  -Wnested-externs -Wno-import -Wparentheses -Wpointer-arith \
#  -Wredundant-decls -Wreturn-type -Wshadow -Wstrict-prototypes \
#  -Wswitch
