/* $Id$ */
/* todo: set datestamp */

/* for startes, what i want to store here is simple data concerning
 * ql ships */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <mysql.h>

#include "monolith.h"
#include "libmono.h"
#include "monosql.h"
#include "sql_utils.h"
#include "sql_userforum.h"
#include "btmp.h"

#define UF_TABLE "userforum"

int
add_to_userlist(userlist_t element, userlist_t ** list)
{
    userlist_t *p, *q;

    p = (userlist_t *) xmalloc(sizeof(userlist_t));
    if (p == NULL)
        return -1;

    /* copy, and set as last element */
    *p = element;
    p->next = NULL;

    /* find last element in list */
    q = *list;
    if (q == NULL) {            /* empty list */
        *list = p;              /* put that in as first element */
    } else {
        while (q->next != NULL)
            q = q->next;
        q->next = p;
    }
    return 0;
}

int
dest_userlist( userlist_t *list ) 
{
    userlist_t *p, *q;

    p = list;
    while (p) {
        q = p->next;
        xfree(p);
        p = q;
    }
    return 0;
}

int
add_to_forumlist(forumlist_t element, forumlist_t ** list)
{
    forumlist_t *p, *q;

    p = (forumlist_t *) xmalloc(sizeof(forumlist_t));
    if (p == NULL)
        return -1;

    /* copy, and set as last element */
    *p = element;
    p->next = NULL;

    /* find last element in list */
    q = *list;
    if (q == NULL) {            /* empty list */
        *list = p;              /* put that in as first element */
    } else {
        while (q->next != NULL)
            q = q->next;
        q->next = p;
    }
    return 0;
}
int
mono_sql_uf_add_entry(unsigned int user_id, unsigned int forum_id)
{
    int ret;
    MYSQL_RES *res;

    /* make this into an sql query that also checks if room & user exist */
    ret = mono_sql_query(&res, 
        "SELECT user_id,username FROM userforum"
        "WHERE forum_id=%u AND u.user_id=%u", forum_id, user_id );
    mysql_free_result(res);

    if (ret != 0) {
	return -1;
    }
    ret = mono_sql_query(&res, "INSERT INTO " UF_TABLE " (user_id,forum_id) VALUES (%u,%u)", user_id, forum_id);
    mysql_free_result(res);
    return ret;
}

int
mono_sql_uf_list_hosts_by_forum(unsigned int forumnumber, userlist_t **p )
{
    int ret, rows, i;
    unsigned int user_id;
    userlist_t e;
    MYSQL_RES *res;
    MYSQL_ROW row;

    ret = mono_sql_query(&res, 
          "SELECT u.id,u.username " 
          "FROM user u,userforum uf "
          "WHERE forum_id=%u AND uf.user_id=u.user_id AND host='y'"
        , forumnumber);

    if (ret == -1) {
	return -1;
    }
    rows = mysql_num_rows(res);
     
    *p = NULL;

    for (i = 0; i < rows; i++) {
	row = mysql_fetch_row(res);
	if (row == NULL)
	    break;

	if (sscanf(row[0], "%u", &user_id) == -1)
	    continue;
        
        e.usernum = user_id;
        strcpy( e.name, row[1] ); 
        add_to_userlist( e, p );

	fprintf( stderr, "host usernumber: %u\n", user_id);
	/* do something ! */
    }
    mysql_free_result(res);
    return 0;
}

int
mono_sql_uf_list_hosts_by_user(unsigned int usernumber, forumlist_t **p )
{
    int ret, rows, i;
    unsigned int forum_id;
    forumlist_t e;
    MYSQL_RES *res;
    MYSQL_ROW row;

    ret = mono_sql_query(&res, "SELECT forum_id FROM " UF_TABLE " WHERE user_id=%u AND host='y'", usernumber);

    if (ret == -1) {
	return -1;
    }
    rows = mysql_num_rows(res);
   
    *p = NULL;

    for (i = 0; i < rows; i++) {
	row = mysql_fetch_row(res);
	if (row == NULL)
	    break;
	if (sscanf(row[0], "%u", &forum_id) == -1)
	    continue;

        e.forumnum = forum_id;
        /* strcpy( e.forumname, row[1] ); */
        add_to_forumlist( e, p );

	printf("host forumnumber: %u\n", forum_id);
	/* do something ! */
    }
    mysql_free_result(res);
    return 0;
}

int
mono_sql_uf_is_host(unsigned int usernumber, unsigned int forumnumber)
{
    int ret, rows;
    MYSQL_RES *res;

    ret = mono_sql_query(&res, "SELECT count(*) FROM " UF_TABLE " WHERE (user_id='%u' AND forum_id='%u' AND host='y')", usernumber, forumnumber);

    if (ret == -1) {
	fprintf(stderr, "No results from query.\n");
	return FALSE;
    }
    rows = mysql_num_rows(res);
    if (rows != 1) {
	fprintf(stderr, "internal error\n");
	return FALSE;
    }
    mysql_free_result(res);
    return TRUE;
}

int
mono_sql_uf_add_host(unsigned int user_id, unsigned int forum_id)
{
    int ret;
    MYSQL_RES *res;

    ret = mono_sql_query(&res, "UPDATE " UF_TABLE " SET "
			 " host='y' WHERE user_id=%u AND forum_id=%u"
			 ,user_id, forum_id);
    mysql_free_result(res);
    if (ret != 0) {
	fprintf(stderr, "Some sort of error (addhost).\n");
	return -1;
    }
    mysql_free_result(res);
    return 0;
}

int
mono_sql_uf_remove_host(unsigned int usernumber, unsigned int forumnumber)
{
    int ret;
    MYSQL_RES *res;

    ret = mono_sql_query(&res, "UPDATE " UF_TABLE " SET host='n' WHERE (user_id='%d' AND forum_id='%d')", usernumber, forumnumber);

    if (ret != 0) {
	fprintf(stderr, "Some sort of error.\n");
	return -1;
    }
    mysql_free_result(res);
    return 0;
}

int
mono_sql_uf_kill_forum(unsigned int forumnumber)
{
    int ret;
    MYSQL_RES *res;

    ret = mono_sql_query(&res, "DELETE FROM " UF_TABLE " WHERE (forum_id='%d')", forumnumber);

    if (ret != 0) {
	fprintf(stderr, "Some sort of error.\n");
	return -1;
    }
    mysql_free_result(res);
    return 0;
}

int
mono_sql_uf_kill_user(unsigned int userid)
{
    int ret;
    MYSQL_RES *res;

    ret = mono_sql_query(&res, "DELETE FROM " UF_TABLE " WHERE (user_id='%d')", userid);

    if (ret != 0) {
	fprintf(stderr, "Some sort of error.\n");
	return -1;
    }
    mysql_free_result(res);
    return 0;
}

int
mono_sql_uf_list_invited_by_forum(unsigned int forumnumber, userlist_t **p )
{
    int ret, rows, i;
    unsigned int user_id;
    userlist_t e;
    MYSQL_RES *res;
    MYSQL_ROW row;

    ret = mono_sql_query(&res, 
          "SELECT u.id,u.username " 
          "FROM user u,userforum uf "
          "WHERE forum_id=%u AND uf.user_id=u.user_id AND status='invited'"
        , forumnumber);

    if (ret == -1) {
	return -1;
    }
    rows = mysql_num_rows(res);
   
    *p = NULL;

    for (i = 0; i < rows; i++) {
	row = mysql_fetch_row(res);
	if (row == NULL)
	    break;

	if (sscanf(row[0], "%u", &user_id) == -1)
	    continue;

        e.usernum = user_id;
        strcpy( e.name, row[1] ); 
        add_to_userlist( e, p );

	printf("debug: host usernumber: %u\n", user_id);
    }
    mysql_free_result(res);
    return 0;
}

int
mono_sql_uf_list_invited_by_user(unsigned int usernumber, forumlist_t **p )
{
    int ret, rows, i;
    unsigned int forum_id;
    forumlist_t e;
    MYSQL_RES *res;
    MYSQL_ROW row;

    ret = mono_sql_query(&res, "SELECT forum_id FROM " UF_TABLE
		      " WHERE user_id=%u AND status='invited'", usernumber);

    if (ret == -1) {
	return -1;
    }
    rows = mysql_num_rows(res);
   
    *p = NULL;

    for (i = 0; i < rows; i++) {
	row = mysql_fetch_row(res);
	if (row == NULL)
	    break;
	if (sscanf(row[0], "%u", &forum_id) == -1)
	    continue;

        e.forumnum = forum_id;
        /* strcpy( e.forumname, row[1] ); */
        add_to_forumlist( e, p );

	printf("invited forumnumber: %u\n", forum_id);
	/* do something ! */
    }
    mysql_free_result(res);
    return 0;
}

int
mono_sql_uf_is_invited(unsigned int usernumber, unsigned int forumnumber)
{
    int ret, rows;
    MYSQL_RES *res;

    ret = mono_sql_query(&res, "SELECT count(*) FROM " UF_TABLE
			 " WHERE (user_id='%u' AND forum_id='%u' AND status='invited' )", usernumber, forumnumber);

    if (ret == -1) {
	fprintf(stderr, "No results from query.\n");
	return FALSE;
    }
    rows = mysql_num_rows(res);
    if (rows != 1) {
	fprintf(stderr, "internal error\n");
	mysql_free_result(res);
	return FALSE;
    }
    mysql_free_result(res);
    return TRUE;
}

int
mono_sql_uf_add_invited(unsigned int user_id, unsigned int forum_id)
{
    int ret;
    MYSQL_RES *res;

    ret = mono_sql_query(&res, "UPDATE " UF_TABLE " SET "
			 " status='invited' WHERE user_id=%u AND forum_id=%u"
			 ,user_id, forum_id);

    mysql_free_result(res);
    if (ret != 0) {
	fprintf(stderr, "Some sort of error (addinvited).\n");
	return -1;
    }
    return 0;
}

int
mono_sql_uf_remove_invited(unsigned int usernumber, unsigned int forumnumber)
{
    int ret;
    MYSQL_RES *res;

    ret = mono_sql_query(&res, "UPDATE " UF_TABLE " SET status='normal' WHERE (user_id='%d' AND forum_id='%d')", usernumber, forumnumber);

    if (ret != 0) {
	fprintf(stderr, "Some sort of error.\n");
	return -1;
    }
    mysql_free_result(res);
    return 0;
}


int
mono_sql_uf_list_kicked_by_forum(unsigned int forumnumber, userlist_t **p )
{
    int ret, rows, i;
    unsigned int user_id;
    userlist_t e;
    MYSQL_RES *res;
    MYSQL_ROW row;

    ret = mono_sql_query(&res, 
          "SELECT u.id,u.username " 
          "FROM user u,userforum uf "
          "WHERE forum_id=%u AND uf.user_id=u.id AND status='kicked'"
        , forumnumber);

    printf( "done query" );fflush(stdout);

    if (ret == -1) {
        printf( "no results" );fflush(stdout);
	return -1;
    }
    rows = mysql_num_rows(res);
   
    *p = NULL;
    printf( "got results" );fflush(stdout);

    for (i = 0; i < rows; i++) {
	row = mysql_fetch_row(res);
	if (row == NULL)
	    break;

	if (sscanf(row[0], "%u", &user_id) == -1)
	    continue;

	fprintf( stderr, "debug: kicked usernumber: %u\n", user_id);

        e.usernum = user_id;
        strcpy( e.name, row[1] ); 
        add_to_userlist( e, p );

    }
    mysql_free_result(res);
    return 0;
}

int
mono_sql_uf_list_kicked_by_user(unsigned int usernumber, forumlist_t **p )
{
    int ret, rows, i;
    unsigned int forum_id;
    forumlist_t e;
    MYSQL_RES *res;
    MYSQL_ROW row;

    ret = mono_sql_query(&res, "SELECT forum_id FROM " UF_TABLE " WHERE user_id=%u AND status='kicked'", usernumber);

    if (ret == -1) {
	return -1;
    }
    rows = mysql_num_rows(res);
   
    *p = NULL;

    for (i = 0; i < rows; i++) {
	row = mysql_fetch_row(res);
	if (row == NULL)
	    break;
	if (sscanf(row[0], "%u", &forum_id) == -1)
	    continue;

        e.forumnum = forum_id;
        /* strcpy( e.forumname, row[1] ); */
        add_to_forumlist( e, p );

	printf("kicked forumnumber: %u\n", forum_id);
	/* do something ! */
    }
    mysql_free_result(res);
    return 0;
}

int
mono_sql_uf_is_kicked(unsigned int user_id, unsigned int forum_id)
{
    int ret, rows;
    MYSQL_RES *res;

    ret = mono_sql_query(&res, "SELECT count(*) FROM " UF_TABLE " WHERE (user_id='%u' AND forum_id='%u' AND status='kicked')", user_id, forum_id);

    if (ret == -1) {
	fprintf(stderr, "No results from query.\n");
	return FALSE;
    }
    rows = mysql_num_rows(res);
    if (rows != 1) {
	fprintf(stderr, "internal error\n");
	mysql_free_result(res);
	return FALSE;
    }
    mysql_free_result(res);
    return TRUE;
}

int
mono_sql_uf_add_kicked(unsigned int user_id, unsigned int forum_id)
{
    int ret;
    MYSQL_RES *res;

    ret = mono_sql_query(&res, "UPDATE " UF_TABLE " SET "
			 " status='kicked' WHERE user_id=%u AND forum_id=%u"
			 ,user_id, forum_id);
    mysql_free_result(res);

    if (ret != 0) {
	fprintf(stderr, "Some sort of error (addkicked).\n");
	return -1;
    }
    return 0;
}

int
mono_sql_uf_remove_kicked(unsigned int usernumber, unsigned int forumnumber)
{
    int ret;
    MYSQL_RES *res;

    ret = mono_sql_query(&res, "UPDATE " UF_TABLE " SET status='normal' WHERE (user_id='%d' AND forum_id='%d')", usernumber, forumnumber);

    if (ret != 0) {
	fprintf(stderr, "Some sort of error.\n");
	return -1;
    }
    mysql_free_result(res);
    return 0;
}


/* eof */
