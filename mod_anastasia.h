void anastasia_init(pool *p, server_rec *s);
void send_anastasia_error(request_rec *r, char *args, char *first_post_args, int inhtml);
void send_anastasia_index(request_rec *r, Tcl_Interp *tcl_interp, pool *current_pool);
void load_in_global_config_file(Tcl_Interp *the_interp, pool *this_pool, int initalisation);
//static int check_ana_access(request_rec *r);
#if defined(ANA_MYSQL)
 void log_connection(MYSQL *db_connection, char *bookname, char *username, char *conn_ip, int client_id, pool *current_pool, int perm, time_t time, char *file, int lineno);
#endif
