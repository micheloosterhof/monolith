/* $Id$ */
/* todo: set datestamp */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <build-defs.h>

#include <unistd.h>
#include <signal.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <time.h>

#ifdef USE_MYSQL
#include MYSQL_HEADER
#endif

#include "monolith.h"
#include "routines.h"
#include "log.h"
#include "btmp.h"
#include "sql_utils.h"

#define SQL_QUERY_BUFF_INC	128
#undef SQL_DEBUG

static MYSQL mp;
static int connected = FALSE;
static int logqueries = FALSE;

int
mono_sql_connect ()
{
  MYSQL *ms;

  mysql_init (&mp);

  ms = mysql_real_connect(&mp, shm->mysql.host, shm->mysql.user, shm->mysql.pass, shm->mysql.base, 0, shm->mysql.sock, 0);

  if ( ms == NULL )
  {
      log_it ("sql", "could not connect to server (%s) !", mysql_error (&mp));
      return -1;
  }

  connected = TRUE;
  return 0;
}

int
mono_sql_detach ()
{
  mysql_close (&mp);
  connected = FALSE;
  return 0;
}

int
mono_sql_query (MYSQL_RES ** result, const char *format, ...)
{
  va_list ptr;
  int ret = 0;
  char *query = NULL;
  sigset_t set;
  size_t buffsize = SQL_QUERY_BUFF_INC;

  /* block signal during query */
  sigemptyset (&set);
  sigaddset(&set, SIGUSR1);
  sigaddset(&set, SIGIO);
  sigaddset (&set, SIGUSR2);
  sigaddset (&set, SIGINT);
  sigaddset (&set, SIGQUIT);
  /* sigaddset (&set, SIGTERM); */	/* May be nice to do a re-login if a query fails, yes? */
  if (sigprocmask (SIG_BLOCK, &set, NULL) < 0)
    perror ("sigprocmask");

  query = (char *) xmalloc (buffsize * sizeof (char));

  /*
   * create query string
   * check ret vs. buffsize too, coz glibc 2.1 seems to be broken
   * and returns the entire string length regardless.
   */
  do
    {
      if ((ret == -1) || (ret > buffsize))
	{
	  buffsize += SQL_QUERY_BUFF_INC;
	  query = (char *) xrealloc (query, buffsize * sizeof (char));
	}

      va_start (ptr, format);
      ret = vsnprintf (query, buffsize - 1, format, ptr);
      va_end (ptr);

    }
  while ((ret == -1) || (ret > buffsize));

  /*
   * Are we still connected to the server?
   */
  if (mysql_ping (&mp))
    {
      log_it ("sql_reconnect", "Reconnecting");
      if (mono_sql_connect ())
	{
	  xfree(query);
	  log_it ("sql_reconnect", "Reconnect failed.");
	  return -1;
	}
    }

#ifdef SQL_DEBUG
  fprintf (stderr, "sql query: %s\n", query);
#endif

  /* do the query */
  ret = mysql_real_query (&mp, query, strlen (query));

  if (sigprocmask (SIG_UNBLOCK, &set, NULL) < 0)
    perror ("sigprocmask");

  if (ret)
    {
#ifdef SQL_ERROR_SHIT_THAT_MAKES_USERS_WHINE
      fprintf (stderr, "errno: %d error: %s\n", mysql_errno (&mp),
	       mysql_error (&mp));
#endif
      log_it ("sqlerr", "%s", query);
      log_it ("sqlerr", "errno: %d error: %s", mysql_errno (&mp),
	      mysql_error (&mp));
      xfree (query);
      return -1;		/* no results */
    }
  if (logqueries == TRUE)
    log_it ("queries", "%s", query);

  xfree (query);

  *result = mysql_store_result (&mp);

  return 0;
}

void
mono_sql_u_free_result (MYSQL_RES * res)
{
	mono_sql_free_result( res );
}

void
mono_sql_free_result (MYSQL_RES * res)
{
  if (res == NULL)
    {
      log_it ("sqlmem", "Tried to free NULL value at 0x%x", res);
      return;
    }
  mysql_free_result (res);
  return;
}

int
mono_sql_connected ()
{
  return connected;
}

int
mono_sql_logqueries ()
{
  return logqueries;
}

int
mono_sql_logqueries_toggle ()
{
  logqueries = !logqueries;
  return logqueries;
}

const char *
mono_mysql_server_info ()
{
  return mysql_get_server_info (&mp);
}

const char *
mono_mysql_host_info ()
{
  return mysql_get_host_info (&mp);
}

/* this is a useful, if dangerous function */
/* it mallocs its own memroy. be sure to free this !! */
int
escape_string (const char *old_string, char **new_string)
{
  int i, j;
  size_t old_string_len;
  char *ns;

  if (!old_string)
    return -1;

  old_string_len = strlen (old_string);

  if (old_string_len < 1)
    {				/* empty string */
      ns = (char *) xmalloc (sizeof (char));
    }
  else
    {
      ns = (char *) xmalloc (old_string_len * 2 * sizeof (char));
    }

  if (ns == NULL)
    return -1;

  for (i = 0, j = 0; i < old_string_len; i++)
    {
      switch (old_string[i])
	{
/*	    case ',': you don't need to escape this, according ot he manual */
	case '"':
	case '\'':
	case '\\':
	  ns[j] = '\\';
	  j++;
	  break;
/* this is only for binary data! 
	    case 0:
		ns[j] = '\\';
		j++;
		ns[j] = '0';
		i++;j++;
*/
	}
      ns[j] = old_string[i];
      j++;
    }
  ns[j] = '\0';

  *new_string = ns;

  return 0;
}

int
mono_sql_test_connection (mysql_t *cfg)
{
  static MYSQL _tp;

  mysql_init (&_tp);

  if (!mysql_real_connect
      (&_tp, cfg->host, cfg->user, cfg->pass, cfg->base, 0, cfg->sock, 0))
    return 0;
  if (mysql_ping (&_tp))
    return 0;

  (void) mysql_close (&_tp);
  return 1;
}

/* eof */
