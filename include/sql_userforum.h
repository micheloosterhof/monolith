extern int mono_sql_uf_add_entry(unsigned int , unsigned int );
extern int mono_sql_uf_list_hosts_by_forum(unsigned int, userlist_t **p );
extern int mono_sql_uf_list_hosts_by_user(unsigned int, forumlist_t **p );
extern int mono_sql_uf_is_host(unsigned int , unsigned int );
extern int mono_sql_uf_add_host(unsigned int , unsigned int );
extern int mono_sql_uf_remove_host(unsigned int , unsigned int );
extern int mono_sql_uf_kill_forum(unsigned int );
extern int mono_sql_uf_kill_user(unsigned int );
extern void convert_quick(void);
extern int mono_sql_uf_list_invited_by_forum(unsigned int, userlist_t **p );
extern int mono_sql_uf_list_invited_by_user(unsigned int, forumlist_t **p );
extern int mono_sql_uf_is_invited(unsigned int , unsigned int );
extern int mono_sql_uf_add_invited(unsigned int , unsigned int );
extern int mono_sql_uf_remove_invited(unsigned int , unsigned int );
extern int mono_sql_uf_list_kicked_by_forum(unsigned int, userlist_t **p );
extern int mono_sql_uf_list_kicked_by_user(unsigned int, forumlist_t **p );
extern int mono_sql_uf_is_kicked(unsigned int , unsigned int );
extern int mono_sql_uf_add_kicked(unsigned int , unsigned int );
extern int mono_sql_uf_remove_kicked(unsigned int , unsigned int );
extern int mono_sql_uf_new_user(unsigned int);
extern int dest_userlist(userlist_t * );

#define UF_TABLE "userforum"
