/* $Id$
 */
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <mysql.h>

#include "monolith.h"
#include "libmono.h"
#include "routines2.h"
#include "sql_forum.h"
#include "sql_message.h"
#include "ext.h"
#include "input.h"
#include "display_message.h"

static int get_alias(room_t *quad, message_t *message);
static int get_subject(message_t *message);
static int get_content(message_t *message);
static int fill_buffer(char **content);

/*
 * 	0: normal new message.
 *	1: reply to message.
 *	2: 
 */

int
enter_message(int flag, unsigned int forum)
{
    room_t *quad;
    message_t *message;

    quad = (room_t *)xcalloc( 1, sizeof(room_t) );

    (void) mono_sql_f_read_quad(forum, quad);

    /* if( (!may_post(usersupp, quad)) ) {
     *     xfree(quad);
     *     return -1;
     * } 
     */

    message = (message_t *)xcalloc( 1, sizeof(message_t) );
    
    message->forum = forum;
    message->author = usersupp->usernum;
    message->content = NULL;

    (void) get_alias(quad, message);

    /*
     * Display just the username/alias. The rest we can format through
     * proper input selection. Nice idea, coz quite often when the BBS
     * lags, it takes forever to re-display the header.
     */
    cprintf("\n");
    (void) display_header(message);

    /*
     * Get the subject line.
     */
    (void) get_subject(message);

    switch( get_content(message)) {

        case -1: /* Empty post */
            cprintf("\1f\1rWe don't save empty %s!\n", config.message_pl);
            xfree(message->content);
            xfree(message);
            xfree(quad);
            return -1;

        case -2: /* <A>bort */
            cprintf("\1f\1rAborting yer %s eh!\n", config.message);
            xfree(message->content);
            xfree(message);
            xfree(quad);
            return -1;

        default:
            break;
            
    }

    if( (mono_sql_mes_add(message, forum)) == -1)
        cprintf("\1f\1rCould not save your %s!\n", config.message);
    else
        cprintf("\1f\1gSaved.\n");

    xfree(message->content);
    xfree(message);
    xfree(quad);

    return 0;
}

static int
get_alias(room_t *quad, message_t *message)
{

    if((quad->flags & QR_ALIASNAME) || (quad->flags & QR_ANON2)) {
        cprintf("\1f\1gDo you want to add an aliasname to this %s? \1w(\1gY\1w/\1gn\1w) \1c", config.message);
        if (yesno_default(YES) == YES) {
            if ((usersupp->config_flags & CO_USEALIAS) && (strlen(usersupp->alias))) {
                sprintf(message->alias, usersupp->alias);
            } else {
                do {
                    cprintf("\1f\1bAlias\1w: \1c");
                    getline(message->alias, L_USERNAME, 1);
                } while ( (check_user(message->alias) == TRUE) && (!(EQ(message->alias, usersupp->username))) );
            }
            if(strlen(message->alias))
                message->type = MES_AN2;
            else {
                message->type = MES_ANON;
            }
        }
    }
    if( !(strlen(message->alias)))
        strcpy(message->alias, "");

    return 0;
}

/*
 * Format this exacly like when displaying a message.
 */
static int
get_subject(message_t *message)
{
    cprintf("\n\1f\1gSubject\1w: \1y");
    getline(message->subject, L_SUBJECT-1, 1);

    if(!(strlen(message->subject)))
        strcpy(message->subject, "" );

    return 0;
}

#define POST_LINE_LENGTH	200

static int
get_content(message_t *message)
{
    FILE *fp;
    int lines = 0;

    fp = xfopen(temp, "a", TRUE);

    if( usersupp->config_flags & CO_NEATMESSAGES )
        cprintf("\n\1a\1c");

    switch(get_buffer(fp, 1, &lines)) {

        case 's':
        case 'S':
            fclose(fp);
            return fill_buffer(&message->content);
            break;

        case 'a':
        case 'A':
        default:
            fclose(fp);
            unlink(temp);
            return -2;
    }

    return 0;
}

static int
fill_buffer(char **content)
{

     char *post = NULL;
     int fd = -1;
     struct stat buf;

     /*
      * Open temp file.
      */
     if( (fd = open(temp, O_RDONLY)) == -1) {
         log_it("sqlpost", "Can't open() temp file %s!", temp);
         return -1;
     }

     /*
      * Determine file size.
      */
     fstat(fd, &buf);

     /*
      * mmap() tempfile.
      */ 
     if( (post = mmap(post, buf.st_size, PROT_READ, MAP_PRIVATE, fd, 0)) == -1 ) { 
         log_it("sqlpost", "Can't mmap() temp file %s!", temp);
         return -1;
     }
     

     /*
      * Reserve memory for the post content.
      */
     *content = (char*)xcalloc(1, sizeof(post));
     if( *content == NULL ) {
         log_it("sqlpost", "Unable to malloc() message content.");
         return -1;
     }

     cprintf("\n\nContent: %s\n\n", post); fflush(stdout);
     /*
      * Copy post content into mem.
      */
     if( (*content = memcpy(*content, post, sizeof(post))) == NULL) {
         log_it("sqlpost", "memcpy() of message content failed!");
         return -1;
     }
     /*
      * The Terminator; he said he'd be back!
      */
#ifdef SHIT
     *content[ strlen(*content)-1 ] = '\0';
#else
     strcat(*content, "\0");
#endif

     /*
      * And close, destroy, kill etc.
      */
     if( (munmap(post, buf.st_size)) == -1) {
         log_it("sqlpost", "Can't munmap() temp file %s!", temp);
     }
     close(fd);
     unlink(temp);

     return 0;
}