/* $Id$ */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <build-defs.h>

#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <time.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <unistd.h>

#ifdef ENABLE_NLS
#include <libintl.h>
#include <locale.h>
#define _(String) gettext (String)
#else
#define _(String) (String)
#endif

#include "monolith.h"
#include "libmono.h"
#include "ext.h"
#include "telnet.h"

#include "chat.h"
#include "enter_message.h"
#include "express.h"
#include "input.h"
#include "friends.h"
#include "messages.h"
#include "read_menu.h"
#include "uadmin.h"
#include "rooms.h"
#include "routines2.h"
#include "statusbar.h"
#include "usertools.h"

static char x_default[L_USERNAME + 1];
static char web_default[L_USERNAME + 1];
static char chat_default[L_USERNAME + 1];
static char *mySysGuide = NULL;

static void catchx(int key);
static void format_ack(express_t * Catchxs);

/**********************************************************/

void
single_catchx(int sig)
{
    sig++;			/* to remove warning */
    signal(SIGIO, single_catchx);	/* reset signal */
    catchx(0);
}

/**********************************************************/

void
multi_catchx(int sig)
{
    sig++;			/* to remove warning */
    signal(SIGUSR1, multi_catchx);
    catchx(9);
}

/**********************************************************/
/* sendx()
 *   to                   : receiving user,
 *   send_string[]        : message,
 *   override             : override symbol
 */

void
sendx(char *to, const char *send_string, char override)
{

    x_str *Sendxs = NULL, xmessage;
    int channel = -1, i, timeout;
    btmp_t *tuser = NULL, *p;
    unsigned int to_id = 0;

    nox = 1;
    /* important: read the btmp structure as soon as possible, because the
     * who-list might change, while this function is running */

/* who to send it to */
    if (IS_BROADCAST)
	Sendxs = &(shm->broadcast);
    else if (IS_WEB) {
        (void) mono_cached_sql_u_name2id(to, &to_id);
#ifdef PORT
        if (mono_sql_web_send_x(usersupp->usernum, to_id, send_string, "telnet"))
#else
        if (mono_sql_web_send_x(usersupp->usernum, to_id, send_string, "client"))
#endif
	    cprintf(_("\1f\1gSuccessfully saved \1pWeb\1g %s %s for \1y%s.\1a\n"), config.express, config.x_message, to);
        else
	    cprintf(_("\1f\1rUnable to save \1pWeb\1g %s %s for \1y%s to database.\1a\n"), config.express, config.x_message, to);
    } else {
	Sendxs = mono_find_xslot(to);
	tuser = mono_read_btmp(to);
	if (Sendxs == NULL || tuser == NULL) {
	    char * save_x, * save_to;

	    save_x = (char *) xmalloc(strlen(send_string) + 1);
	    save_to = (char *) xmalloc(strlen(to) + 1);

	    strcpy(save_to, to);
	    strcpy(save_x, send_string);

	    cprintf(_("\1f\1y%s \1glogged off before you finished typing.\1a\n"), to);
            /* ask user if perhaps mail to the user that logged off... */
            x_message_to_mail(save_x, save_to);
	    xfree(save_to);
	    xfree(save_x);
	    return;
	}
    }

/* make the xmessage structure */
    memset( &xmessage, 0, sizeof( xmessage ) );
    xmessage.override = override;
    xmessage.ack = NO_ACK;
    if (tuser != NULL)
	strcpy(xmessage.recipient, tuser->username);
    else if (override == OR_CHAT || IS_WEB)
	strcpy(xmessage.recipient, to);
    else
	strcpy(xmessage.recipient, "Everyone");
    xmessage.sender_priv = usersupp->priv;
    strcpy(xmessage.sender, usersupp->username);
    strcpy(xmessage.message, send_string);
    time(&xmessage.time);

    /* Very dirty hack to store Web Xes to xlog. */
    if( IS_WEB ) {
        Sendxs = (x_str *) xmalloc( sizeof(x_str) );
        memcpy(Sendxs, &xmessage, sizeof(xmessage));
	add_x_to_personal_xlog(SENDX, Sendxs, override);
        xfree( Sendxs );
        return;
    }

/* wait until memory is not locked */
    if ((Sendxs->override != OR_FREE) && (!IS_BROADCAST))
	for (timeout = 0; timeout < 20; timeout++) {
	    usleep(100000);
	    fflush(stdout);
	    if (Sendxs->override == OR_FREE)
		break;
	}
/* copy message to destination */
    memcpy(Sendxs, &xmessage, sizeof(xmessage));
    if (override == OR_CHAT) {
	for (channel = -1, i = 0; i < MAXCHATROOMS; i++) {
	    if (EQ(to, shm->holodeck[i].name)) {
		channel = i;
		break;
	    }
	}
    }
/* send signal */
    if (IS_BROADCAST) {
	i = shm->first;
	while (i != -1) {
	    p = &(shm->wholist[i]);
	    if (strcasecmp(p->username, usersupp->username) != 0) {
		if (override == OR_SILC) {
		    IFSYSOP
			(void) kill(p->pid, SIGUSR1);
		} else if (override == OR_CHAT) {
		    if (p->chat & 1 << (channel))
			(void) kill(p->pid, SIGUSR1);
		} else
		    (void) kill(p->pid, SIGUSR1);
	    }
	    i = p->next;
	}
    } else
	(void) kill(tuser->pid, SIGIO);

/* wait for ack */
    if (!IS_BROADCAST) {	/* no ack for broadcasts */
	for (timeout = 0; timeout < 20; timeout++) {
	    usleep(100000);
	    if (Sendxs->ack != NO_ACK) {
		format_ack(Sendxs);
		break;
	    }
	}

	if (Sendxs->ack == NO_ACK) {
	    if (mono_return_pid(to) == -1)
		cprintf(_("\1f\1y%s \1rlogged off before you finished typing, sorry.\1a\n")
			,tuser->username);
	    else if (override == OR_PING)
		cprintf(_("\1f\1rPING to %s timed out waiting for ACK.\1a\n"),
			tuser->username);
	    else
		cprintf(_("\1f\1g%s %s received by \1y%s\1g.\1a\n"), config.express
			,config.x_message, tuser->username);
	}
    }
    if (tuser != NULL)
	xfree(tuser);

/* put in personal x log */


    switch (override) {
	    /* don't log these */
	case OR_LOGIN:
	case OR_LOGOUT:
	case OR_KICKOFF:
	case OR_PING:
	    break;
	default:
	    add_x_to_personal_xlog(SENDX, Sendxs, override);
	    break;
    }
}

/**************************************************************************/
/* add_x_to_personal_xlog()
 *
 * Why do we need to know which is the calling function?  because sendx()
 * is not aware of happenings while it was running. user may have taken 15
 * minutes to type an x, and meanwhile 6 x's arrived..  so if the x is put at 
 * the end of the buffer, [XLIMIT - 1] bad things are going to happen on
 * display of those x's. 
 * Instead, it's inserted just before any x's that arrived meanwhile.
 *
 * NOTE: the override type for the x is set to OR_FREE during the sending of the
 * message, and must be reset to whatever it was before the ACK, or the message
 * won't display properly in the old_express() buffer. The override can't be
 * restored prior to the call to this function and passed in via Sendxs
 * (locally xmessagePtr) or unlocking of memory for next x to this user will
 * timeout because OR_FREE has been overwritten in the xmessage pointed to by 
 * Sendxs (xmessagePtr, locally), as this is the same entity in shm used by
 * mono_find_xslot.  */

void
add_x_to_personal_xlog(char calling_function, x_str * xmessagePtr, char tempoverride)
{
    int i, insert_here;

/* error checking */

    if ( strcmp(xmessagePtr->sender, usersupp->username) && 
	 strcmp(xmessagePtr->recipient, usersupp->username) &&
         strcmp(xmessagePtr->recipient, "Everyone") &&
         tempoverride != 'N') 
        return;

    if (xmsgb[0])
	xfree(xmsgb[0]);

    insert_here = ((calling_function == SENDX) ? xmsgp - 1 : XLIMIT - 1);

    for (i = 1; i <= insert_here; i++)
	xmsgb[i - 1] = xmsgb[i];

    xmsgb[insert_here] = (express_t *) xcalloc(1, sizeof(x_str));
    memcpy(xmsgb[insert_here], xmessagePtr, sizeof(x_str));

    if (calling_function == CATCHX)
	xmsgp--;

    xmsgb[insert_here]->override = tempoverride;
    usersupp->x_s++;
    mono_sql_u_increase_x_count( usersupp->usernum );

    return;
}

/*******************************************************************/
/* catchx( int key )
 * processes a received x message.
 * key is the shmkey of the x message
 */
static void
catchx(int key)
{
    express_t *Catchxs, *xmsg;
    int friendb, is_my_vnemy = 0;
    char override;
    char tempstr[100];

    if (!shm)
	return;			/* no shared mem, just ignore */

    if (key == 9)
	Catchxs = &(shm->broadcast);
    else
	Catchxs = mono_find_xslot(who_am_i(NULL));

    override = Catchxs->override;

    if (override == OR_SILC && (cmdflags & C_NOSILC)) {
        IFSYSOP
	    cprintf("\1f\1r\nSilc Debug: Silc disable flag found, skipping\n");
	return;
    }
    if (override == OR_SILC && (usersupp->priv < PRIV_SYSOP)) {
	IFSYSOP
	    cprintf("\n\1r\1fSilc Error!\n\1a");
	return;
    }

/* error checking */
    if (!IS_BROADCAST && !EQ(Catchxs->recipient, usersupp->username)) {
	/* EEP!! */
	Catchxs->ack = ACK_OOPS;
	Catchxs->override = OR_FREE;
	return;
    }
    if (Catchxs->override == OR_PING) {
	Catchxs->ack = nox ? ACK_PING_BUSY : ACK_NOTBUSY;
	Catchxs->override = OR_FREE;
	return;
    }
/* friend logon/off notify */
    friendb = is_cached_friend(Catchxs->sender);
    if (Catchxs->override == OR_LOGIN || Catchxs->override == OR_LOGOUT
	|| Catchxs->override == OR_KICKOFF) {
	if (!((usersupp->flags & US_NOTIFY_ALL) ||
	      (friendb && (usersupp->flags & US_NOTIFY_FR))))
	    return;
    }
/* sender is enemy? */

    is_my_vnemy = is_cached_enemy(Catchxs->sender);

    if (!(IS_BROADCAST || Catchxs->override == OR_ENABLED)) {
	if (is_my_vnemy && Catchxs->override != OR_ENABLE_FORCE) {
	    Catchxs->ack = ACK_DISABLED;
	    Catchxs->override = OR_FREE;
	    return;

/* sysop/sys_analyst force x */
	} else if (((cmdflags & C_XDISABLED) || (is_my_vnemy))
		   && (Catchxs->override == OR_ENABLE_FORCE)) {
	    if (Catchxs->sender_priv >= PRIV_SYSOP)
		Catchxs->override = OR_T_ADMIN;
	    else if (Catchxs->sender_priv >= PRIV_TECHNICIAN)
		Catchxs->override = OR_T_TECH;
	    else {
		Catchxs->ack = ACK_DISABLED;
		Catchxs->override = OR_FREE;
		return;
	    }
        }
    }

	if (!IS_BROADCAST)	/* file log this x? */
	    if (usersupp->flags & US_IAMBAD) {
		char filename[L_FILE + 1];
		FILE *f;

		strcpy(filename, "");
		sprintf(filename, "%s/xlog", getuserdir(who_am_i(NULL)));
		if ((f = fopen(filename, "a")) != NULL) {
		    fprintf(f, "\n***** Message from %s to %s at %s%s",
			    Catchxs->sender, usersupp->username,
			     date(), Catchxs->message);
		    fclose(f);
		}
	    }
/* SHIX */
    if ((usersupp->flags & US_SHIX) && (!friendb) && (shix(Catchxs->message) == TRUE)) {
	Catchxs->ack = ACK_SHIX;
	Catchxs->override = OR_FREE;
	nox = 1;
	return;
    }
/* other non-broadcast received ACK's and set x_default to sender */
    if (!IS_BROADCAST) {
	if (cmdflags & C_AWAY)
	    Catchxs->ack = ACK_AWAY;
        else if (cmdflags & C_LOCK)
	    Catchxs->ack = ACK_LOCK;
	else
	    Catchxs->ack = nox ? ACK_BUSY : ACK_RECEIVED;
	override = Catchxs->override;
	Catchxs->override = OR_FREE;
	strcpy(x_default, Catchxs->sender);
    }
/* show or hold message */

/* queued messages: anything that's NOT an IMPORTANT or COUNTDOWN broadcast */
    if (QUEUED) {
	add_x_to_personal_xlog(CATCHX, Catchxs, override);

/* if not busy, show now */
	if (nox == 0) {
	    for (; xmsgp < XLIMIT; xmsgp++) {
		if (usersupp->flags & US_BEEP)
		    putchar('\007');
#ifdef CLIENTSRC
		(void) putchar(IAC);
		(void) putchar(XMSG_S);		/* x message start */
#endif
		cprintf("%s", format_express(xmsgb[xmsgp]));
#ifdef CLIENTSRC
		(void) putchar(IAC);
		(void) putchar(XMSG_E);		/* x message end */
#endif
		(void) fflush(stdout);
	    }
	} else {
	    sprintf(tempstr, _("\1d** \1bAn X from \1r%s\1b has arrived. \1d** "),
		    xmsgb[XLIMIT - 1]->sender);
	    statusbar(tempstr);
	    if (usersupp->flags & US_BEEP) {
		cprintf("");
	    }
	}

    } else {
	xmsg = (x_str *) xcalloc(1, sizeof(x_str));
	memcpy(xmsg, Catchxs, sizeof(x_str));
	putchar('\007');	/* beep regardless for these */

#ifdef CLIENTSRC
	putchar(IAC);
	putchar(XMSG_S);	/* x message start */
#endif
	cprintf("\r                                                                               \r");
	cprintf("%s", format_express(xmsg));
#ifdef CLIENTSRC
	putchar(IAC);
	putchar(XMSG_E);	/* x message end */
#endif
	(void) fflush(stdout);
	xfree(xmsg);
    }
    return;
}

/***********************************************************/
/* express()
 * X_PARAM:      20 - 40 -> feelings
 *               40 - 60 -> feelings
 *               10 - 19 -> QuickX to friend #0 - #9
 *                7 -> SILC
 *                3 -> Chat X
 *                2 -> Emote
 *                1 -> Question
 *                0 -> eXpress
 *               -1 -> X to the person who sent you the last x
 *               -2 -> BroadCast
 *               -3 -> Important Broadcast (emp only)
 *               -4 -> web X.
 */
void
express(int X_PARAM)
{
    char to[L_USERNAME + 1] = "";
    char buffer[120], filename[60];
    FILE *f;
    char override = ' ', tmpoverride;
    char send_string[X_BUFFER];

    if (user_not_allowed_to_send_x(X_PARAM))
	return;

    strcpy(to, get_xmessage_destination(to, X_PARAM, &override));
    if (!strlen(to) && !((BROADCAST) && (!NCHAT)))
	return;

    override = (check_x_permissions(to, X_PARAM, override));

    if (override == OR_NO_PERMS)
	return;

    if(!WEB)
        mono_change_online(who_am_i(NULL), to, 15);
    else
        mono_change_online(who_am_i(NULL), "", 15);

    display_express_prompt(X_PARAM);

/* get the x buffer */

    if (EMOTE) {
	xgetline(send_string, 74 - strlen(usersupp->username), FALSE);
	if (strlen(send_string) == 0) {
	    mono_change_online(who_am_i(NULL), "", 15);
	    return;
	}
    } else if (!FEEL) {
	switch ((tmpoverride = get_x_lines(send_string, X_PARAM))) {
	    case 'A':
		cprintf(_("\1f\1r\1g%s %s \1raborted.\1a\n"), config.express, config.x_message);
		if (!(QUESTION || BROADCAST)) {
		    mono_change_online(who_am_i(NULL), "", 15);
		    return;
		}
	    case OR_PING:
		if (BROADCAST)
		    cprintf("\1f\1gAborting message.\n");
		else
		    ping(to);
		mono_change_online(who_am_i(NULL), "", 15);
		return;

	    case OR_T_TECH:
	    case OR_T_ADMIN:
	    case OR_T_GUIDE:
		if (!BROADCAST)
		    override = tmpoverride;
		break;

	    case OR_SILENT:
	    case OR_NOWHERE:
	    case OR_LLAMA:
	    case OR_FISH:
	    case OR_SHOUT:

		override = tmpoverride;
		break;

	    default:
		break;
	}

	if (!strlen(send_string)) {
	    if (!BROADCAST)
		ping(to);
	    mono_change_online(who_am_i(NULL), "", 15);
	    return;
	}
	if (!BROADCAST)		/* file log this x? */
	    if (usersupp->flags & US_IAMBAD) {
		strcpy(filename, "");
		sprintf(filename, "%s/xlog", getuserdir(who_am_i(NULL)));
		if ((f = fopen(filename, "a")) != NULL) {
		    fprintf(f, "\n***** Message from %s to %s at %s%s",
			    usersupp->username, to, date(), send_string);
		    fclose(f);
		}
	    }
    }
/* send the x */

    if (BROADCAST)
	if (override == OR_CHAT || override == OR_SILC)
	    sendx(to, send_string, override);
	else
	    sendx(NULL, send_string, override);
    else {
	if (FEEL) {
	    override = OR_FEEL;
	    strcpy(send_string, "");
	    sprintf(filename, "share/feelings/feeling%d", X_PARAM - 20);
	    f = xfopen(filename, "r", FALSE);
	    if (f == NULL) {
		cprintf(_("\1r\1fCould not open feeling file.\n"));
		mono_change_online(who_am_i(NULL), "", 15);
		return;
	    }
	    while (fgets(buffer, 120, f))
		strcat(send_string, buffer);
	    fclose(f);
	}
	sendx(to, send_string, override);
    }

    mono_change_online(who_am_i(NULL), "", 15);
}


/*******************************************************************/
/* returns name to send x to, or an empty string */

char *
get_xmessage_destination(char *xmg_dest, const int X_PARAM, char *override)
{
    char namePtr[L_USERNAME+1]; 

    strcpy(xmg_dest, "");

    if (!BROADCAST) {
	if (QUICKX) {

	    strcpy(namePtr, cached_x_to_name(X_PARAM-10));
	    if (!strlen( namePtr )) {
		cprintf(_("\1f\1rThere's no X-Friend in slot #%d.\1a"), X_PARAM - 10);
		return xmg_dest;
	    }
	    strcpy(xmg_dest, namePtr);
	    cprintf(_("\n\1f\1gSend Quick %s %s to \1y%s\1g.\1a\n"),
		    config.express, config.x_message, xmg_dest);
	    fflush(stdout);

	    /* reply */
	} else if (REPLY) {
	    if (!strlen(x_default))
		return xmg_dest;

	    /* reply to guide question? */
	    if ((usersupp->flags & US_GUIDE) && (xmsgb[XLIMIT - 1]))
		if ((xmsgb[XLIMIT - 1]->override == OR_QUESTION) &&
		    (strcmp(xmsgb[XLIMIT - 1]->sender, x_default) == 0))
		    *override = OR_T_GUIDE;

	    cprintf("\n\1f\1gSending the %s%s\1g %s to \1y%s\1g.\1a\n"
		    ,(*override == OR_T_GUIDE) ? GUIDECOL : ""
		  ,(*override == OR_T_GUIDE) ? config.guide : config.express
		    ,config.x_message, x_default);
	    strcpy(xmg_dest, x_default);

	    /* question */
	} else if (QUESTION) {
	    strcpy(xmg_dest, get_guide_name(xmg_dest));

	    /* web X */
        } else if (WEB) {
	    if (web_default[0]) {
		cprintf(_("\1f\1gRecipient \1w(\1y%s\1w): \1c"), web_default);
		strcpy(xmg_dest, web_default);
	    } else
		cprintf(_("\1f\1gRecipient\1w: \1c"));
	    strcpy( namePtr, get_name(5)); /* was assignment */
            if(*namePtr)
	        strcpy(xmg_dest, namePtr);
	    if (!strlen(xmg_dest))
		return xmg_dest;
	    strcpy(web_default, xmg_dest);
            *override = OR_WEB;

	    /* normal/feeling */
	} else if (NORMAL || FEEL || EMOTE) {
	    if (strlen(x_default)) {
		cprintf(_("\1f\1gRecipient \1w(\1y%s\1w): \1c"), x_default);
		strcpy(xmg_dest, x_default);
	    } else
		cprintf(_("\1f\1gRecipient\1w: \1c"));
	    strcpy( namePtr, get_name(5)); /* was assignment */
	    if (*namePtr) {
		strcpy(xmg_dest, namePtr);
	    }
	    if (strlen(xmg_dest))
		strcpy(profile_default, xmg_dest);
	}
/* broadcast */

    } else {
	if (NCHAT) {
	    int channel, i;

	    if (chat_default[0]) {
		cprintf(_("\1f\1gSend %s %s \1w(\1p%s\1w): \1c"), config.chatmode, config.chatroom, chat_default);
		strcpy(xmg_dest, chat_default);
	    } else
		cprintf(_("\1f\1gSend %s %s\1w: \1c"), config.chatmode, config.chatroom);
	    strcpy( namePtr, get_name(5) );
	    if (*namePtr)
		strcpy(xmg_dest, namePtr);
	    if (!strlen(xmg_dest))
		return xmg_dest;

	    for (channel = -1, i = 0; i < MAXCHATROOMS; i++)
		if (EQ(xmg_dest, shm->holodeck[i].name)) {
		    channel = i;
		    break;
		}
	    if (channel == -1) {
		i = atoi(xmg_dest);
		if (i > 0 && i < 10)
		    channel = i - 1;
	    }
	    if (channel == -1) {
		cprintf(_("\1r\1fNo such %s %s.\1a\n"), config.chatmode, config.chatroom);
		strcpy(xmg_dest, "");
		return xmg_dest;
	    }
	    strcpy(xmg_dest, shm->holodeck[channel].name);

	    if (!(usersupp->chat & (1 << channel))) {
		cprintf(_("\1r\1gYou're not listening to %s \1p%s\1g.\n"), config.chatroom, xmg_dest);
		strcpy(xmg_dest, "");
	    }
	    strcpy(chat_default, xmg_dest);

	} else if (NSILC)	/* here for documentation porposes, xmg_dest is */
	    strcpy(xmg_dest, "SILC");	/* already strlen 0 */
    }
    return xmg_dest;
}


/**************************************************************/
/* returns proper override, or OR_NO_PERMS which aborts the x */
char
check_x_permissions(const char *x_recip_name, const int X_PARAM, char override)
{
    btmp_t *x_recip_btmp = NULL;
    int is_my_vnemy = 0, is_vnemy_to = 0;

    for (;;) {
	if (!BROADCAST) {
	    is_vnemy_to = is_enemy(x_recip_name, who_am_i(NULL));
	    is_my_vnemy = is_cached_enemy(x_recip_name);

	    if (EQ("guest", x_recip_name)) {
		cprintf(_("\1rYou cannot send %s to Guests.\n\1a"), config.x_message_pl);
		override = OR_NO_PERMS;
		break;
	    }

/* (if web x) does user exist? */
/* This must be checked before we try to read a btmp entry!!! */
            if(WEB) {
                if( mono_sql_u_check_user(x_recip_name) == FALSE ) {
		    cprintf(_("\1f\1rNo such user.\1a\n"));
                    flush_input();
                    override = OR_NO_PERMS;
                    return override;
                }
                if( mono_sql_onl_check_user(x_recip_name) == FALSE ) {
		    cprintf(_("\1f\1rThat user is not online via the web.\1a\n"));
                    flush_input();
                    override = OR_NO_PERMS;
                    return override;
                }
                override = OR_WEB;
                return override;
            }

/* read the btmp and get the destination record */
            x_recip_btmp = mono_read_btmp(x_recip_name);
	    if (x_recip_btmp == NULL) {
		cprintf("%s", (mono_sql_u_check_user(x_recip_name) == TRUE) ?
		 _("\1f\1rThat user is not online.\1a\n") : _("\1f\1rNo such user.\1a\n"));
		flush_input();
		override = OR_NO_PERMS;
		break;
	    }
/* deleted/degraded recipient? */
	    if (x_recip_btmp->priv & (PRIV_DELETED | PRIV_DEGRADED)) {
		cprintf(_("\1f\1rYou cannot send messages to degraded users.\1a\n"));
		override = OR_NO_PERMS;
		break;
	    }
#ifdef ARGH_BUG_FIXED
/* unvalidated recipient ? */
	    if (!(x_recip_btmp->priv & PRIV_VALIDATED))
		if (!(usersupp->flags & US_GUIDE)) {
		    cprintf(_("\n\1f\1rSorry, that user is unvalidated.\1a\n"));
		    override = OR_NO_PERMS;
		    break;
		} else
		    override = OR_T_GUIDE;
#endif
/* x-disabled sender trying to x non-friends ? */
	    if (cmdflags & C_XDISABLED && X_PARAM != 1)
		if (is_cached_friend(x_recip_name) == FALSE) {
		    cprintf(_("\1f\1gSorry, you are \1rX-disabled\1g right now, and you cannot"));
		    cprintf(_(" X users that are not\non your X-friends-list.\1a\n"));
		    override = OR_NO_PERMS;
		    break;
		}
/* recipient is x-enemy ? */
	    if (is_vnemy_to || is_my_vnemy) {
		if (mySysGuide != NULL)
		    if (strcmp(x_recip_name, mySysGuide) == 0) {
		        xfree(mySysGuide);
		        mySysGuide = NULL;
		    }    
		if (is_my_vnemy)
		    if (is_cached_friend(x_recip_name))
			cprintf(_("\1f\1y\n%s is on both your friend and enemy lists, how odd..  (:\1a\n")
				,x_recip_name);
		    else
			cprintf(_("\1f\1r\nSorry. You have \1y%s \1rX-disabled.\1a\n"), x_recip_name);
		else
		    cprintf(_("\1f\1rSorry, \1y%s \1rhas X-disabled you.\1a\n"), x_recip_name);
		if (override != OR_ENABLED && usersupp->priv >= PRIV_TECHNICIAN) {
		    cprintf(_("\1f\1pDo you wish to send the X anyway? \1w(\1ry/n\1w) \1a"));
		    if (yesno() == NO) {
			override = OR_NO_PERMS;
			break;
		    }
		    override = OR_ENABLE_FORCE;
		    log_sysop_action("overrode %s's enemylist status.", x_recip_name);
		} else if (override != OR_ENABLED) {
		    override = OR_NO_PERMS;
		    break;
		}
	    }
/* x-disabled recipient ? */
	    if (x_recip_btmp->flags & B_XDISABLED) {
		if (is_friend(x_recip_name, usersupp->username))
		    override = OR_ENABLED;
		cprintf(_("\1y\1f%s \1ghas disabled %s %s.\n\1a"), x_recip_btmp->username
			,config.express, config.x_message_pl);
		if (override != OR_ENABLED && usersupp->priv >= PRIV_TECHNICIAN) {
		    cprintf(_("\1f\1pDo you wish to send the X anyway? \1w(\1ry/n\1w) \1a"));
		    if (yesno() == NO) {
			override = OR_NO_PERMS;
			break;
		    }
		    override = OR_ENABLE_FORCE;
		    log_sysop_action("overrode %s's x-disable status.", x_recip_name);
		} else if (override != OR_ENABLED) {
		    override = OR_NO_PERMS;
		    break;
		}
	    }
/* away notify */
	    if (x_recip_btmp->flags & B_AWAY) {
		cprintf(_("\1r\1y%s \1gis \1raway \1gand may not receive your %s %s immediately.\n")
			,x_recip_btmp->username, config.express, config.x_message);
		cprintf("\1rAway\1w: \1r%s\n", x_recip_btmp->awaymsg);
	    }
            if (x_recip_btmp->flags & B_LOCK) {
		cprintf(_("\1g\1y%s \1gis \1clocked \1gand will not get your %s %s immediately.\n")
			,x_recip_btmp->username, config.express, config.x_message);
            }
	    if (EMOTE)
		override = OR_EMOTE;
	    else if (QUESTION)
		override = OR_QUESTION;

	} else {
	    if (X_PARAM == 7)	/* SILC */
		override = OR_SILC;
	    else if (X_PARAM == 3)	/* Chat */
		override = OR_CHAT;
	    else if (X_PARAM == -3)	/* emp broadcast */
		override = OR_IMPORTANT;
	    else if (X_PARAM == -2)	/* broadcast */
		override = OR_BROADCAST;

	}
	break;
    }
    if (x_recip_btmp != NULL)
	xfree(x_recip_btmp);

    return override;
}

/*************************************************/
void
ping(char *to)
{
    sendx(to, "", OR_PING);
}			

/*------------------------------------------*/
/*
 * setup_express()
 * sets up some things needed for X's to work
 * properly; only called once at login.
 * it should not be called BEFORE the user is added to the wholist
 */
void
setup_express()
{
    int i;

    for (i = 0; i < XLIMIT; i++)
	xmsgb[i] = NULL;
    xmsgp = XLIMIT;
    mono_find_xslot(who_am_i(NULL))->override = OR_FREE;
    start_user_cache(usersupp->usernum);
/*    update_friends_cache(); */
    return;
}
/*----------------------------------*/
/* void remove_express()
 * removes xmsg buffer from memory
 */
void
remove_express()
{
    int i;

    for (i = 0; i < XLIMIT; i++)
	if (xmsgb[i] != NULL)
	    xfree(xmsgb[i]);
    return;
}				

/**************************************************************//* displays old x messages.  <ctrl-x>
				 */
void
old_express()
{

    int i, direction = -1;
    char c = ' ';
    FILE *fp;

    if (xmsgb[XLIMIT - 1] == NULL) {
	cprintf(_("\1f\1rThere are no %s %s in your log.\1a\n"), config.express, config.x_message_pl);
	return;
    }
    i = XLIMIT - 1;
    while (i >= 0 && i < XLIMIT && c != 'S' && xmsgb[i]) {
	cprintf("%s", format_express(xmsgb[i]));
	cprintf("\1f\1w<\1rc\1w>\1ylip  \1w<\1ra\1w>\1ygain  \1w<\1rn\1w>\1yext");
	cprintf("  \1w<\1rs\1w>\1ytop  \1w<\1rd\1w>\1yirection \1w[%s\1w] \1w-> "
		,(direction == -1) ? "\1rbackward" : "\1gforward");
	c = get_single_quiet("ABCDFNSX ");
	switch (c) {
	    case 'A':
		cprintf("Again\n");
		break;

	    case 'D':
	    case 'F':
	    case 'B':
		direction = (c == 'D') ? ((direction == 1) ? -1 : 1) : (c == 'F') ? 1 : -1;
		cprintf("%s\n", (direction == 1) ? "next" : "previous");
		i += direction;
		break;

	    case 'N':
	    case ' ':
		cprintf("%s\n", (direction == 1) ? "next" : "previous");
		i += direction;
		break;

	    case 'C':
		cprintf("Clip\n");
		fp = fopen(CLIPFILE, "a");
		fprintf(fp, "%s", format_express(xmsgb[i]));
		fclose(fp);
		break;

	    case 'S':
		cprintf("Stop\n");
		break;

	    case 'X':
		cprintf("Send %s %s.\n", config.express, config.x_message);
		nox = 1;
		express(0);
		break;
	}
    }
}

/*****************************************************/
/* change_express()
 * how: -1 -> disable
 *       0 -> just change to the other state
 *       1 -> enable
 */

void
change_express(int how)
{
    btmp_t *my_btmp;

    my_btmp = mono_read_btmp(who_am_i(NULL));
    if (my_btmp == NULL) {
	cprintf("ack!  Can't load a spare copy of myself.\n");
	return;
    }
    if ((!(my_btmp->flags & B_XDISABLED) && !how) || how < 0) {
	mono_change_online(who_am_i(NULL), "", 9);
	cmdflags |= C_XDISABLED;
    } else if (((my_btmp->flags & B_XDISABLED) && !how) || how > 0) {
	mono_change_online(who_am_i(NULL), "", -9);
	cmdflags &= ~C_XDISABLED;
    } else
	cprintf("oopsie.. ? changing-problems...\n");
    cprintf(_("\1f\1wYour %s %s are %sabled\1w.\1a"),
	    config.express, config.x_message_pl,
	    (cmdflags & C_XDISABLED) ? "\1rdis" : "\1gen");

    xfree(my_btmp);
}

/*******************************************************************/
void
are_there_held_xs()
{

    nox = 0;			/* are_there_held_xs() */

    if (!shm)
	return;

    if (xmsgp < XLIMIT) {
	cprintf(_("\1f\1gThese %s %s were held for you while you were busy:\1a\n"), config.express, config.x_message_pl);
	while (xmsgp < XLIMIT) {
	    if ((usersupp->flags & US_BEEP))
		cprintf("\007");

#ifdef CLIENTSRC
	    putchar(IAC);
	    putchar(XMSG_S);
#endif
	    cprintf("%s", format_express(xmsgb[xmsgp++]));
#ifdef CLIENTSRC
	    putchar(IAC);
	    putchar(XMSG_E);
#endif

	}
	flush_input();		/* let's hope this helps */
	fflush(stdout);
    }

    are_there_held_web_xs();

    return;
}

/************************************************************/
void
emergency_boot_all_users()
{

    int i;
    char send_string[X_BUFFER];
    btmp_t *p;

    cprintf("\1f\1rDo you really want to \1cBOOT\1r all users ? (y/n) ");
    if (yesno() == NO) {
	return;
    }
    log_sysop_action("booted all users");

    strcpy(send_string, "\
This is an emergency! All users will be booted off the bbs in 20 seconds.\n\
Quit what you're doing now, and log off!!!\n");

    sendx(NULL, send_string, OR_IMPORTANT);

    for (i = 20; i >= 0; i--) {
	sprintf(send_string, "\1f%s !!! %d !!!", (i < 11) ? "\1r" : "\1w", i);
	sleep(1);
	sendx(NULL, send_string, OR_COUNTDOWN);
    }

    i = shm->first;
    while (i != -1) {
	p = &(shm->wholist[i]);
	if (p->pid != getpid())
	    (void) kill(p->pid, SIGABRT);
	i = p->next;
    }
    return;
}


/*************************************************/
void
send_silc()
{
    if (cmdflags & C_NOSILC) {
	cprintf("\1rNot when you have SILC disabled\n");
	return;
    }
    cprintf("\r                                                                               \r");
    cprintf("\1w\1f<\1rSILC\1w>\n");
    nox = TRUE;
    express(7);
    nox = FALSE;
}

/*************************************************/
void
quoted_Xmsgs()
{
    char quoteduser[L_USERNAME + 1];
    int channel, i, matches = 0;
    FILE *fp;

    cprintf("\1f\1c\n%s%s%s%s",
	"This function quotes all of the express messages in your xlog ",
	"sent to and from a\ncertain harassing user, and sends them to the ",
	"Sysops so that they can take\nproper actions, and serve as the ",
	"evidence of the harassment.\n\n\1gUsername/Channel: \1c");

    strcpy(quoteduser, get_name(5));
    if (strlen(quoteduser) == 0)
	return;

    for (channel = -1, i = 0; i < MAXCHATROOMS; i++)
        if (EQ(quoteduser, shm->holodeck[i].name)) {
            channel = i;
	    break;
	}
    if (channel == -1 && mono_sql_u_check_user(quoteduser) == FALSE) {
        cprintf("\1f\1rNo such user/channel.\n\1a");
        return;
    }

    cprintf("%s%s\1g' to the Admin? \1w(\1gy\1w/\1gn\1w)\1c ", 
		"\1f\1gQuote conversation with `\1y", quoteduser);

    if (yesno() == NO) {
	cprintf("\1f\1gOkay, not quoting the conversation.\1a\n");
	return;
    }
    if ((fp = fopen(temp, "w")) == NULL) {
	cprintf("%s%s%s",
	"\1f\1gThe x-es were \1rNOT \1gsent to the Admin due to", 
		" an error.\nPlease try again if you still want to",
		" quote the conversation.\n\1a");
	return;
    }
    for (i = 0; i < XLIMIT; i++) {
	if (!(xmsgb[i]))
	    continue;

	if ((EQ(xmsgb[i]->sender, quoteduser)) || 
		(EQ(xmsgb[i]->recipient, quoteduser))) {
	    fprintf(fp, "%s", format_express(xmsgb[i]));
	    matches++;
	}
    }
    fclose(fp);

    if (matches == 0) {
	cprintf("\1ySorry, there were no X'msgs from %s in your XLog... \n",
		quoteduser);
	unlink(temp);
	return;
    }

    enter_message(QUOTED_X_FORUM, EDIT_NOEDIT, FILE_POST_BANNER, NULL);

    cprintf("\1f\1gA log of your conversation with \1y%s\1g has been sent%s",
	    quoteduser,  " to the admin.\n");
    return;
}

/*******************************************************************/
static void
format_ack(express_t * Catchxs)
{
    switch (Catchxs->ack) {
        
	case ACK_LOCK:
	    cprintf(_("\1f\1g%s %s received by \1y%s\1g.\n"), config.express
		    ,config.x_message, Catchxs->recipient);
	    cprintf(_("\1f\1y%s\1g is marked as \1clocked\1g though.\1a\n")
		    ,Catchxs->recipient);
	    break;

	case ACK_AWAY:
	    cprintf(_("\1f\1g%s %s received by \1y%s\1g.\n"), config.express
		    ,config.x_message, Catchxs->recipient);
	    cprintf(_("\1f\1y%s\1g is marked as \1raway\1g from keyboard though.\1a\n")
		    ,Catchxs->recipient);
	    break;

	case ACK_OOPS:
	    cprintf(_("\1f\1gOops! Something went wrong.\n"));
	    cprintf(_("\1y%s\1g did \1rnot\1g receive your %s %s.\1a\n"), Catchxs->recipient
		    ,config.express, config.x_message);
	    break;

	case ACK_NOTBUSY:	/* ping */
	    cprintf(_("\1f\1y%s \1gis not busy.\n\1a"), Catchxs->recipient);
	    break;

	case ACK_SHIX:
	    cprintf(_("\1f\1y%s\1g does not wish to receive that %s.\1a\n")
		    ,Catchxs->recipient, config.x_message);
	    break;

	case ACK_RECEIVED:
	    {
		int idling;
		btmp_t *idleuser = NULL;

		if ((idleuser = mono_read_btmp(Catchxs->recipient)) != NULL) {
		    if (idleuser->flags & B_REALIDLE) {
			idling = time(0) - idleuser->idletime;
			idling /= 60;
			cprintf("\1f\1g%s %s received by \1y%s\1g.\n\1y%s\1g has been %s for \1y%2.2d:%2.2d\1g though.\1a\n", config.express, config.x_message, idleuser->username, idleuser->username, (usersupp->timescalled > 50) ? config.idle : "idle", idling / 60, idling % 60);
		    } else
			cprintf(_("\1f\1g%s %s received by \1y%s\1g.\1a\n")
				,config.express, config.x_message
				,Catchxs->recipient);
		    xfree(idleuser);
		} else
		    cprintf(_("\1f\1g%s %s received by \1y%s\1g.\1a\n")
			    ,config.express, config.x_message
			    ,Catchxs->recipient);
		break;
	    }

	case ACK_DISABLED:
	    cprintf(_("\1f\1gSorry, \1y%s \1ghas \1rX-disabled\1g you!!!\1a\n")
		    ,Catchxs->recipient);
	    break;

	case ACK_BUFFFULL:
	    cprintf(_("\1f\1gOops! \1y%s\1g's buffer is full.\n"), Catchxs->recipient);
	    cprintf(_("Your %s was /1rnot/1g received, try re-sending it in a moment.\1a\n")
		    ,config.x_message);
	    break;

	case ACK_TURNEDOFF:
	    cprintf(_("\1f\1y%s \1gturned off %s %s before you finished typing.\1a\n")
		  ,config.express, config.x_message_pl, Catchxs->recipient);
	    break;

	case ACK_PING_BUSY:
	    cprintf(_("\1f\1y%s \1gis \1rbusy\1g.\1a\n"), Catchxs->recipient);
	    break;

	case ACK_BUSY:
	    cprintf(_("\1f\1y%s \1gis \1rbusy\1g"), Catchxs->recipient);
	    cprintf(_(" and will receive your %s when done.\1a\n"), config.x_message);
	    break;

	default:
	    cprintf(_("\1f\1gOops, \1y%s\1g is not making sense.\n"), Catchxs->recipient);
	    cprintf(_("The %s %s may /1rnot/1g have arrived.\1a\n"), config.express, config.x_message);
	    break;

    }				/* switch */
    return;
}


/*******************************************************************/
char *
format_express(express_t * Catchxs)
{
    int i, vriend;
    struct tm *tp;
    char from[40];		/* the formatted from header */
    static char buf[X_BUFFER + 100];

/* an x from whom? */
    if ((vriend = is_cached_friend(Catchxs->sender))) {
	if ((i = cached_name_to_x(Catchxs->sender)) == -1)
	    sprintf(from, FRIENDCOL "\1n%s\1N", Catchxs->sender);
	else
	    sprintf(from, FRIENDCOL "\1n%s\1N \1w(\1g%d\1w)", Catchxs->sender, i);
    } else
	sprintf(from, "\1y\1n%s\1N", Catchxs->sender);

    if ((tp = localtime(&(Catchxs->time))) == NULL) {
	sprintf(buf, "Error, can't get current time.\n");
	(void) log_it("errors", "Can't get localtime in format_express(): express.c");
    } else
	switch (Catchxs->override) {

	    case OR_SHIX:
		sprintf(buf, _("\r\1f\1w*** \1gA %s from %s was intercepted by SHIX \1w***\n")
			,config.x_message, from);
		break;

	    case OR_EMOTE:
		sprintf(buf, _("\r\1f\1b*** %s \1g%s\n"), from, Catchxs->message);
		break;

	    case OR_SILC:
		sprintf(buf, _("\n\n\1f\1w[\1r %s \1w]\n\1y%s\n"), Catchxs->sender, Catchxs->message);
		break;

	    case OR_CHAT:
		sprintf(buf, _("\n\1f\1p=== \1g%s %s from %s \1gto \1p%s \1gat \1w(\1g%02d:%02d\1w) \1p===\1a\n\1c%s")
		     ,config.chatmode, config.chatroom, from, Catchxs->recipient, tp->tm_hour
			,tp->tm_min, Catchxs->message);
		break;

	    case OR_BROADCAST:
		sprintf(buf, _("\n\1f\1b*** \1pBroadcast from %s \1gat \1c(\1p%02d:%02d\1w) \1b***\n\1c%s\1a")
			,from, tp->tm_hour, tp->tm_min, Catchxs->message);
		break;

	    case OR_FISH:
		sprintf(buf, _("\n\r\1f\1w*** \1gMany squids wave their tentacles as if to say: \1w***\1a\n\1c%s"), Catchxs->message);
		break;

	    case OR_LLAMA:
		sprintf(buf, _("\n\r\1f\1r*** \1wYour Black Llama Airhostess says: \1r***\1a\n\1c%s"), Catchxs->message);
		break;

	    case OR_SHOUT:	/* shout message */
		sprintf(buf, _("\n\r\1f\1b*** \1gYour mother shouts from downstairs \1b***\1a\n\1c%s"), Catchxs->message);
		break;

	    case OR_IMPORTANT:	/* Emp Broadcast  */
		sprintf(buf, _("\007\n\1f\1r*** \1pIMPORTANT BROADCAST from \1w\1n%s\1N \1r***\n\1w%s\1a"), Catchxs->sender, Catchxs->message);
		break;

	    case OR_LOGIN:
	    case OR_LOGOUT:
		if (vriend) {
			if ( Catchxs->override == OR_LOGIN ) {
		    sprintf(buf, _("\1f\1b*** \1gYour friend %s\1g just logged \1gon \1w(\1g%02d:%02d\1w) \1b***\1a\n"), from, tp->tm_hour, tp->tm_min);
			} else {
		    sprintf(buf, _("\1f\1b*** \1gYour friend %s\1g just logged \1roff \1w(\1g%02d:%02d\1w) \1b***\1a\n"), from, tp->tm_hour, tp->tm_min);
			}
		} else {
                       if ( Catchxs->override == OR_LOGIN ) {
		    sprintf(buf, _("\1f\1b*** %s\1g just logged \1gon \1w(\1g%02d:%02d\1w) \1b***\1a\n"), from, tp->tm_hour, tp->tm_min);
			} else {
		    sprintf(buf, _("\1f\1b*** %s\1g just logged \1roff \1w(\1g%02d:%02d\1w) \1b***\1a\n"), from, tp->tm_hour, tp->tm_min);
			}
		}
		break;

	    case OR_KICKOFF:
		if (vriend)
		    sprintf(buf, _("\1f\1b*** \1gYour friend %s\1g has been logged off \1w(\1g%02d:%02d\1w) \1b***\n\1a\n"), from, tp->tm_hour, tp->tm_min);
		else
		    sprintf(buf, _("\1f\1b*** %s\1g has been logged off \1w(\1g%02d:%02d\1w) \1b***\n\1a\n"), from, tp->tm_hour, tp->tm_min);
		break;

	    case OR_NOWHERE:	/* House Spirit Broadcast */
		sprintf(buf, _("\n\1f\1r* \1wThe House Spirit whispers: \1r*\n\1a\1c%s"), Catchxs->message);
		break;

	    case OR_COUNTDOWN:	/* CountDown-Broadcast  */
		sprintf(buf, _("\n%s\n"), Catchxs->message);
		break;

	    case OR_SILENT:
		sprintf(buf, _("\n\1f\1r***\1w Cthulhu whispers: \1r***\1a\n\1c%s"), Catchxs->message);
		break;

	    case OR_T_GUIDE:
		sprintf(buf, _("\n\1f\1b*** \1g%s %s from \1w( %s%s \1w) \1y\1n%s\1N \1gat \1w(\1g%02d:%02d\1w) \1b***\1a\n\1c%s")
		   ,config.express, config.x_message, GUIDECOL, config.guide
		,Catchxs->sender, tp->tm_hour, tp->tm_min, Catchxs->message);
		break;

	    case OR_T_TECH:
		sprintf(buf, _("\n\1f\1b*** \1g%s %s from \1w( %s%s \1w) \1n%s\1N \1gat \1w(\1g%02d:%02d\1w) \1b***\1a\n\1c%s")
			,config.express, config.x_message, PROGRAMMERCOL, config.programmer
		,Catchxs->sender, tp->tm_hour, tp->tm_min, Catchxs->message);
		break;

	    case OR_T_ADMIN:
		sprintf(buf, _("\n\1f\1b*** \1g%s %s from \1w( %s%s \1w) \1n%s\1N \1gat \1w(\1g%02d:%02d\1w) \1b***\1a\n\1c%s")
		   ,config.express, config.x_message, ADMINCOL, config.admin
		,Catchxs->sender, tp->tm_hour, tp->tm_min, Catchxs->message);
		break;

	    case OR_QUESTION:
		sprintf(buf, _("\n\1f\1r????? \1gHELP REQUEST \
from \1y\1n%s\1N \1gat \1w(\1g%02d:%02d\1w) \1r?????\1a\n\1c%s"),
		Catchxs->sender, tp->tm_hour, tp->tm_min, Catchxs->message);
		break;

	    case OR_FEEL:
		sprintf(buf, _("\n\1f\1b*** \1gFeeling from %s \1gat \1w(\1g%02d:%02d\1w) \1b***\1a\n%s\1a"), from, tp->tm_hour, tp->tm_min, Catchxs->message);
		break;

	    case OR_WEB:
		sprintf(buf, _("\n\1f\1b*** \1pWeb\1g %s %s from %s \1gto \1y%s \1gat \1w(\1g%02d:%02d\1w) \1b***\1a\n\1c%s"), config.express, config.x_message, from, Catchxs->recipient
			,tp->tm_hour, tp->tm_min, Catchxs->message);
                break;

	    case OR_ENABLED:
	    default:
		sprintf(buf, _("\n\1f\1b*** \1g%s %s from %s \1gto \1y%s \1gat \1w(\1g%02d:%02d\1w) \1b***\1a\n\1c%s"), config.express, config.x_message, from, Catchxs->recipient
			,tp->tm_hour, tp->tm_min, Catchxs->message);
		break;

	}
    return buf;
}


/******************************************************/
int
user_not_allowed_to_send_x(const int X_PARAM)
{
    if (usersupp->priv & PRIV_DELETED || usersupp->priv & PRIV_DEGRADED ||
	(!(usersupp->priv & PRIV_VALIDATED) && X_PARAM != 1)) {
	cprintf(_("\1f\1rYou cannot send %s %s.\1a\n"), config.express, config.x_message_pl);
	return 1;
    }
    return 0;
}

/**************************************************/
char *
get_guide_name(char *guide_person_name)
{
    int search_again = 0;
    btmp_t *guide_person;

    if (mySysGuide == NULL) {
	mySysGuide = (char *) xmalloc(L_USERNAME + 2);
	search_again = 1;
    } else if ((guide_person = mono_read_btmp(mySysGuide)) == NULL) {
	search_again = 1;
	cprintf(_("\1f\1rYour %s is no longer online.\nSearching for a new one...\1a\n"), config.guide);
    } else if (guide_person->flags & B_XDISABLED) {
	cprintf(_("\1f\1rYour %s has his/her messages disabled.\1a\n"), config.guide);
	cprintf(_("\1f\1rSearching for a new one...\1a\n"));
	search_again = 1;
	xfree(guide_person);
    } else if ((guide_person->flags & B_GUIDEFLAGGED) == 0) {
	cprintf(_("\1f\1rYour %s is no longer marked as %s.\1a\n"), config.guide, config.guide);
	cprintf(_("\1f\1rSearching for a new one...\1a\n"));
	search_again = 1;
	xfree(guide_person);
    }
    strcpy(guide_person_name, mySysGuide);

    if (search_again == 1) {
	if ((guide_person = mono_search_guide()) == NULL) {
	    cprintf(_("\1f\1gThere are no %s online right now...\1a\n"), config.guide);
	    cprintf(_("\1f\1gYou could try to find the answer in the help files.\1a\n"));
	    cprintf(_("\1f\1gPress \1w<\1r?\1w>\1g to see all commands, or \1w<\1rh\1w>\1g to access the helpfiles.\n"));
	    xfree(mySysGuide);
	    mySysGuide = NULL;
	    strcpy(guide_person_name, "");
	} else {
	    strcpy(mySysGuide, guide_person->username);
	    xfree(guide_person);
	}
    }
    if ((cmdflags & C_XDISABLED) && (strlen(guide_person_name))) {
	cprintf(_("\1f\1gYou are asking a %s a Question, and therefore\n \1a"),
		config.guide);
	change_express(1);
	cprintf("\n");
    }
    if (mySysGuide != NULL)
	strcpy(guide_person_name, mySysGuide);
    return guide_person_name;
}

/*****************************************************/
void
display_express_prompt(const int X_PARAM)
{
/* emote */
    if (EMOTE) {
	cprintf(_("\1f\1gYou are now sending an emote.\n\1b*** \1y%s \1g")
		,usersupp->username);
	cprintf("\1c");

/* question */
    } else if (QUESTION)
	cprintf(_("\1f\1gYou're now asking \1c%s\1g a BBS-related Question!\1a\n")
		,mySysGuide);

/* broadcast */
    else if (BROADCAST)
	if ((X_PARAM != 7) && (X_PARAM != 3))
	    cprintf(_("\1f\1rYou're now sending a broadcast!\1a\n"));

    if (cmdflags & C_AWAY)
	cprintf(_("\1rWarning: you are still marked as \1caway\1r.\1a\n"));

    if (!(FEEL || EMOTE))
	cprintf("\1a\1c");
}

/*************************************************
 * 
 * feeling() - turn it into a menu.... 
 * 
 *************************************************/

void
feeling()
{

    register char cmd = '\0';

    cprintf(_("\1f\1gSend Feeling %s.\1a\n"), config.x_message);
    if (usersupp->flags & US_EXPERT)
	cprintf(_("\1f\1gPress \1w<\1r?\1w>\1g for a list of available feelings.\1a\n"));
    if (usersupp->flags & US_COOL )
	cprintf(_("\1f\1gYou're cool. Press \1w<\1rf\1w>\1g to send the \1cFREEZE\1g feeling.\1a\n"));

    while ((cmd != SP) && (cmd != 13) && (cmd !='\n')) {
	IFNEXPERT
	{
	    cprintf("\1f\1gMenu Options.\1a\n");
	    more(MENUDIR "/menu_feelings", 1);
	}
	cprintf("\n\1f\1gFeeling: \1a");
	cmd = get_single_quiet("12abcdefFgGhHiklmnoOpPrRstTuwzZ\n ?\013");

	if (cmd != '?')
	    cprintf("\1f\1c%c\n", cmd);

	switch (cmd) {
	    case ' ':
	    case 13:
	    case '\n':
		return;

	    case '?':
		cprintf("\1f\1gFeeling Options.\1a");
		more(MENUDIR "/menu_feelings", 1);
		break;

/* Peter put all feeling under a specific letter instead of a number */

	    case 'e':
		express(20);
		break;
	    case 'w':
		express(21);
		break;
	    case 'z':
		express(22);
		break;
	    case 'h':
		express(23);
		break;
	    case 'l':
		express(24);
		break;
	    case 'r':
		express(25);
		break;
	    case 's':
		express(26);
		break;
	    case 'u':
		express(27);
		break;
	    case 'b':
		express(28);
		break;
	    case 'm':
		express(29);
		break;
	    case 'o':
		express(30);
		break;
	    case 'f':
		if (usersupp->flags & US_COOL)
		    express(31);
		break;
	    case 'i':
		express(32);
		break;
	    case 'k':
		express(33);
		break;
	    case 't':
		express(34);
		break;
	    case 'g':
		express(35);
		break;

	    case '1':
		express(36);
		break;

	    case '2':
		express(37);
		break;

	    case 'p':
		express(38);
		break;
	    case 'a':
		express(39);
		break;

	    case 'R':
		express(40);
		break;

	    case 'd':
		express(41);
		break;
	    case 'G':
		express(42);
		break;
	    case 'P':
		express(43);
		break;
	    case 'O':
		express(44);
		break;
	    case 'n':
		express(45);
		break;
	    case 'c':
		express(46);
		break;
	    case 'Z':
		express(47);
		break;
	    case 'T':
		express(48);
		break;
	    case 'H':
		express(49);
		break;
	    case 'F':
		express(50);
		break;

	    default:
		cprintf("\1f\1rSomething went wrong here...\1a\n");
		return;
	}
    }
}

void
are_there_held_web_xs()
{
    wx_list_t *list = NULL;
    int count = 0;

    count = mono_sql_web_get_xes(usersupp->usernum, &list);

    if(count == -1)
       cprintf(_("\1f\1rAn error occurrect trying to retrieve \1pweb \1rx-es.\n"));
    if(count > 0) {
       cprintf(_("\n\1f\1gYou have \1y%d \1gnew \1pWeb \1g%s %s \1w(\1gHit \1w<\1r^\1w> \1gto reply\1w)\1a\n"), count, config.express, (count == 1) ? config.x_message : config.x_message_pl);
       show_web_xes(list);
       (void) mono_sql_web_mark_wx_read(list);
    }

    mono_sql_ll_free_wxlist(list);

    return;
}

void
show_web_xes(wx_list_t *list)
{

    struct tm *tp;
    x_str *Sendxs = NULL;

    while(list != NULL) {

        Sendxs = (x_str *) xmalloc( sizeof(x_str) );
        Sendxs->override = OR_WEB;
        Sendxs->time = list->x->date;
        sprintf(Sendxs->sender, "%s@Web", list->x->sender);
        sprintf(Sendxs->recipient, "%s", usersupp->username);
        sprintf(Sendxs->message, "%s\n", list->x->message);
        add_x_to_personal_xlog(SENDX, Sendxs, Sendxs->override);
        xfree( Sendxs );

        tp = localtime(&(list->x->date));
	if (usersupp->flags & US_BEEP)
	    putchar('\007');
        cprintf("\1f\1b\n*** \1pWeb \1g%s %s from \1y%s@web \1gto \1y%s \1gat \1w(\1g%02d:%02d\1w) \1b***\n\1a\1c%s\n"
            ,config.express, config.x_message, list->x->sender
            ,usersupp->username, tp->tm_hour, tp->tm_min, list->x->message );
        list = list->next;
    }
    return;
}

/* eof */
