
noinst_LIBRARIES = libyawc.a

libyawc_a_SOURCES = btmp.c libcache.c \
  libfriends.c libquad.c libuid.c log.c msg_file.c sql_express.o \
  routines.c userfile.c sql_utils.c sql_user.c sql_userforum.c sql_useruser.c \
  sql_mail.c sql_topic.c sql_config.c sql_forum.c sql_goto.c sql_login.c \
  sql_usertopic.c sql_message.c sql_rating.c sql_llist.c sql_convert.c \
  sql_online.c sql_web.c sql_forumtopic.c

AM_CFLAGS = -Wall

#ifdef USE_MYSQL
AM_CPPFLAGS = $(MYSQL_INCLUDE)
#endif

#-w -W -pedantic \
#  -Wall -Waggregate-return -Wcast-align -Wcast-qual \
#  -Wcomment -Wconversion -Werror -Wformat -Wimplicit \
#  -Wimplicit-int -Wimplicit-function-declaration -Winline \
#  -Wmissing-prototypes -Wmissing-declarations \
#  -Wnested-externs -Wno-import -Wparentheses -Wpointer-arith \
#  -Wredundant-decls -Wreturn-type -Wshadow -Wstrict-prototypes \
#  -Wswitch

