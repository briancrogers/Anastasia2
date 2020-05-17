#include "ana_common.h"
#include "mod_anastasia.h"
#include "ana_browsegrove.h"
#include "ana_utils.h"
#include <apr_strings.h>
#include <mod_auth.h>
#if defined(ANASTANDALONE)
  #error "ANASTANDALONE defined!!"
#endif
//coz this is not included in stand alone (ANASTANDALONE must be undefined in ana_config.h)


long _Tcl_Eval(request_rec *r, Tcl_Interp *t, const char *s) {
        long retVal = Tcl_Eval(t, s);
//        ap_rprintf(r, "Tcl_Eval(\"%s\"): %ld<br/>", s, retVal);
 	return retVal;
 }
 
  
 
void anastasia_init(pool *server_pool, server_rec *s);
int ana_error_callback(void *rec, const char *key, const char *value);
static const char *set_ana_access(cmd_parms *params, void *mconfig, int flag);
static const char *set_ana_access_log(cmd_parms *params, void *mconfig, const char *flag);
static const char *set_anastasia_root(cmd_parms *params, void *mconfig, const char *ana_root);
static const char *set_anastasia_url(cmd_parms *params, void *mconfig, const char *ana_url);
static const char *set_ana_db_default(cmd_parms *params, void *mconfig, int flag);
static const char *set_ana_db_access(cmd_parms *params, void *mconfig, const char *host, const char *database);
static const char *set_ana_db_access2(cmd_parms *params, void *mconfig, const char *username, const char *password);

static int anastasia_handler(request_rec *r) {

    if (strcmp(r->handler, "anastasia-handler")) {
        return DECLINED;
    }

 char *args, *first_post_args=NULL, *http_post_args, *targs, *str, *tstr;
 char *argv[4];
 pool *current_pool = r->pool;
 Tcl_Interp *tcl_interp;
 int return_code = false;
 r->content_type = "text/html";
 tcl_interp = Tcl_CreateInterp();
 Tcl_Preserve(tcl_interp);
 Tcl_FindExecutable(NULL);
 args = apr_pstrdup(current_pool, r->args);
 //write_log("starting point 1");
 //Hmm perhaps we should remove the %20 and stuff like that before we go any further.
 if (args==NULL) {
  args = apr_pstrdup(current_pool, "");
 } else {
  tstr = apr_pstrdup(current_pool, args);
  ap_unescape_url(tstr);
 }
 targs = args;
 //Need to parse the args into the 3 main request for anareader, and shove the rest into the http_post string
 // write_log("starting point 2");
if ((return_code=ap_setup_client_block(r, REQUEST_CHUNKED_ERROR))!=OK) {
  return return_code;
 }
 if (ap_should_client_block(r)) {
  char argsbuffer[HUGE_STRING_LEN];
  int rsize, len_read, rpos=0;
  long length = r->remaining;
  char *rbuf = (char *) apr_pcalloc(current_pool, length+1);
//NOTE:Docs say simply remove these  ap_hard_timeout("anareader_handler", r);
  while ((len_read = ap_get_client_block(r, argsbuffer, sizeof(argsbuffer))) > 0) {
//NOTE:Docs say simply remove these   ap_reset_timeout(r);
   if ((rpos + len_read) > length) {
    rsize = length - rpos;
   } else {
    rsize = len_read;
   }
   memcpy((char*) rbuf + rpos, argsbuffer, rsize);
   rpos += rsize;
  }
//NOTE:Docs say simply remove these  ap_kill_timeout(r);
  first_post_args=apr_pstrdup(current_pool,rbuf);  //post args might come in chunks: add later in divideParam to get full post_args
 }
 //write_log("starting point 3");
 //we are ready.  Go get.
 if (strcmp(args, "")!=0 && strlen(args)>1) {
	  //O.k. just pass in the 3 needed parameters bookname, element number, anv file.
//	  write_log("starting point 4");
	  argv[1] = ap_getword_nc(current_pool, &targs, '+');
	  argv[2] = ap_getword_nc(current_pool, &targs, '+');
      argv[3] = ap_getword_nc(current_pool, &targs, '+');
	  if (strcmp(argv[3],"")==0) {
		   argv[3] = apr_pstrdup(current_pool, targs);
	  }
	  if (strcmp(targs, "")==0) {
			http_post_args = apr_pstrdup(current_pool, first_post_args);
	  } else {
			http_post_args = apr_pstrcat(current_pool, targs, first_post_args, NULL);
	   }
	   str = apr_pstrdup(current_pool, http_post_args);
	   if (str!=NULL) {
		 reform_http_string(http_post_args, str);
							  ap_unescape_url(str);						
					}
	  //Reload the global config file
	  load_in_global_config_file(tcl_interp, current_pool, 0);
	  //Now what evers felt can be put into the http_post variable along with the first_post_args
	  if (!make_reader_file(argv, r, str, NULL, current_pool)) {
	   //There was an error so send the error message
	   //send_anastasia_error(r, args, first_post_args, 1);
	  }
 } else {
 //	write_log("starting point 5");
	  load_in_global_config_file(tcl_interp, current_pool, 0);
	  //No args so either send error or index page
	#if defined(_ANA_CDROM_)
	   send_anastasia_index(r, tcl_interp, current_pool);
	#else
	  str = (char *) Tcl_GetVar(tcl_interp, "show_index", 0);
	  if (str!=NULL && strcmp("1", str)==0) {
	   send_anastasia_index(r, tcl_interp, current_pool);
	  } else {
	   send_anastasia_error(r, args, first_post_args, 0);
	  }
	#endif
 }
 Tcl_Release(tcl_interp);
 Tcl_DeleteInterp(tcl_interp);
 return OK;
}

/* Dispatch list of content handlers */
/*
static handler_rec anastasia_handlers[] = {
 {"anastasia-handler", anastasia_handler},
 {NULL}
};
*/

/* List of commands */
static const command_rec anastasia_cmds[] = {
 AP_INIT_FLAG("DBAuthorise", set_ana_access, NULL, ACCESS_CONF, "either off or on to toggle authorisation"),
 AP_INIT_TAKE1("DBAuthLog", set_ana_access_log, NULL, ACCESS_CONF, "either 0,1,2 to control logging of authorisation procedure"),
 AP_INIT_FLAG("DBDefaultDeny", set_ana_db_default, NULL, ACCESS_CONF, "either off or on to toggle authorisation"),
 AP_INIT_TAKE2("DBAccess", set_ana_db_access, NULL, ACCESS_CONF, "hostname and database"),
 AP_INIT_TAKE2("DBAccess2", set_ana_db_access2, NULL, ACCESS_CONF, "username and password"),
 AP_INIT_TAKE1("AnastasiaRoot", set_anastasia_root, NULL, ACCESS_CONF, "the root directory containing all anastasia books"),
 AP_INIT_TAKE1("AnastasiaURL", set_anastasia_url, NULL, ACCESS_CONF, "the url required for Anastasia"),
 {NULL}
};

static void anastasia_register_hooks(apr_pool_t *p)
{
//    ap_hook_pre_config(php_pre_config, NULL, NULL, APR_HOOK_MIDDLE);
//    ap_hook_post_config(php_apache_server_startup, NULL, NULL, APR_HOOK_MIDDLE);
    ap_hook_handler(anastasia_handler, NULL, NULL, APR_HOOK_MIDDLE);
    ap_hook_child_init(anastasia_init, NULL, NULL, APR_HOOK_MIDDLE);

}


module AP_MODULE_DECLARE_DATA anastasia_module = {
    STANDARD20_MODULE_STUFF,
    NULL,                  /* create per-dir    config structures */
    NULL,                  /* merge  per-dir    config structures */
    NULL,                  /* create per-server config structures */
    NULL,                  /* merge  per-server config structures */
    anastasia_cmds,        /* table of config file commands       */
    anastasia_register_hooks  /* register hooks                      */
};

#if 0
/* Dispatch list for API hooks */
module MODULE_VAR_EXPORT anastasia_module = {
    STANDARD_MODULE_STUFF, 
    anastasia_init,   /* module initializer                  */
    NULL,     /* create per-dir    config structures */
    NULL,     /* merge  per-dir    config structures */
    NULL,     /* create per-server config structures */
    NULL,     /* merge  per-server config structures */
    anastasia_cmds,   /* table of config file commands       */
    anastasia_handlers,  /* [#8] MIME-typed-dispatched handlers */
    NULL,     /* [#1] URI to filename translation    */
    check_ana_access,  /* [#4] validate user id from request  */
    check_ana_access,  /* [#5] check if the user is ok _here_ */
    NULL,     /* [#3] check access by host address   */
    NULL,     /* [#6] determine MIME type            */
    NULL,     /* [#7] pre-run fixups                 */
    NULL,     /* [#9] log a transaction              */
    NULL,     /* [#2] header parser                  */
    NULL,     /* child_init                          */
    NULL,     /* child_exit                          */
    NULL     /* [#0] post read-request              */
};
#endif

void send_anastasia_error(request_rec *r, char *args, char *first_post_args, int inhtml) {
 const char *str;
 if (!inhtml){
  ap_rputs("<html><head><title>Error message for Anastasia</title>", r);
  ap_rputs("<link rel=\"stylesheet\" href=\"ana_error.css\" type=\"text/css\"></head><body>", r);
 }
 ap_rputs("<div type=\"title\">Error message for Anastasia </div>\n", r);
 if (args==NULL) {
  ap_rprintf(r, "<div type=\"errorline\">Requested: No Args.</div>\n");
 } else {
  ap_rprintf(r, "<div type=\"errorline\">Requested: \"%s\"</div>\n", args);
 }
 ap_rprintf(r, "<div type=\"errorline\">When: \"%s\"</div>\n", ap_ht_time(r->pool, time(NULL), "%T", 0));
 if (first_post_args==NULL) {
  ap_rputs("<div type=\"errorline\">No first_post_args.</div>\n", r);
 } else {
  ap_rprintf(r, "<div type=\"errorline\">first_post_args is \"%s\"</div>\n", first_post_args);
 }
 str = apr_table_get(r->headers_in, "Cookie");
 if (str==NULL || strcmp(str, "")==0) {
  ap_rputs("<div type=\"errorline\">No Cookie</div>", r);
 } else {
  ap_rprintf(r, "<div type=\"errorline\">Cookie = %s</div>", str);
 }
 apr_table_do(ana_error_callback, r, r->headers_in, NULL);
 ap_rputs("</body></html>\n", r);
}
void send_anastasia_index(request_rec *r, Tcl_Interp *tcl_interp, pool *current_pool) {
 int result;
 char *tcl_result, *searchid;
#if defined(_ANA_CDROM_)
 char str[1024], *booktitle, *anaStart, *tmp_str;
 int length, a, b;
#endif
 //O.k. lets first print out the start of the html
 ap_rputs("<html><head><title>Anastasia books on this server</title>", r);
 ap_rputs("<link rel=\"stylesheet\" href=\"ana_index.css\" type=\"text/css\"></head>", r);
#if defined(_ANA_CDROM_)
 ap_rputs("<body bgcolor=#FAE6C3><div align=\"center\"><img src=\"Reader/htdocs/sdetrans50.gif\">", r);
#else
 ap_rputs("<body bgcolor=#FAE6C3><div align=\"center\"><img src=\"sdetrans50.gif\">", r);
#endif
 ap_rputs("<h1><div type=\"title\">Anastasia books on this server.</div></h1><dir>\n", r);
 //Now loop through each book in turn and print out a link to each one
#if defined(_ANA_CDROM_)
 //Loop through all the directories and then check each directory for a AnaRead.cfg file
 sprintf(str, "set thisdir \"%s\"", anastasia_info.path);
 result = _Tcl_Eval(r, tcl_interp, str);
 tcl_result = (char *) Tcl_GetVar(tcl_interp, "thisdir", 0);
 //Drop back a directory
 tmp_str = (char *) apr_palloc(current_pool, strlen(tcl_result)+1024);
 ap_make_dirstr_prefix(tmp_str, tcl_result, ap_count_dirs(tcl_result));
 sprintf(str, "cd \"%s\"", tmp_str);
 _Tcl_Eval(r, tcl_interp, str);
 //Get all the directorys out of this directory.. then remove reader and AnastasiaCD.app
 sprintf(str, "set dirs [glob -type d *]");
 _Tcl_Eval(r, tcl_interp, str);
 sprintf(str, "llength $dirs");
 _Tcl_Eval(r, tcl_interp, str);
 tcl_result = (char *) Tcl_GetStringResult(tcl_interp);
 length = atoi(tcl_result);
 tcl_result = (char *) Tcl_GetVar(tcl_interp, "dirs", 0);
 for (a=0; a<length; a++) {
  Tcl_SetVar(tcl_interp, "booktitle", "", 0);
  Tcl_SetVar(tcl_interp, "anaStart", "", 0);
  //Check the directory
  sprintf(str, "file exist \"[file join \"%s\" [lindex $dirs %d] AnaGrove [lindex $dirs %d].idx]\"", tmp_str, a, a);
  _Tcl_Eval(r, tcl_interp, str);
  tcl_result = (char *) Tcl_GetStringResult(tcl_interp);
  b =atoi(tcl_result);
  if (b==1) {
   sprintf(str, "set afilename \"[file join \"%s\" [lindex $dirs %d] AnaGrove AnaRead.cfg]\"", tmp_str, a);
   _Tcl_Eval(r, tcl_interp, str);
   tcl_result = (char *) Tcl_GetVar(tcl_interp, "afilename", 0);
   strcpy(str, tcl_result);
   Tcl_EvalFile(tcl_interp, str);
   booktitle = (char *) Tcl_GetVar(tcl_interp, "booktitle", 0);
   anaStart = (char *) Tcl_GetVar(tcl_interp, "anaStart", 0);
   sprintf(str, "lindex $dirs %d", a);
   _Tcl_Eval(r, tcl_interp, str);
   tcl_result = (char *) Tcl_GetStringResult(tcl_interp);
   if (strcmp("", anaStart)==0 && strcmp("", booktitle)==0) {
    ap_rprintf(r, "<div type=\"book\"><a href=\"http://%s:%d%s?", apr_pstrdup(current_pool, r->hostname), r->server->port, apr_pstrdup(current_pool, apr_pstrdup(current_pool, r->parsed_uri.path)));
    ap_rprintf(r, "%s+0+start.anv\">%s</a></div>\n", tcl_result, tcl_result);
   } else if (strcmp("", booktitle)==0) {
    ap_rprintf(r, "<div type=\"book\"><a href=\"http://%s:%d?%s\">%s</a></div>\n", apr_pstrdup(current_pool, r->hostname), r->server->port, apr_pstrdup(current_pool, r->parsed_uri.path), anaStart);
   } else {
    ap_rprintf(r, "<div type=\"book\"><a href=\"http://%s:%d/%s\">%s</a></div>\n", apr_pstrdup(current_pool, r->hostname), r->server->port, anaStart, booktitle);
   }
  }
 }
 if (length==0) {
  //There are no books about...
  ap_rprintf(r, "<div type=\"book\">There are currently no books available on this server.</div>");
 }
#else
 result = _Tcl_Eval(r, tcl_interp, "array startsearch books");
 if (result==TCL_OK) {
  searchid = apr_pstrdup(current_pool, Tcl_GetStringResult(tcl_interp));
  while (result==TCL_OK) {
   tcl_result = apr_psprintf(current_pool, "array nextelement books %s", searchid);
   result = _Tcl_Eval(r, tcl_interp, tcl_result);
   if (result==TCL_OK) {
    tcl_result = (char *) Tcl_GetStringResult(tcl_interp);
    if (tcl_result!=NULL && strcmp(tcl_result, "")!=0) {
     ap_rprintf(r, "<div type=\"book\"><a href=\"http://%s%s?", apr_pstrdup(current_pool, r->hostname), apr_pstrdup(current_pool, r->parsed_uri.path));
     ap_rprintf(r, "%s+0+start.anv\">%s</a></div>\n", tcl_result, tcl_result);
    } else {
     result=TCL_ERROR;
    }
   }
  }
  _Tcl_Eval(r, tcl_interp, apr_psprintf(current_pool, "array donesearch books %s", searchid));
 } else {
  ap_rprintf(r, "<div type=\"book\">There are currently no books available on this server.</div>");
 }
#endif
 ap_rputs("</dir></div></body></html>\n",r);
}
int ana_error_callback(void *rec, const char *key, const char *value) {
 ap_rprintf((struct request_rec *) rec, "<div type=\"errorline\">%s is \"%s\"</div>\n", key, value);
 return 1;
};

static const char *set_ana_access(cmd_parms *params, void *mconfig, int flag) {
 anastasia_info.access_control = flag;
 if (flag) {
  anastasia_info.ac_default_deny = 1;
  write_log("Setting anastasia access on");
 } else {
  anastasia_info.ac_default_deny = 0;
 }
 return NULL;
}

static const char *set_ana_access_log(cmd_parms *params, void *mconfig, const char *flag) {
 if (!strcmp(flag, "2"))
 {
  anastasia_info.ac_log = 2;
 } else if (!strcmp(flag, "1"))
 {
  anastasia_info.ac_log = 1;
 } else
 {
  anastasia_info.ac_log = 0;
 }
 if (flag>0) {
  write_log("Activating anastasia access logging");
 }
 return 0;
}

static const char *set_ana_db_default(cmd_parms *params, void *mconfig, int flag) {
 anastasia_info.ac_default_deny = flag;
 return NULL;
}

static const char *set_anastasia_root(cmd_parms *params, void *mconfig, const char *ana_root) {
 char str[2048];
 strcpy(anastasia_info.anastasia_root, ana_root);
 if (ana_root[strlen(ana_root)]!='/') {
  strcat(anastasia_info.anastasia_root, "/");
 }
 sprintf(str, "Setting AnastasiaRoot to %s.\n", anastasia_info.anastasia_root);
 write_log(str);
 return NULL;
}

static const char *set_anastasia_url(cmd_parms *params, void *mconfig, const char *ana_url) {
 char str[2048];
 strcpy(anastasia_info.anastasia_url, ana_url);
 sprintf(str, "Setting AnastasiaURL to %s.\n", anastasia_info.anastasia_url);
 write_log(str);
 return NULL;
}

static const char *set_ana_db_access(cmd_parms *params, void *mconfig, const char *host, const char *database) {
 if (host!=NULL) {
  strcpy(anastasia_info.ac_host, host);
 }
 if (database!=NULL) {
  strcpy(anastasia_info.ac_database, database);
 }
 return NULL;
}

static const char *set_ana_db_access2(cmd_parms *params, void *mconfig, const char *username, const char *password) {
 if (username!=NULL) {
  strcpy(anastasia_info.ac_username, username);
 }
 if (password!=NULL) {
  strcpy(anastasia_info.ac_password, password);
 }
 return NULL;
}

#if 0
static int check_ana_access(request_rec *r) {
 //If the autorization stuff hasn't been already sent we need to send a 401 (Authorization needed)
 const char *sent_pw = NULL;
 char *str, *args, *bookname, *anaid, *anvfile, *sql, *conn_ip;
 pool *current_pool = r->pool;
 int ret = 0, ret2 = 0, sql_len = 0, no_results = 0, userid=-1, bookid=-1;
 //Firstly check that this book has security measures on it.
#if !defined(ANA_MYSQL)
 return OK;
#else
 MYSQL *db_connection, *mysql_connection;
 MYSQL_RES *mysql_result;
 MYSQL_ROW row;
 if (anastasia_info.ac_default_deny) {
  ret = AUTH_REQUIRED;
 } else {
  ret = OK;
 }
 if (anastasia_info.access_control==0) {
  return OK;
 }
 //Initalise the db variable
 db_connection = mysql_init(NULL);
 if (db_connection==NULL)
 {
  if (anastasia_info.ac_log) {
   str = apr_psprintf(current_pool, "Access Control: Couldn't initalise server connection. %s", mysql_error(db_connection));
   write_log((const char *) str);
  }
  return ret;
 } else if (anastasia_info.ac_log==2) {
  write_log("Access Control: Initalised server connection.");
 }
 //Connect to the database
 if (anastasia_info.ac_log==2)
 {
  write_log("Access Control: Trying to connect to database");
 }
 mysql_connection = mysql_real_connect(db_connection, anastasia_info.ac_host, anastasia_info.ac_username, anastasia_info.ac_password, anastasia_info.ac_database, 0, NULL, 0);
 if (mysql_connection==NULL) {
  if (anastasia_info.ac_log) {
   str = apr_psprintf(current_pool, "Access Control: Error trying to connect to database. %s", mysql_error(db_connection));
   write_log((const char *) str);
  }
  if (anastasia_info.ac_log==2)
  {
   str = apr_psprintf(current_pool, "Access Control: hostname=%s | username=%s | password=%s | database=%s", anastasia_info.ac_host, anastasia_info.ac_username, anastasia_info.ac_password, anastasia_info.ac_database); 
   write_log((const char *) str);
  }
  return ret;
 }
 args = apr_pstrdup(current_pool, r->args);
 if (args!=NULL) {
  bookname = ap_getword_nc(current_pool, &args, '+');
  anaid = ap_getword_nc(current_pool, &args, '+');
  anvfile = ap_getword_nc(current_pool, &args, '+');
 } else {
  return ret;
 }
 conn_ip = apr_pstrdup(current_pool, r->connection->remote_ip);
 //Now check for various conditions.
 //Firstly check to see if the book is protected!
 if (anastasia_info.ac_log==2)
 {
  str = apr_psprintf(current_pool, "Access Control: Checking database for bookname='%s'", bookname);
  write_log((const char *) str);
 }
 sql = apr_psprintf(current_pool, "SELECT id,defaultdeny FROM books WHERE bookname='%s'", bookname);
 sql_len = strlen(sql);
 if (mysql_real_query(db_connection, sql, sql_len)!=0) {
  log_connection(db_connection, bookname, NULL, conn_ip, 0, current_pool, ret, r->request_time,  __FILE__, __LINE__);
   if (anastasia_info.ac_log) {
  str = apr_psprintf(current_pool, "Access Control: Error searching access database for book '%s' : %s.", bookname, mysql_error(db_connection));
   write_log(str);
  }
  mysql_close(db_connection);
  return ret;
 }
 mysql_result = mysql_store_result(db_connection);
 no_results = (int) mysql_num_rows(mysql_result);
 if (no_results!=1) {
  if (anastasia_info.ac_log) {
   str = apr_psprintf(current_pool, "Access Control: Could not find entry for book \"%s\".", bookname);
   write_log(str);
  }
  /* Can't find any entry for this book */
  log_connection(db_connection, bookname, NULL, conn_ip, 0, current_pool, ret, r->request_time, __FILE__, __LINE__);
  mysql_free_result(mysql_result);
  mysql_close(db_connection);
  return ret;
 } else {
  row = mysql_fetch_row(mysql_result);
  bookid = atoi(row[0]);
  ret2 = atoi(row[1]);
  if (ret2==0) {
   ret = OK;
  } else {
   ret = AUTH_REQUIRED;
  }
 }
 /* Get out the access permissions */
 mysql_free_result(mysql_result);
 if (anastasia_info.ac_log==2)
 {
  str = apr_psprintf(current_pool, "Access Control: Searching for permission on bookid='%d' AND connection ip='%s'", bookid, conn_ip);
  write_log(str);
 }
 /* Get any permissions for this book for this ip address */
 sql = apr_psprintf(current_pool, "SELECT p.user_id,p.client_id,p.deny FROM permissions AS p LEFT JOIN ips AS ip ON p.ip_id=ip.id WHERE p.book_id=%d AND ((ip.from<=INET_ATON('%s') AND ip.to>=INET_ATON('%s')) OR (p.ip_id IS NULL)) ORDER BY p.deny ASC", bookid, conn_ip, conn_ip);
 sql_len = strlen(sql);
 if (mysql_real_query(db_connection, sql, sql_len)!=0) {
  log_connection(db_connection, bookname, NULL, conn_ip, 0, current_pool, ret, r->request_time, __FILE__, __LINE__);
  if (anastasia_info.ac_log) {
   str = apr_psprintf(current_pool, "Access Control: Error searching access database for book '%s' : %s.", bookname, mysql_error(db_connection));
   write_log(str);
  }
  mysql_close(db_connection);
  return ret;
 }
 mysql_result = mysql_store_result(db_connection);
 no_results = (int) mysql_num_rows(mysql_result);
 if (no_results==0) {
  /* No permissions set for this book and ip address*/
  if (anastasia_info.ac_log) {
   str = apr_psprintf(current_pool, "Access Control: Could not find any permissions for book \"%s\" and ip address \"%s\".", bookname, conn_ip);
   write_log(str);
  }
  log_connection(db_connection, bookname, NULL, conn_ip, 0, current_pool, ret, r->request_time, __FILE__, __LINE__);
  mysql_free_result(mysql_result);
  mysql_close(db_connection);
  return ret;
 }
 //Get the connection ip address
 while ((row = mysql_fetch_row(mysql_result))) {
  int deny, userid, client_id;
  if (row[0]==NULL)
  {
   userid = 0;
  } else 
  {
   userid = atoi(row[0]);
  }
  if (row[1]==NULL)
  {
   client_id = 0;
  } else 
  {
   client_id = atoi(row[1]);
  }
  if (row[2]!=NULL && strcmp(row[2], "1")==0) {
   deny = 1;
  } else {
   deny = 0;
  }
  //If denied and ip address matches and no user specificed, then your not allowed here.
  //Update!! If ip address matches and a user is specified, let them in anyway?
  if (deny==1 && userid==0) {
   //We should deny this?
   break;
  } else if (deny==0 && userid==0) {
   if (anastasia_info.ac_log) {
    str = apr_psprintf(current_pool, "Access Control: Granted access for book \"%s\" on ip \"%s\".", bookname, conn_ip);
    write_log(str);
   }
   log_connection(db_connection, bookname, NULL, conn_ip, client_id, current_pool, OK, r->request_time, __FILE__, __LINE__);
   mysql_free_result(mysql_result);
   mysql_close(db_connection);
   return OK;
  }
 }
 mysql_free_result(mysql_result);
 if  (anastasia_info.ac_log) {
  str = apr_psprintf(current_pool, "Access Control: Requesting username for access to book \"%s\".", bookname);
  write_log(str);
 }
 if (anastasia_info.ac_log==2)
 {
  write_log("Access Control: Asking for username and password");
 }
 //O.k. now we are here, so check out the users.
 if ((ret2 = ap_get_basic_auth_pw(r, &sent_pw))) {
  return ret2;
 }
 if (anastasia_info.ac_log==2)
 {
  str = apr_psprintf(current_pool, "Access Control: Check database for username='%s' AND password='%s'", r->connection->user, sent_pw);
  write_log(str);
 }
 //Check that this username exists and the password is correct
 //sql = apr_psprintf(current_pool, "SELECT id FROM users WHERE username='%s' AND password=PASSWORD('%s')", r->connection->user, sent_pw);
 sql = apr_psprintf(current_pool, "SELECT id FROM sf_guard_user WHERE username='%s' AND password = SHA1(CONCAT(salt,'%s'))", r->connection->user, sent_pw); 
 sql_len = strlen(sql);
 if (mysql_real_query(db_connection, sql, sql_len)!=0) {
  log_connection(db_connection, bookname, r->connection->user, conn_ip, 0, current_pool, ret, r->request_time, __FILE__, __LINE__);
  if (anastasia_info.ac_log) {
   str = apr_psprintf(current_pool, "Access Control: Error searching access database for user '%s' : %s.", r->connection->user, mysql_error(db_connection));
   write_log(str);
  }
  mysql_close(db_connection);
  return ret;
 }
 mysql_result = mysql_store_result(db_connection);
 no_results = (int) mysql_num_rows(mysql_result);
 if (no_results==0) {
  if (anastasia_info.ac_log) {
   str = apr_psprintf(current_pool, "Access Control: Could not find permission for user \"%s\" for book \"%s\".", r->connection->user, bookname);
   write_log(str);
  }
  log_connection(db_connection, bookname, r->connection->user, conn_ip, 0, current_pool, ret, r->request_time, __FILE__, __LINE__);
  mysql_free_result(mysql_result);
  mysql_close(db_connection);
  return ret;
 } else {
  //Get a username and password match.
  row = mysql_fetch_row(mysql_result);
  userid = atoi(row[0]);
  mysql_free_result(mysql_result);
 }
 if (anastasia_info.ac_log==2) {
  str = apr_psprintf(current_pool, "Access Control: Checking for permission for user=\"%s\" and bookid=\"%d\" AND ips=\"%s\".", r->connection->user, bookid, conn_ip);
  write_log(str);
 }
 sql = apr_psprintf(current_pool, "SELECT p.deny,p.client_id FROM permissions AS p LEFT JOIN ips AS ip ON p.ip_id=ip.id WHERE p.book_id=%d AND ((ip.from<=INET_ATON('%s') AND ip.to>=INET_ATON('%s')) OR (p.ip_id IS NULL)) AND p.user_id=%d ORDER BY p.desc ASC", bookid, conn_ip, conn_ip, userid);
 sql_len = strlen(sql);
 write_log(sql);
 if (mysql_real_query(db_connection, sql, sql_len)!=0) {
  if (anastasia_info.ac_log) {
   str = apr_psprintf(current_pool, "Access Control: Error searching access database for book '%s' : %s.", bookname, mysql_error(db_connection));
   write_log(str);
  }
  log_connection(db_connection, bookname, r->connection->user, conn_ip, 0, current_pool, ret, r->request_time, __FILE__, __LINE__);
  mysql_close(db_connection);
  return ret;
 }
 mysql_result = mysql_store_result(db_connection);
 no_results = (int) mysql_num_rows(mysql_result);
 if (no_results==0) {
  if (anastasia_info.ac_log) {
   str = apr_psprintf(current_pool, "Access Control: Could not find permission for user \"%s\", for book \"%s\" on ip \"%s\".", r->connection->user, bookname, conn_ip);
   write_log(str);
  }
  log_connection(db_connection, bookname, r->connection->user, conn_ip, 0, current_pool, ret, r->request_time, __FILE__, __LINE__);
  mysql_free_result(mysql_result);
  mysql_close(db_connection);
  return ret;
 }
 //Now check out if any of these matched users match the ip addresses as well.
 while ((row = mysql_fetch_row(mysql_result))) {
  int deny, client_id;
  if (row[0]!=NULL && strcmp(row[0], "1")==0) {
   deny = 1;
  } else {
   deny = 0;
  }
  if (row[1]==NULL)
  {
   client_id = 0;
  } else 
  {
   client_id = atoi(row[1]);
  }
  //If denied and ip address matches and no user specificed, then your not allowed here.
  if (deny==1) {
   break;
  } else if (deny==0) {
   if (anastasia_info.ac_log) {
    str = apr_psprintf(current_pool, "Access Control: Allowing connection for user \"%s\" for book \"%s\" on ip \"%s\".", r->connection->user, bookname, conn_ip);
    write_log(str);
   }
   log_connection(db_connection, bookname, r->connection->user, conn_ip, client_id, current_pool, OK, r->request_time, __FILE__, __LINE__);
   mysql_free_result(mysql_result);
   mysql_close(db_connection);
   return OK;
  }
 }
 if (anastasia_info.ac_log) {
  if (ret==OK) {
   str = apr_psprintf(current_pool, "Access Control: Allowing connection for user \"%s\" for book \"%s\" on ip \"%s\".", r->connection->user, bookname, conn_ip);
  } else {
   str = apr_psprintf(current_pool, "Access Control: Denying connection for user \"%s\" for book \"%s\" on ip \"%s\".", r->connection->user, bookname, conn_ip);
  }
  write_log(str);
 }
 log_connection(db_connection, bookname, r->connection->user, conn_ip, 0, current_pool, ret, r->request_time, __FILE__, __LINE__);
 mysql_free_result(mysql_result);
 mysql_close(db_connection);
 return ret;
#endif
}
#endif

#if defined(ANA_MYSQL)
void log_connection(MYSQL *db_connection, char *bookname, char *username, char *conn_ip, int client_id, pool *current_pool, int perm, time_t time, char *file, int lineno) {
 MYSQL_RES *mysql_result;
 char *sql, *str;
 int sql_len, no_results;
 //Check that we haven't already added permissions for this call
 sql = apr_psprintf(current_pool, "SELECT * FROM logs WHERE bookname='%s' AND ip_address=INET_ATON('%s') and time='%d' LIMIT 1", bookname, conn_ip, (int)time);
 sql_len = strlen(sql);
 if (!mysql_real_query(db_connection, sql, sql_len)) {
  mysql_result = mysql_store_result(db_connection);
  no_results = (int) mysql_num_rows(mysql_result);
  mysql_free_result(mysql_result);
  if (no_results==0) {
   if (anastasia_info.ac_log)
   {
    str = apr_psprintf(current_pool, "Logging access for username=%s | ip_address=%s | time=%d | bookname=%s | perm=%d | client_id=%d", username, conn_ip, (int)time, bookname, perm, client_id);
    write_log(str);
   }
   //Hasn't been stored so log this call
   if (username!=NULL) {
    sql = apr_psprintf(current_pool, "INSERT INTO logs (username, ip_address, client_id, time, bookname, perm) VALUES('%s', INET_ATON('%s'), %d, %d, '%s', %d)", username, conn_ip, client_id, (int)time, bookname, perm);
   } else {
    sql = apr_psprintf(current_pool, "INSERT INTO logs (username, ip_address, client_id, time, bookname, perm) VALUES(NULL, INET_ATON('%s'), %d, %d, '%s', %d)", conn_ip, client_id, (int)time, bookname, perm);
   }
   write_log(sql);
   sql_len = strlen(sql);
   write_log(sql);
   if (mysql_real_query(db_connection, sql, sql_len)) {
    write_log(sql);
    sql = apr_psprintf(current_pool, "Error logging access '%s' : %s.", bookname, mysql_error(db_connection));
    write_log(sql);
   }
  }
 } else {
  write_log(sql);
  sql = apr_psprintf(current_pool, "Error logging access '%s' : %s.", bookname, mysql_error(db_connection));
  write_log(sql);
 }
}
#endif

