/* $Id$ */
/* message subsystem */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>

#ifdef HAVE_MYSQL_H
  #undef HAVE_MYSQL_MYSQL_H
  #include <mysql.h>
#else
  #ifdef HAVE_MYSQL_MYSQL_H
    #undef HAVE_MYSQL_H
    #include <mysql/mysql.h>
  #endif
#endif

#include "monolith.h"
#include "routines.h"
#include "monosql.h"
#include "sql_utils.h"
#include "sql_forum.h"
#include "sql_userforum.h"
#ifdef USE_RATING
  #include "sql_rating.h"
#endif

#include "sql_message.h"

static int _mono_sql_mes_add_mes_to_list(mlist_t entry, mlist_t ** list);
static message_t * _mono_sql_mes_row_to_mes(MYSQL_ROW row);

int
mono_sql_mes_add(message_t *message, unsigned int forum, const char *tmpfile)
{
    MYSQL_RES *res;
    int ret = 0;
    char *alias = NULL, *subject = NULL;

    if( (message->num = mono_sql_f_get_new_message_id(forum)) == 0)
        return -1;

    (void) copy(tmpfile, mono_sql_mes_make_file(forum, message->num));

    (void) escape_string(message->alias, &alias);
    (void) escape_string(message->subject, &subject);

    /*
     * We add the date here, so it really is the date the message was
     * added to the BBS. TIMESTAMP in the table will not be used, as it might
     * be updated. That's better used to check if a message was modified.
     */
    message->date = time(0);

    ret = mono_sql_query(&res, "INSERT INTO " M_TABLE " (message_id,topic_id,forum_id,author,alias,subject,date,type,priv,deleted) VALUES (%u,%u,%u,%u,'%s','%s',FROM_UNIXTIME(%u),'%s','%s','%c')",
        message->num, message->topic, message->forum, message->author,
        alias, subject, message->date, message->type,
        message->priv, message->deleted );

    (void) mysql_free_result(res);
    xfree(alias);
    xfree(subject);

#ifdef USE_RATING
    /*
     * To prevent an autor from uprating their own posts.
     */
    (void) mono_sql_rat_add_rating(message->author, message->num, message->forum, 0);
#endif

    return ret;

}

char *
mono_sql_mes_make_file(unsigned int forum, unsigned int number)
{
    static char filename[100];

    (void) snprintf(filename, 100, FILEDIR "/forum.%u/message.%u", forum, number);
    return filename;
}

int
mono_sql_mes_mark_deleted(unsigned int id, unsigned int forum)
{
    MYSQL_RES *res;
    int ret = 0;

    ret = mono_sql_query(&res, "UPDATE " M_TABLE " SET deleted='y' WHERE message_id=%u AND forum_id=%u", id, forum );

    (void) mysql_free_result(res);

    return ret;
}
    

int
mono_sql_mes_remove(unsigned int id, unsigned int forum)
{
    MYSQL_RES *res;
    int ret = 0;

    ret = mono_sql_query(&res, "DELETE FROM " M_TABLE " WHERE message_id=%u AND forum_id=%u", id, forum);
    (void) mysql_free_result(res);

    return ret;
}

int
mono_sql_mes_retrieve(unsigned int id, unsigned int forum, message_t *data)
{

    MYSQL_RES *res = NULL;
    MYSQL_ROW row;
    message_t message;
    int ret = 0;

    ret = mono_sql_query(&res, "SELECT message_id,topic_id,forum_id,author,alias,subject,UNIX_TIMESTAMP(date),type,priv,deleted FROM " M_TABLE " WHERE message_id=%u AND forum_id=%u", id, forum);

    if (ret == -1) {
 	(void) mysql_free_result(res);
	return -1;
    }
    if (mysql_num_rows(res) == 0) {
	(void) mysql_free_result(res);
	return -2;
    }
    if (mysql_num_rows(res) > 1) {
	(void) mysql_free_result(res);
	return -3;
    }
    row = mysql_fetch_row(res);

    /*
     * Determine size.
     */

    memset(&message, 0, sizeof(message_t));

    /*
     * Admin info
     */
    sscanf(row[0], "%u", &message.num);
    sscanf(row[1], "%u", &message.topic);
    sscanf(row[2], "%u", &message.forum);

    /*
     * Header info.
     */
    sscanf(row[3], "%u", &message.author);
    sprintf(message.alias, row[4]);
    sprintf(message.subject, row[5]);
    sscanf(row[6], "%lu", &message.date);

    /*
     * And more admin info.
     */
    sscanf(row[7], "%s", message.type);
    sscanf(row[8], "%s", message.priv);
    sscanf(row[9], "%c", &message.deleted);

#ifdef USE_RATING
    /*
     * Score!
     */
    message.score = mono_sql_rat_get_rating(message.num, message.forum);
#else
    message.score = 0;
#endif

    (void) mysql_free_result(res);

    *data = message;

    return 0;
}

/*
 * Returns a linked list containing messages in a forum.
 * Points both ways, for reading forward and backwards..
 */
int
mono_sql_mes_list_forum(unsigned int forum, unsigned int start, mlist_t ** list)
{

    MYSQL_RES *res;
    MYSQL_ROW row;
    int ret = 0, rows = 0, i = 0;
    mlist_t entry;
    message_t *message;


    /*
     * Coolest query in the BBS sofar :)
     */
#ifdef ALLOW_SUBQUERIES
    ret = mono_sql_query(&res, "SELECT m.message_id,m.topic_id,m.forum_id,m.author,m.alias,m.subject,UNIX_TIMESTAMP(m.date) AS date,m.type,m.priv,m.deleted,AVG(r.score) AS score FROM message AS m LEFT JOIN rating AS r ON m.message_id = r.message_id AND m.forum_id = m.forum_id WHERE m.message_id>%u AND m.forum_id=%u ORDER BY m.message_id", start, forum );
#else
    ret = mono_sql_query(&res, "SELECT message_id,topic_id,forum_id,author,alias,subject,UNIX_TIMESTAMP(date) AS date,type,priv,deleted FROM " M_TABLE " WHERE message_id>%u AND forum_id=%u ORDER BY message_id", start, forum);
#endif

    
    if (ret == -1) {
	(void) mysql_free_result(res);
	return -1;
    }
    if ((rows = mysql_num_rows(res)) == 0) {
	(void) mysql_free_result(res);
	return -2;
    }
    for (i = 0; i < rows; i++) {
	row = mysql_fetch_row(res);
	if (row == NULL)
	    break;
	/*
         * Get message and add to list.
         */
        if( (entry.message = _mono_sql_mes_row_to_mes(row)) == NULL ) {
            continue;
        }

	if( _mono_sql_mes_add_mes_to_list(entry, list) == -1 ) {
	    continue;
        }
    }
    mysql_free_result(res);
    return 0;
}

/*
 * Returns a linked list containing messages in a topic.
 * Points both ways, for reading forward and backwards..
 */
int
mono_sql_mes_list_topic(unsigned int topic, unsigned int start, mlist_t ** list)
{

    MYSQL_RES *res;
    MYSQL_ROW row;
    int ret = 0, rows = 0, i = 0;
    mlist_t entry;

#ifdef ALLOW_SUBQUERIES
    ret = mono_sql_query(&res, "SELECT m.message_id,m.topic_id,m.forum_id,m.author,m.alias,m.subject,UNIX_TIMESTAMP(m.date) AS date,m.type,m.priv,m.deleted,AVG(r.score) AS score FROM message AS m LEFT JOIN rating AS r ON m.message_id = r.message_id AND m.topic_id = m.topic_id WHERE m.message_id>%u AND m.topic_id=%u ORDER BY m.message_id", start, topic );
#else
    ret = mono_sql_query(&res, "SELECT message_id,topic_id,forum_id,author,alias,subject,UNIX_TIMESTAMP(date) AS date,type,priv,deleted FROM " M_TABLE " WHERE message_id>%u AND topic_id=%u ORDER BY message_id", start, topic);
#endif

    if (ret == -1) {
	(void) mysql_free_result(res);
	return -1;
    }
    if ((rows = mysql_num_rows(res)) == 0) {
	(void) mysql_free_result(res);
	return -1;
    }
    for (i = 0; i < rows; i++) {
	row = mysql_fetch_row(res);
	if (row == NULL)
	    break;
	/*
         * Get message and add to list.
         */
        if( (entry.message = _mono_sql_mes_row_to_mes(row)) == NULL ) {
            break;
        }
	if (_mono_sql_mes_add_mes_to_list(entry, list) == -1)
	    break;
    }
    mysql_free_result(res);
    return 0;

}

/*
 * Add an entry to the message linked list.
 * Provide support for going in both directions.
 *
 * God, I HATE linked lists.
 */
static int
_mono_sql_mes_add_mes_to_list(mlist_t entry, mlist_t ** list)
{

    mlist_t *p, *q;

    /*
     * Note mono_sql_free_list()
     */
    p = (mlist_t *) xmalloc(sizeof(mlist_t));
    if (p == NULL)
	return -1;

    *p = entry;
    p->next = NULL;
    p->prev = NULL;

    q = *list;
    if (q == NULL) {
	*list = p;
    } else {
	while (q->next != NULL)
	    q = q->next;
	q->next = p;
        p->prev = q;
    }
    return 0;
}

void
mono_sql_mes_free_list(mlist_t *list)
{

    mlist_t *ref;

    while (list != NULL) {
        ref = list->next;
        (void) xfree(list->message);
        (void) xfree(list);
        list = ref;
    }
    return;
}

message_t *
_mono_sql_mes_row_to_mes(MYSQL_ROW row)
{
    message_t *message = NULL;

    message = (message_t *) xmalloc(sizeof(message_t));
    memset(message, 0, sizeof(message_t));

    /*
     * Admin info
     */
    sscanf(row[0], "%u", &message->num);
    sscanf(row[1], "%u", &message->topic);
    sscanf(row[2], "%u", &message->forum);

    /*
     * Header info.
     */
    sscanf(row[3], "%u", &message->author);
    sprintf(message->alias, row[4]);
    sprintf(message->subject, row[5]);
    sscanf(row[6], "%lu", &message->date);

    /*
     * And more admin info.
     */
    sscanf(row[7], "%s", &message->type);
    sscanf(row[8], "%s", &message->priv);
    sscanf(row[9], "%c", &message->deleted);

#ifdef USE_RATING
  #ifdef ALLOW_SUBQUERIES
    sscanf(row[10], "%f", &message->score);
  #else
    message->score = mono_sql_rat_get_rating(message->num, message->forum);
  #endif
#else
    message->score = 0;
#endif

    return message;
}

/* eof */
