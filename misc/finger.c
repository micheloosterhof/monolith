/* $Id$ */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>

#include <mysql.h>

#include "monolith.h"
#include "libmono.h"

extern char *wholist(int level, const user_t * user);

#undef FINGER_DISABLED

int main(int, char *argv[] );

int
main(int argc, char *argv[] )
{
    char *p;

    set_invocation_name(argv[0]);
    mono_setuid("guest");
    chdir(BBSDIR);

#ifdef FINGER_DISABLED
    printf("We are sorry, but the finger service is temporarily disabled.\n");
    fflush(stdout);
    exit(0);
#endif

    mono_connect_shm();
    strremcol(p = wholist(1, NULL));
    mono_detach_shm();

    printf("%s", p);
    fflush(stdout);
    xfree(p);
    return 0;
}
