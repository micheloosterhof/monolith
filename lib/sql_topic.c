/* $Id$ */
/* operatoins we can do on topics */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>

#ifdef HAVE_MYSQL_H
  #undef HAVE_MYSQL_MYSQL_H
  #include <mysql.h>
#else
  #ifdef HAVE_MYSQL_MYSQL_H
    #undef HAVE_MYSQL_H
    #include <mysql/mysql.h>
  #endif
#endif

#include "routines.h"
#include "monolith.h"
#include "monosql.h"
#include "sql_utils.h"

#define extern
#include "sql_topic.h"
#undef extern

int
mono_sql_t_new_topic(unsigned int topic_id, topic_t * top)
{
    int ret;
    MYSQL_RES *res;
    char *esc_name = NULL;

    if (escape_string(top->name, &esc_name) != 0) {
	printf("could not escape topicname (#%u).\n", topic_id);
	return -1;
    }
    ret = mono_sql_query(&res, "INSERT INTO " T_TABLE
	 " (topic_id,name,forum) " "VALUES ( %u, '%s', '%u'" 
	   ,topic_id, esc_name, top->forum);

    xfree(esc_name);
    mysql_free_result(res);

    return ret;
}

int 
mono_sql_t_rename_topic( unsigned int topic_id, const char *newname )
{
    int ret;
    MYSQL_RES *res;
    char *esc_name = NULL;

    if (escape_string(newname, &esc_name) != 0) {
	fprintf( stderr, "could not escape topicname (#%u).\n", topic_id);
	return -1;
    }
    ret = mono_sql_query(&res, "UPDATE " T_TABLE
	 " SET name='%s'", esc_name );

    mysql_free_result(res);
    xfree(esc_name);

    return ret;
}

int
mono_sql_kill_topic(int topic_id)
{
    int ret;
    MYSQL_RES *res;

    ret = mono_sql_query(&res, "DELETE " T_TABLE " WHERE topic_id=%u",
			 topic_id);

    mysql_free_result(res);

    if (ret != 0) {
	return -1;
    }
    return 0;
}

/* eof */
