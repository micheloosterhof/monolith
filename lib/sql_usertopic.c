/* $Id$ */
/* todo: set datestamp */

/* for startes, what i want to store here is simple data concerning
 * ql ships */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <build-defs.h>

#include <stdio.h>

#ifdef USE_MYSQL
  #include MYSQL_HEADER
#endif

#include "monolith.h"
#include "sql_utils.h"
#include "sql_usertopic.h"

int
mono_sql_ut_update_lastseen( unsigned int user_id,  
                             unsigned int topic_id, unsigned int message_id )
{
    int ret;
    unsigned int dummy;
    MYSQL_RES *res;

    ret = mono_sql_ut_query_lastseen( user_id, topic_id, &dummy ) ;
    if ( ret == -1 ) {
         mono_sql_ut_add_entry( user_id, topic_id );
    }

    /* make this into an sql query that also checks if room & user exist */
    ret = mono_sql_query(&res,
			 "UPDATE " UT_TABLE " SET lastread=%u "
                         "WHERE topic_id=%u AND user_id=%u"
                , message_id, topic_id, user_id );
    mono_sql_u_free_result(res);

    return ret;
}

int
mono_sql_ut_query_lastseen( unsigned int user_id,
                             unsigned int topic_id, unsigned int *message_id )
{
    int ret;
    MYSQL_RES *res;
    MYSQL_ROW row;
    unsigned int m_id;

    /* make this into an sql query that also checks if room & user exist */
    ret = mono_sql_query(&res,
		 "SELECT lastread FROM " UT_TABLE " "
                 "WHERE topic_id=%u AND user_id=%u"
                , topic_id, user_id);

    if (ret != 0) {
	return -1;
    }
    if ( mysql_num_rows(res) == 0 ) {
        return -1;
    } else if ( mysql_num_rows(res) > 1 ) {
        mono_sql_u_free_result(res);
        return -1;
    }

    row = mysql_fetch_row(res);

    ret = sscanf( row[0], "%u", &m_id );
    
    *message_id = m_id;

    mono_sql_u_free_result(res);
    return ret;
}

int
mono_sql_ut_add_entry(unsigned int user_id, unsigned int topic_id)
{
    int ret;
    MYSQL_RES *res;

    /* make this into an sql query that also checks if room & user exist */
    ret = mono_sql_query(&res,
 		 "SELECT username FROM user "
      	         "WHERE user.id=%u"
       	, user_id);
    mono_sql_u_free_result(res);

    if (ret != 0) {
	return -1;
    }
    ret = mono_sql_query(&res, 
           "INSERT INTO " UT_TABLE 
	   " (user_id,topic_id) VALUES (%u,%u)"
         , user_id, topic_id);
    return ret;
}

int
mono_sql_ut_kill_topic( unsigned int topic_id )
{
    int ret;
    MYSQL_RES *res;

    ret = mono_sql_query(&res, "DELETE FROM " UT_TABLE 
          " WHERE topic_id=%u", topic_id );

    if (ret != 0) {
	fprintf(stderr, "Some sort of error.\n");
	return -1;
    }
    mono_sql_u_free_result(res);
    return 0;
}

int
mono_sql_ut_kill_user(unsigned int userid)
{
    int ret;
    MYSQL_RES *res;

    ret = mono_sql_query(&res, "DELETE FROM " UT_TABLE " WHERE user_id=%u", userid);

    if (ret != 0) {
	fprintf(stderr, "Some sort of error.\n");
	return -1;
    }
    mono_sql_u_free_result(res);
    return 0;
}

int
mono_sql_ut_new_user(unsigned int user_id)
{
    unsigned int topic_id;
    int ret;
    MYSQL_RES *res;

    for (topic_id = 0; topic_id < MAXQUADS; topic_id++) {
	ret = mono_sql_query(&res,
		   "INSERT INTO " UT_TABLE 
 		   "(user_id,topic_id) VALUES (%u,%u)" ,user_id, topic_id);
	if (ret == -1)
	    printf("\nInsert error");
    }
    mono_sql_u_free_result(res);
    return 0;
}

/* eof */
