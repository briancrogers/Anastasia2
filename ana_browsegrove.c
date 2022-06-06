#include "ana_common.h"
#include "ana_commands.h"
#include "ana_browsegrove.h"
#include "ana_utils.h"
#include "mod_anastasia.h"
#include "sgrep/ana_sgrep.h"
#include <apr_strings.h>
#include <apr_pools.h>

//moved from mod_anastart.c
#if !defined(WIN32)
 #include<unistd.h>
  #include <sys/types.h>
    #include <sys/ipc.h>
    #include <sys/shm.h>
#endif

//Global.. only the one.. the main anastasia information structure
struct anastasia_information anastasia_info;
global_book_arr_ptr books_array;
         
void anastasia_init(pool *server_pool, server_rec *s) {
 char *server_root, *tmp2, str[2048];
 pool *current_pool = 0; apr_pool_create(&current_pool, server_pool);
 int initalisation = 0, ble=1; initalisation = 1;
 anastasia_info.server_pool = server_pool;
 //Is this an apache or standalone version of anastasia?
 if (s == NULL) {
  anastasia_info.standalone = 1;
 } else {
  anastasia_info.standalone = 0;
 }
 //are we big or little endian?
 if(*(char *)&ble == 1) {
  anastasia_info.big_endian=0;
 } else {
  anastasia_info.big_endian=1;
 }
#if defined(WIN32)
  anastasia_info.platform = 1;
#else
  anastasia_info.platform = 0;
#endif
 anastasia_info.server_rec = s;
 //Lets also create the location of where this is being run from
 if (anastasia_info.standalone==0) {
  server_root = ap_server_root_relative(server_pool, "");
  tmp2 = (char*)apr_pcalloc(server_pool, strlen(server_root)+1);
  ap_make_dirstr_prefix(tmp2, server_root, ap_count_dirs(server_root)-1);
  if (tmp2!=NULL) {
   ap_no2slash(tmp2);
#if defined(_ANA_CDROM_)
    //always want to get the books out of the default directory
    strcpy(anastasia_info.anastasia_root, tmp2);
#endif
   anastasia_info.path = apr_pstrdup(server_pool, tmp2);
  } else {
   anastasia_info.path = apr_pstrdup(server_pool, "");
  }
 } else {
  getcwd(str, 2048);
  anastasia_info.path = apr_pstrdup(server_pool, str);
 }
 if (initalisation && !anastasia_info.standalone) { //3: for Apache 2, with consequent remodelling of memory handling. Many errors found and corrected
 //these changes started by Troy Griffits in 2010, completed by Peter Robinson in 2019
  write_log("Anastasia says hello: AnaServer initialised (version 3.0). \n");
  printf("Anastasia says hello: AnaServer initialised (version 2.1). \n");
  printf( "In the event of error see log file for details. \n");
  printf("And contact support@sd-editions.com for support.\n");
  printf("If the browser fails see http://www.sd-editions.com/anastasia/fixes.\n");
 }
 //Now lets create the temporary memory for AnaInfo
 anastasia_info.ana_start_time = time(NULL);
 if (anastasia_info.access_control!=0 && anastasia_info.access_control!=1) {
  anastasia_info.access_control = 0;
  strcpy(anastasia_info.ac_host, "");
  strcpy(anastasia_info.ac_username, "");
  strcpy(anastasia_info.ac_password, "");
 }
 //Destory the current temporary pool...no!!! we need it
// apr_pool_destroy(current_pool);
}

/* Look for the main configuration file and load it in,
Check these directorys in this order
./  (local directory)
/etc/Anastasia/AnaRead.cfg ($SYSTEM_ROOT\AnaRead.cfg for windows, e.g. C:\WinNT\AnaRead.cfg)
And to be backwards compatible /Anastasia/AnaRead.cfg
*/ 
void load_in_global_config_file(Tcl_Interp *the_interp, pool *this_pool, int initalisation) {
 char *tcl_res, *tmp=NULL, str[2048]="", *tmp2, tcl_cmd[2048]="", cfg_file[2048]="";
 int result=0;
 //Find any and all books!
// write_log("starting point 7-2");
 if (strcmp(anastasia_info.anastasia_root, "")==0) {
  createBooksArray(the_interp, "");
 } else {
  createBooksArray(the_interp, anastasia_info.anastasia_root);
 }
 strcpy(cfg_file, "");
 //Check /anastasia/AnaRead.cfg
 if (anastasia_info.platform==1) {
  //Find the system drive
  tcl_res = (char *) Tcl_GetVar(the_interp, "env(SystemDrive)", 0);
  if (tcl_res!=NULL) {
   sprintf(str, "%s\\Anastasia\\AnaRead.cfg", tcl_res);
  } else {
   strcpy(str, "C:\\Anastasia\\AnaRead.cfg");
  }
 } else {
  strcpy(str, "/Anastasia/AnaRead.cfg");
 }
 sprintf(tcl_cmd, "file exists {%s}", str);
 result = Tcl_Eval(the_interp, tcl_cmd);
 tcl_res = (char *) Tcl_GetStringResult(the_interp);
 if (strcmp(tcl_res, "1")==0) {
  strcpy(cfg_file, str);
 }
 //Check the local directory
 tmp = ap_make_full_path(this_pool, anastasia_info.path, "AnaRead.cfg");
 ap_no2slash(tmp);
 sprintf(tcl_cmd, "file exists {%s}", tmp);
 result = Tcl_Eval(the_interp, tcl_cmd);
 tcl_res = (char *) Tcl_GetStringResult(the_interp);
 if (strcmp(tcl_res, "1")==0) {
  strcpy(cfg_file, tmp);
 }
 //Check the etc/Windows directory directory
// write_log("starting point 7-3");
 if (anastasia_info.platform==1) {
  tcl_res = (char *) Tcl_GetVar(the_interp, "env(SystemRoot)", 0);
  if (tcl_res!=NULL) {
   tmp2 = (char *) apr_pcalloc(this_pool, strlen(tcl_res)+15);
   sprintf(tmp2, "%s\\AnaRead.cfg", tcl_res);
  } else {
   tmp2 = (char *) apr_pcalloc(this_pool, strlen("C:\\Windows\\AnaRead.cfg")+1);
   strcpy(tmp2, "C:\\Windows\\AnaRead.cfg");
  }
 } else {
  tmp2 = (char *) apr_pcalloc(this_pool, strlen("/etc/Anastasia/AnaRead.cfg")+1);
  strcpy(tmp2, "/etc/Anastasia/AnaRead.cfg");
 }
// write_log("starting point 7-4");
 sprintf(tcl_cmd, "file exists {%s}", tmp2);
 result = Tcl_Eval(the_interp, tcl_cmd);
 tcl_res = (char *) Tcl_GetStringResult(the_interp);
 if (strcmp(tcl_res, "1")==0) {
  strcpy(cfg_file, tmp2);
 }
 //Check the AnastasiaRoot directive from Apache (overrides all)
 tmp = ap_make_full_path(this_pool, anastasia_info.anastasia_root, "AnaRead.cfg");
 if (strcmp(tmp, "")!=0) {
  ap_no2slash(tmp);
  sprintf(tcl_cmd, "file exists {%s}", tmp);
  result = Tcl_Eval(the_interp, tcl_cmd);
  tcl_res = (char *) Tcl_GetStringResult(the_interp);
  if (strcmp(tcl_res, "1")==0) {
   strcpy(cfg_file, tmp);
  }
 }
 if (strcmp(cfg_file, "")!=0) {  //original cfg_file!=NULL && condition is otiose
  result=Tcl_EvalFile(the_interp, cfg_file);
  tcl_res = (char *) Tcl_GetStringResult(the_interp);
  if (result!=TCL_OK) {
   write_log(apr_psprintf(this_pool, "Error loading Anastasia Reader configuration file \"%s\". Error message is \"%s\", in line %d.  This could lead to unexpected results.\n", cfg_file, tcl_res, Tcl_GetErrorLine(the_interp)));
  }
 } else {
 // write_log("cfg_file is null!\n");
 }
}


int make_reader_file(char **argv, request_rec *r, char *http_post_args, char *output_file, pool *current_pool) {
 int result, last_el=0;
 char *el_num_str=argv[2];
 char *anv_file=argv[3];
 char *bookname=argv[1];
 char *tcl_return;
 buffer_ptr output_buffer;
 process_book_ptr this_process_book;
 process_book_ptr *current_books;
 Tcl_Obj *tobj1, *tobj2;
 style_ptr this_style;
 sgml_obj_ptr this_sgml_el, temp_sgml_el;
 //before we do anything..let us check if this is a info call
// write_log("starting point 6");
 if (strcmp(bookname, "AnaInfo")==0) {
//  write_log("starting point 6-1");
  send_http_header(r, NULL);
  return get_book_info(r, http_post_args, current_pool);
 }
 if (strcmp(bookname, "")!=0) {
  //First off open the book
//  write_log("starting point 6-2");
  result = create_process_book(&this_process_book, NULL, NULL, bookname, http_post_args, r, current_pool);
  } else {
  result = 1;
 }
 //O.k. returned 0 on sucess, 1 on error and 2 on book unavailable
 if (result==1 || this_process_book==NULL) {
#if !defined(ANASTANDALONE)
//this crashes the thread if we send this...
 // send_http_header(r, NULL);
  ap_rputs("<html><body><p><font color=red>Error processing Anastasia URL parameter: the FIRST parameter ", r);
  ap_rprintf(r, "\"%s\", should be a valid bookname.</font></p></body></html>", bookname);
#else
  printf("Error processing Anastasia URL parameter: the FIRST parameter \"%s\", should be a valid bookname.", bookname);
#endif
  return false;
 } else if (result==2) {
#if !defined(ANASTANDALONE)
  send_http_header(r, NULL);
  ap_rputs("<p><font color=red>Error processing Anastasia URL parameter: ", r);
  ap_rprintf(r, "The book \"%s\" is unavailable for reading.</font></p>", bookname);
#else
  printf("Error processing Anastasia URL parameter: The book \"%s\" is unavailable for reading.", bookname);
#endif
  return false;
 }
 if (this_process_book==NULL) return false;
 //Got a valid book so put it in the array of books
 current_books = (process_book_ptr *) ana_malloc(&(this_process_book->anastasia_memory), (sizeof(process_book_ptr) * NUMBOOKS), __FILE__, __LINE__);
 if (current_books==NULL) {
  send_http_header(r, NULL);
  close_ana_book(this_process_book, NULL, 1);
  return false;
 }
//  write_log("starting point 7");
 tobj1 = Tcl_NewObj();
 Tcl_AppendStringsToObj(tobj1, "current_books", NULL);
 tobj2 = Tcl_NewLongObj((unsigned long int) current_books);
 Tcl_ObjSetVar2((this_process_book)->book_interp, tobj1, NULL, tobj2, TCL_GLOBAL_ONLY);
 //Now try getting the book back out to check
 current_books[0] = this_process_book;
 //this is the first book.  put current books into the interpreter for this book
 this_process_book->in_search = 0;
// write_log("starting point 8");
 if (strcmp(el_num_str, "")==0 || !is_valid_el(el_num_str, "URLPARAM", &this_sgml_el, this_process_book, NULL)) {
  //Something went wrong so..
  //this causes a crash too if we do not have a correct number...
 // send_http_header(r, NULL);
  ap_rputs("<html><body><p><font color=red>Error processing Anastasia URL parameter: the SECOND parameter ", r);
  ap_rprintf(r, "\"%s\", should be the number of a valid Anastasia element identifier.</font></p></body></html>", el_num_str);
  close_all_books(current_books);
  return false;
 }
 if (strcmp(anv_file, "")==0) {
  //Something went wrong so..again, send_http_header crashes everything
//  send_http_header(r, NULL);
  ap_rputs("<html><body><p><p><font color=red>Error processing Anastasia URL parameter: the third parameter ", r);
  ap_rprintf(r, "\"%s\", should be an anv file name</font></p></body></html>", anv_file);
  close_all_books(current_books);
  return false;
 }
 if (!clone_sgml_obj(this_sgml_el, &temp_sgml_el, this_process_book, __FILE__, __LINE__)) {
  close_all_books(current_books);
  return false;
 }
 //Need to make a text object for this content output.
 if (!init_output_buffer(&output_buffer, this_process_book, this_process_book->current_pool)) {
  close_all_books(current_books);
  return false;
 }
 //Considering that we don't keep books from one call to the next why don't we just make a new one each time.
// write_log("diong element 1a");
 if (!make_new_tcl_style(anv_file, bookname, &this_style, output_buffer, this_process_book, true, true)) {
  delete_output_buffer(&output_buffer, this_process_book);
  close_all_books(current_books);
  return false;
 }
//  write_log("diong element 1ab");

 this_style->rec = r;
 //first: call before proc in this style file
 result=do_start_end_ana_file(temp_sgml_el->number, "begin", this_style, this_process_book);
//ap_rputs("after do_start_end_ana_file <br/>", r);
//return true;
 //Finished doing the start so any calls to setCookie or getCookie will have been done.
//  write_log("diong element 1");
#if !defined(ANASTANDALONE)
 send_http_header(r, this_process_book);
 ap_rputs(this_style->file_spec->ptr, r);
#else
 printf("%s", this_style->file_spec->ptr);
#endif
 ana_free(&(this_process_book->anastasia_memory), this_style->file_spec->ptr);
 this_style->file_spec->ptr = NULL;
 //we might have reset startEl in the begin function to start at a specific point...
 //  write_log("diong element 1-1");
 tcl_return = (char *) Tcl_GetVar(this_style->tcl_interp, "startEl", 0);
 if (tcl_return) {
  //if not successful: there will be a warning and all will go just as before
  if (is_valid_el(tcl_return, "URLPARAM", &this_sgml_el, this_process_book, this_style)) {
  //reset last_el to startEl
   last_el= atoi(tcl_return);
   if (!dispose_clone_sgml_obj(&temp_sgml_el, this_process_book)) {
    ap_rputs("<p><font color=red>Error disposing of clone_sgml_obj in make_reader_file.</p>\n", r);
    delete_output_buffer(&output_buffer, this_process_book);
    close_all_books(current_books);
    return false;
   }
   if (!clone_sgml_obj(this_sgml_el, &temp_sgml_el, this_process_book, __FILE__, __LINE__)) {
    ap_rputs("<p><font color=red>Error creating clone_sgml_obj in make_reader_file.</p>\n", r);
    delete_output_buffer(&output_buffer, this_process_book);
    close_all_books(current_books);
    return false;
   }
  }
  //Unset the startEl to prevent other procedures getting confused later on
 //   write_log("diong element 1-2");
  Tcl_Eval(this_style->tcl_interp, "unset startEl");
 }
 //might also have reset toc for this book
 tcl_return = (char *) Tcl_GetVar(this_style->tcl_interp, "tocLevels", 0);
 if (tcl_return) {
  (this_style)->outputTOC=atoi(tcl_return);
 }
 //Also check the outputSGML variable to see if that needs to be changed
 tcl_return = (char *) Tcl_GetVar(this_style->tcl_interp, "outputSGML", 0);
 if (tcl_return!=NULL && strcmp(tcl_return, "1")==0) {
  this_style->outputSGML = 1;
 }
//   write_log("diong element 1-3");
 if (result==0) {
  dispose_clone_sgml_obj(&temp_sgml_el, this_process_book);
  delete_output_buffer(&output_buffer, this_process_book);
  close_all_books(current_books);
  return false;
 } else if (result!=2) {
//   write_log("diong element 1-4");
  //2 returned if finish set
  //if a search was set in the begin proc: skip through found els till we are pointing 
  //either at this element or the next one after this
  if (this_process_book->in_search) {
   update_curr_search(temp_sgml_el->number, this_process_book);
  }
     if (this_style->outputTOC)  {
   if (!output_ana_toc(temp_sgml_el->number, this_style->outputTOC, 0, this_style, this_process_book)) {
    dispose_clone_sgml_obj(&temp_sgml_el, this_process_book);
    delete_output_buffer(&output_buffer, this_process_book);
    close_all_books(current_books);
    return false; 
   }
  } else if (!output_element(temp_sgml_el->number, this_style->outputSiblings, this_style->outputRight, this_style, (int *)&last_el, NULL, this_process_book)) {
   dispose_clone_sgml_obj(&temp_sgml_el, this_process_book);
   delete_output_buffer(&output_buffer, this_process_book);
   close_all_books(current_books);
   return false;
  }
 }
 //now: call after proc in this style file
   if (!do_start_end_ana_file(last_el, "end", this_style, this_process_book)) {
  dispose_clone_sgml_obj(&temp_sgml_el, this_process_book);
  delete_output_buffer(&output_buffer, this_process_book);
  close_all_books(current_books);
  return false;
 }
 //Dispose of the cloned sgml obj
 dispose_clone_sgml_obj(&temp_sgml_el, this_process_book);
 delete_output_buffer(&output_buffer, this_process_book);
 close_all_books(current_books);
 return true;
}

void send_ana_param_error(style_ptr this_style_ptr, char *functname, char *numparams, char *specparam, process_book_ptr this_book) {
 char *this_out;
 this_out=apr_psprintf(this_style_ptr->style_pool, "<p><font color=red>Error processing Anastasia style TCL extension function \"%s\" in style file \"%s\" for element \"%s\": this function requires %s parameter(s) only: %s\n</font></p>", functname, this_style_ptr->file_name, functname, numparams, specparam);
 write_output(this_style_ptr, this_book, this_out);
}

int is_valid_el(char *el_num_str, char *ana_cmd, sgml_obj_ptr *this_sgml_el, process_book_ptr this_book, style_ptr this_style) {
 int el_sought;
 char *end_ptr;
 char *error_out;
// write_log("about to look for shit");
 el_sought=strtol(el_num_str, &end_ptr, 10);
//  error_out=apr_psprintf(this_book->current_pool, "about to look for an integer ... \"%d\"", el_sought); 
//  write_log(error_out);
 if (el_sought<0) {
     char *this_out;
     this_out=apr_psprintf(this_book->current_pool, "<p><font color=red>Error processing Anastasia style TCL extension function \"%s\" in style file \"%s\": the first parameter \"%s\" is less than zero.  Check the style file\n</font></p>", ana_cmd, this_style->file_name, el_num_str);
     write_output(this_style, this_book, this_out);
     return false;
 }
 //if alleged number string contains non-digits!!
 if (end_ptr[0]) {
   //if this: we are coming out of a anareader while loading a parameter
  if (strcmp(ana_cmd,"URLPARAM")!=0) {
   char *this_out;
   this_out=apr_psprintf(this_book->current_pool, "<p><font color=red>Error processing Anastasia style TCL extension function \"%s\" in style file \"%s\" for element \"%s\": the first parameter, indicating an element identifier, contained the string \"%s\".  It should contain digits only.  Check the style file\n</font></p>", ana_cmd, this_style->file_name, el_num_str, end_ptr);
   write_output(this_style, this_book, this_out);
  }
  return false;
 }
  error_out=apr_psprintf(this_book->current_pool, "about to look for a number.. get it while its hot... \"%d\"", el_sought);
//  write_log(error_out);
 if (!get_next_sgml_obj(this_sgml_el, el_sought, this_book)) {
  return false;
 }
 if (!(*this_sgml_el))  {
  if (strcmp(ana_cmd,"URLPARAM")!=0) {
   char *this_out=apr_psprintf(this_book->current_pool, "<p><font color=red>Error processing Anastasia style TCL extension function \"%s\" in style file \"%s\" for element \"%s\": unable to read information for element identifier %s.  Check the style file\n</font></p>", ana_cmd, this_style->file_name, el_num_str, el_num_str);
   write_output(this_style, this_book, this_out);
  }
  return false;
 }
 return true;
}

//for a given set of parameters: checks if one is numeric, one is string, and returns them if so
int fill_in_specs(int argc, char **argv, int *got_str, int *got_num, char **spec_str, int *spec_num, char *ana_cmd, style_ptr this_style, process_book_ptr this_book) {
 char *end_ptr;
 long int ret_val;
 if (argc==2) return true;
 ret_val=strtol(argv[2], &end_ptr, 10);
 if (!end_ptr[0]) {
  *spec_num=ret_val;
  *got_num=1;
 }
 else  {
  *spec_str=argv[2];
  *got_str=1;
 }
 if (argc==3) return true;
 ret_val=strtol(argv[3], &end_ptr, 10);
 if (!end_ptr[0]) {
  if (*got_num==1)  {
   char *this_out=apr_psprintf(this_style->style_pool, "<p><font color=red>Error processing Anastasia style TCL extension function \"%s\" in style file \"%s\" for element \"%s\": the second and third parameters are both numbers, \"%s\" and \"%s\".  One of these should have been a string.\n</font></p>", argv[0], this_style->file_name, ana_cmd, argv[2], argv[3]); 
   write_output(this_style, this_book, this_out);
   return false;
  }
  *spec_num=ret_val;
  *got_num=1;
 }
 else {
  if (*got_str==1)  {
   char *this_out=apr_psprintf(this_style->style_pool, "<p><font color=red>Error processing Anastasia style TCL extension function \"%s\" in style file \"%s\" for element \"%s\": the second and third parameters are both strings, \"%s\" and \"%s\".  One of these should have been a number.\n</font></p>", argv[0], this_style->file_name, ana_cmd, argv[2], argv[3]); 
   write_output(this_style, this_book, this_out);
   return false;
  }
  *spec_str=argv[3];
  *got_str=1;
 }
 return true;
}

//looks only at immediate parent to see if it is right gi
int find_parent(int start_el, int n_relative, char *gi, int *el_found, process_book_ptr this_book) {
 sgml_obj_ptr this_sgml_el;
 int this_el;
// write_log("in find_parent");
// write_log(apr_psprintf(anastasia_info.server_pool, "in find_parent, element %d", start_el));
 if (!get_next_sgml_obj(&this_sgml_el, start_el, this_book)) return false;  //we got this one already
 if (!this_sgml_el->ancestor) return true;
 this_el=this_sgml_el->ancestor;
 if (!get_next_sgml_obj(&this_sgml_el, this_el, this_book)) return false;  //we got this one already
 if (!strncmp(gi, this_sgml_el->gi->ptr, this_sgml_el->gi->len)) {
  *el_found=this_el;
  return 2;
 }
 return true;  //not found
}

int find_ancestor(int start_el, int n_relative, char *gi, int *el_found, process_book_ptr this_book) {
 sgml_obj_ptr this_sgml_el;
 int this_el;
 int n_fnd=0;
//  write_log("in find_ancestor");
 if (!get_next_sgml_obj(&this_sgml_el, start_el, this_book)) return false;  //we got this one already
 if (!this_sgml_el->ancestor) return true;
 this_el=this_sgml_el->ancestor;
 do {
  if (!get_next_sgml_obj(&this_sgml_el, this_el, this_book)) return false;  
  if ((!gi)  || (!strncmp(gi, this_sgml_el->gi->ptr, this_sgml_el->gi->len))) {
   n_fnd++;
   if (n_fnd==n_relative) {
    *el_found=this_el;
    return 2;
   }
  }
  this_el=this_sgml_el->ancestor;
 } while (this_el);
 return true;  //not found
}


int find_lsibling(int start_el, int n_relative, char *gi, int *el_found, process_book_ptr this_book) {
 sgml_obj_ptr this_sgml_el;
 int this_el;
 int n_fnd=0;
// write_log("in find_lsibling");
 if (!get_next_sgml_obj(&this_sgml_el, start_el, this_book)) return false;  //we got this one already
 if (!this_sgml_el->lsib) return true;
 this_el=this_sgml_el->lsib;
 do {
  if (!get_next_sgml_obj(&this_sgml_el, this_el, this_book)) return false;  
  if ((!gi)  || (!strncmp(gi, this_sgml_el->gi->ptr, this_sgml_el->gi->len))) {
   n_fnd++;
   if (n_fnd==n_relative) {
    *el_found=this_el;
    return 2;
   }
  }
  this_el=this_sgml_el->lsib;
 } while (this_el);
 return true;  //not found
}

int find_rsibling(int start_el, int n_relative, char *gi, int *el_found, process_book_ptr this_book) {
 sgml_obj_ptr this_sgml_el;
 int this_el;
 int n_fnd=0;
 if (!get_next_sgml_obj(&this_sgml_el, start_el, this_book)) return false;  //we got this one already
 if (!this_sgml_el->rsib) return true;
 this_el=this_sgml_el->rsib;
 do {
  if (!get_next_sgml_obj(&this_sgml_el, this_el, this_book)) return false;  
  if ((!gi)  || (!strncmp(gi, this_sgml_el->gi->ptr, this_sgml_el->gi->len))) {
   n_fnd++;
   if (n_fnd==n_relative) {
    *el_found=this_el;
    return 2;
   }
  }
  this_el=this_sgml_el->rsib;
 } while (this_el);
 return true;  //not found
}

//gets previous element...
//in extremis: could cause you to go looking through the whole document...
int find_prev_el(int start_el, int n_relative, char *gi, int *el_found, process_book_ptr this_book) {
 sgml_obj_ptr this_sgml_el;
 int n_fnd=0;
 int this_el, c;
 if (start_el==0) return true;
 if (!get_next_sgml_obj(&this_sgml_el, start_el, this_book)) return false;  //we got this one already
 this_el=this_sgml_el->lsib;
 //so now just go back in the order
 for (c=start_el - 1; c>0; c--) {
  if (!get_next_sgml_obj(&this_sgml_el, c, this_book)) return false;  //we got this one already
  if ((!gi)  || (!strncmp(gi, this_sgml_el->gi->ptr, this_sgml_el->gi->len))) {
   n_fnd++;
   if (n_fnd==n_relative) {
    *el_found=c;
    return 2;
   }
  }
 }
 return true;  //not found
}

//all we have to is zoom through the element numbers, starting at this one...
int find_next_el(int start_el, int n_relative, char *gi, int *el_found, process_book_ptr this_book) {
 sgml_obj_ptr this_sgml_el;
 int last_el, c;
 int n_fnd=0;
 last_el = this_book->no_elements;
 for (c=start_el + 1; c<=last_el; c++) {
  if (!get_next_sgml_obj(&this_sgml_el, c, this_book)) return false;  //we got this one already
  if ((!gi)  || (!strncmp(gi, this_sgml_el->gi->ptr, this_sgml_el->gi->len))) {
   n_fnd++;
   if (n_fnd==n_relative) {
    *el_found=c;
    return 2;
   }
  }
 }
 return true; //not found
}

//find nth child of nominated type, where n is the number given in n_relative.  
//where no type is nominated, gi is NULL: just return nth child regardless of type.
//Look at one level at a time, checking to find first element at this level with a child
//once we reach last sibling: check if there are any more at this level by going up to the
//parent (if this is NOT the ultimate ancestor of the child), across to the right sib, and down
//repeating the process until there are no more siblings at all at this level
//if we reach the last sibling at this level and NO child of any sibling: no more levels, and search fails
//parent: is parent of first element at this level.  Ancestor: is ultimate ancestor for whom we are seeking a child
//first time we call this, parent and ancestor will be the same
int find_descendant(int parent, int n_relative, char *gi, int *el_found, process_book_ptr this_book) {
 sgml_obj_ptr this_sgml_el;
 int this_el;
 int n_fnd=0;
 if (!get_next_sgml_obj(&this_sgml_el, parent, this_book)) return false;  //we got this one already
 if (!this_sgml_el->child) return true;
 this_el=this_sgml_el->child;
 do {
  if (!get_next_sgml_obj(&this_sgml_el, this_el, this_book)) return false;  
  if ((!gi)  || (!strncmp(gi, this_sgml_el->gi->ptr, this_sgml_el->gi->len))) {
   n_fnd++;
   if (n_fnd==n_relative) {
    *el_found=this_el;
    return 2;
   }
  }
  this_el=this_sgml_el->rsib;
 } while (this_el);
 return true;  //not found
}

//return false: fatal error of some kind; 1: not found, but no error; 2: found
int find_child(int parent, int n_relative, char *gi, int *el_found, process_book_ptr this_book) {
 sgml_obj_ptr this_sgml_el;
 int this_el;
 int n_fnd=0;
// write_log("inside find_child");
 if (!get_next_sgml_obj(&this_sgml_el, parent, this_book)) return false;  //we got this one already
 if (!this_sgml_el->child) return true;
 this_el=this_sgml_el->child;
 do {
  if (!get_next_sgml_obj(&this_sgml_el, this_el, this_book)) return false;  
  if ((!gi)  || (!strncmp(gi, this_sgml_el->gi->ptr, this_sgml_el->gi->len))) {
   n_fnd++;
   if (n_fnd==n_relative) {
    *el_found=this_el;
    return 2;
   }
  }
  this_el=this_sgml_el->rsib;
 } while (this_el);
 return true;  //not found
}

int get_book_info(request_rec *r, char *http_post_args, pool *current_pool) {
    char *tcl_res, *pword;
    char *err_mess="<HTML><HEAD><TITLE>Anastasia error message</TITLE><BODY><H1>Anastasia \"AnaInfo\" error message</H1><p>Incorrectly formatted \"AnaInfo\" call, or incorrect password given.  The call should be of the form \"AnaInfo+0+anyfile.anv+password=mypassword\".  See the reference documentation for more details.</p></body></html>";
    char strt_time[64];
 Tcl_Interp *tcl_interp;
 tcl_interp = Tcl_CreateInterp();
 Tcl_FindExecutable(NULL);
// write_log("starting point 7a");
 load_in_global_config_file(tcl_interp, current_pool, 0);
    tcl_res = (char *) Tcl_GetVar(tcl_interp, "password", 0);
 if (http_post_args==NULL) {
  ap_rputs(err_mess, r);
  return true;
 }
//    write_log("starting point 8-1");
 
    pword=strstr(http_post_args, "password=");
    if ((!pword) || (!tcl_res) || strcmp(pword+9, tcl_res)) {
  		ap_rputs(err_mess, r);
        return true;
    }
    //righto folks.  We got a valid AnaInfo call.  Lets put it all together and send the info back
 //   write_log("starting point 8-2");
 
    sprintf(strt_time,"%s", ctime(&anastasia_info.ana_start_time));
    ap_rprintf(r, "<HTML><HEAD><TITLE>Anastasia Information</TITLE><BODY><H1>Anastasia information</H1>");
    ap_rprintf(r, "<table align=\"center\" width=\"90%%\" border=1><tr><th>Book</th><th>Hits</th><th>Max. elements</th><th>Max. memory</th><th>Max. index objs</th><th>Max. memory</th><th>Number of errors</th></tr>");
    ap_rputs("</table>",r);
 return true;
}

//second version of this: revised to make it self contained; just send a book pointer and style file and this does it all
int make_new_tcl_style(char *anv_file, char *book_name, style_ptr *this_style, buffer_obj *file_spec, process_book_ptr this_book, int first, int header_needed) {
 Tcl_Interp *tcl_interp;
 int result =0, style_pos=0;
 unsigned int a;
 pool *style_pool;
 char *tcl_res, *tmp_str, *name, *val, str[200], *anv_filename, *tmp_name;
 unsigned char *utf_str;
 table *http_inf;
 process_book_ptr *current_books;
 //O.k. here we can check the style_ptr's array to see if the requested style already exist
 //if so return it if not create a new one.
 (*this_style) = this_book->styles[style_pos];
 while ((*this_style)!=NULL && strcmp(anv_file, (*this_style)->file_name)!=0 && style_pos!=NUMSTYLES) {
  (*this_style) = this_book->styles[++style_pos];
 }
 if ((*this_style)!=NULL) {
  //Found one already
  (*this_style)->file_spec = file_spec;
    return true;
 }
 //right.  The file does not exist.  We better do make it
 //get the current books first
 tcl_res = (char *) Tcl_GetVar(this_book->book_interp, "current_books", TCL_GLOBAL_ONLY);
 if (tcl_res!=NULL) {
  //current_books = (process_book_ptr *) atol(tcl_res);
  current_books = (process_book_ptr *) strtol(tcl_res, NULL, 0);
 } else {
  current_books = NULL;
  return false;  //should never happen!
 }
 //Now loading the anv file.
 anv_filename = ap_make_full_path(this_book->current_pool, this_book->dir,anv_file);
 ap_no2slash(anv_filename);
 //Check to see there is an actual file there
 sprintf(str, "file exists {%s}", anv_filename);
 result = Tcl_Eval(this_book->book_interp, str);
 if (result==TCL_OK) {
  tcl_res = (char *) Tcl_GetStringResult(this_book->book_interp);
  if ((int) strcmp(tcl_res, "0")==0) {
   //First check the global anagrove directory
   sprintf(str, "file exists [file join $gAnaGrove %s]", anv_file);
   result = Tcl_Eval(this_book->book_interp, str);
   if (result==TCL_OK) {
    tcl_res = (char *) Tcl_GetStringResult(this_book->book_interp);
    if (strcmp(tcl_res, "1")==0) {
     //Found the anv file in the global anagrove directory
     //Need to somehow get the global anagrove directory out
     tcl_res = (char *) Tcl_GetVar(this_book->book_interp, "gAnaGrove", 0);
     if (strcmp(tcl_res, "")!=0) {
      anv_filename = ap_make_full_path(this_book->current_pool, tcl_res, anv_file);
      ap_no2slash(anv_filename);
     }
    } else {
     strcpy(anv_filename, "");
    }
   }
  }
 } else {
  strcpy(anv_filename, "");
 }
 //Check to make sure there was an anv file
 if (strcmp(anv_filename, "")==0) {
  //Something went wrong so..
  //we will need process books
#if !defined(ANASTANDALONE)
  if (header_needed) send_http_header(this_book->request_rec, NULL);
  ap_rputs("<p><font color=red>Error processing Anastasia URL parameter: the third parameter ", this_book->request_rec);
  ap_rprintf(this_book->request_rec, "\"%s\", should be an anv file name.</font></p>", anv_file);
#else
  printf("<p><font color=red>Error processing Anastasia URL parameter: the third parameter ");
  printf("\"%s\", should be an anv file name.</font></p>", anv_file);
#endif
  return false;
 }
#if defined(WIN32)
 for (a=0; a< strlen(anv_filename); a++) {
  if (anv_filename[a]=='/') {
   anv_filename[a] = '\\';
  }
 }
#endif
 apr_pool_create(&style_pool, this_book->current_pool);
 tcl_interp = Tcl_CreateInterp();
 Tcl_Preserve(tcl_interp);
 Tcl_FindExecutable(NULL);
 //Allocate some memory for this style object
 (*this_style) = (style_ptr) ana_malloc(&(this_book->anastasia_memory), sizeof(struct style_obj), __FILE__, __LINE__);
 if ((*this_style)==NULL) {
  return false;
 }
 //if (!first) {
  this_book->styles[style_pos] = (*this_style);
 //}
 (*this_style)->style_pool = style_pool;
 (*this_style)->tcl_interp = tcl_interp;
 (*this_style)->file_name = apr_pstrdup(style_pool, anv_file);
 (*this_style)->book_name = apr_pstrdup(style_pool, this_book->bookname);
 (*this_style)->file_spec = file_spec;
 (*this_style)->rec = NULL;
 strcpy((*this_style)->element_name, "");
 Tcl_SetVar(tcl_interp, "bookname", this_book->bookname, 0);
 sprintf(str, "%p", (void *) this_book);
 Tcl_SetVar((*this_style)->tcl_interp, "this_book_ptr", str, 0);
 sprintf(str, "%p", (void *) (*this_style));
 Tcl_SetVar((*this_style)->tcl_interp, "this_style_ptr", str, 0);
 sprintf(str, "%lu", (size_t) this_book->current_pool);  //in 64 bits this is a long!
 //write_log(apr_psprintf("make new tcl style: pointer is %lu", (size_t) this_book->current_pool));
//  write_log(apr_psprintf(this_book->current_pool, "make new tcl style: pointer is %lu", (size_t) this_book->current_pool));
//  write_log(apr_psprintf(this_book->current_pool, "make new tcl style sgring: string is is %s", str));
//  write_log(str);
 Tcl_SetVar((*this_style)->tcl_interp, "current_pool", str, 0);
 sprintf(str, "%d", (int) current_books);
 Tcl_SetVar((*this_style)->tcl_interp, "current_books", str, 0);
 init_ana_interp(this_style, this_book);
 //Load in global cfg file
//  write_log("findling load config 9-1 make_new_tcl_style");
 load_in_global_config_file(tcl_interp, style_pool, 0);
// write_log("findling load config 9-2 make_new_tcl_style");
 //Setup the books array for this book just in case
 tcl_res = apr_psprintf(style_pool, "books(%s)", this_book->bookname);
 Tcl_SetVar(tcl_interp, tcl_res, this_book->dir, 0);
 //Now load in the local cfg file is it exists.
 tmp_str = ap_make_full_path(style_pool, this_book->dir, "AnaRead.cfg");
 ap_no2slash(tmp_str);
 result = Tcl_Eval(tcl_interp, apr_psprintf(style_pool, "file exists {%s}", tmp_str));
 tcl_res = (char *) Tcl_GetStringResult(tcl_interp);
//  write_log("findling load config 9-3");
 if (strcmp(tcl_res, "1")==0) {
  result = Tcl_EvalFile(tcl_interp, tmp_str);
  if (result!=TCL_OK) {
   write_output((*this_style), this_book, apr_psprintf(style_pool, "<p><font color=red>Error compiling local Anastasia AnaRead.cfg file for \"%s\": %s in line %d. </font></p>",  anv_filename, Tcl_GetStringResult(tcl_interp), Tcl_GetErrorLine(tcl_interp)));
  }
 }
//Now add all the http_inf variables
//  write_log("findling load config 9-4");
 http_inf = apr_table_make(style_pool, 5);
 if (anastasia_info.standalone==0 && this_book->request_rec!=NULL && this_book->request_rec->headers_in!=NULL) {
  apr_table_do(http_inf_callback, tcl_interp, this_book->request_rec->headers_in, NULL);
 }
//And finally do the http_post stuff.
 if (this_book->http_post_args!=NULL) {
//  write_log("findling load httppost 9-2 make_new_tcl_style");
  tmp_str = apr_pstrdup(style_pool, this_book->http_post_args);
//   write_log(this_book->http_post_args);
  //Now loop through
  while(*tmp_str && (val = ap_getword(style_pool, (const char **) &tmp_str, '&'))) {
   name = ap_getword(style_pool, (const char **) &val, '=');
   //Need to convert the val to unsigned char so that we retain the utf8 characters
   utf_str = (unsigned char *) apr_palloc(style_pool, (sizeof(unsigned char) * strlen(val)));
   for (a=0; a<strlen(val); a++) { utf_str[a] = (unsigned char) val[a]; }
        tmp_name = apr_psprintf(style_pool, "httppost(%s)", name);
   Tcl_SetVar(tcl_interp, tmp_name, val, 0);
  }
 }
 /* Lets add the unparsed uri information */
 // write_log("findling load config 9-5");
 if (this_book->request_rec!=NULL) {
  //Scheme
  name = apr_pstrdup(style_pool, "parsed_uri(scheme)");
  tmp_str = apr_pstrdup(style_pool, this_book->request_rec->parsed_uri.scheme);
  Tcl_SetVar(tcl_interp, name, tmp_str, 0);
  //Hostinfo
  name = apr_pstrdup(style_pool, "parsed_uri(hostinfo)");
  tmp_str = apr_pstrdup(style_pool, this_book->request_rec->parsed_uri.hostinfo);
  Tcl_SetVar(tcl_interp, name, tmp_str, 0);
  //User
  name = apr_pstrdup(style_pool, "parsed_uri(user)");
  tmp_str = apr_pstrdup(style_pool, this_book->request_rec->parsed_uri.user);
  Tcl_SetVar(tcl_interp, name, tmp_str, 0);
  //Password
  name = apr_pstrdup(style_pool, "parsed_uri(password)");
  tmp_str = apr_pstrdup(style_pool, this_book->request_rec->parsed_uri.password);
  Tcl_SetVar(tcl_interp, name, tmp_str, 0);
  //hostname
  name = apr_pstrdup(style_pool, "parsed_uri(hostname)");
  tmp_str = apr_pstrdup(style_pool, this_book->request_rec->parsed_uri.hostname);
  Tcl_SetVar(tcl_interp, name, tmp_str, 0);
  //port_str
  name = apr_pstrdup(style_pool, "parsed_uri(port_str)");
  tmp_str = apr_pstrdup(style_pool, this_book->request_rec->parsed_uri.port_str);
  Tcl_SetVar(tcl_interp, name, tmp_str, 0);
  //path
  name = apr_pstrdup(style_pool, "parsed_uri(path)");
  tmp_str = apr_pstrdup(style_pool, this_book->request_rec->parsed_uri.path);
  Tcl_SetVar(tcl_interp, name, tmp_str, 0);
  //query
  name = apr_pstrdup(style_pool, "parsed_uri(query)");
  tmp_str = apr_pstrdup(style_pool, this_book->request_rec->parsed_uri.query);
  Tcl_SetVar(tcl_interp, name, tmp_str, 0);
  //fragment
  name = apr_pstrdup(style_pool, "parsed_uri(fragment)");
  tmp_str = apr_pstrdup(style_pool, this_book->request_rec->parsed_uri.fragment);
  Tcl_SetVar(tcl_interp, name, tmp_str, 0);
  //This from the request_rec
  name = apr_pstrdup(style_pool, "request_rec(hostname)");
  tmp_str = apr_pstrdup(style_pool, this_book->request_rec->hostname);
  Tcl_SetVar(tcl_interp, name, tmp_str, 0);
  //Request method
  name = apr_pstrdup(style_pool, "request_rec(method)");
  tmp_str = apr_pstrdup(style_pool, this_book->request_rec->method);
  Tcl_SetVar(tcl_interp, name, tmp_str, 0);
  //Request content_encoding
  name = apr_pstrdup(style_pool, "request_rec(content_encoding)");
  tmp_str = apr_pstrdup(style_pool, this_book->request_rec->content_encoding);
  Tcl_SetVar(tcl_interp, name, tmp_str, 0);
  //Request content_language
  name = apr_pstrdup(style_pool, "request_rec(content_language)");
  apr_array_header_t *languages = this_book->request_rec->content_languages;
  if (apr_is_empty_array(languages)) {
    Tcl_SetVar(tcl_interp, name, apr_pstrdup(style_pool, ""), 0);
  }
  else {
    Tcl_SetVar(tcl_interp, name, apr_pstrdup(style_pool, APR_ARRAY_IDX(languages,0,char *)), 0);
  }
  //Request content_uri
 //  write_log("findling load config 9-5");
  name = apr_pstrdup(style_pool, "request_rec(unparsed_uri)");
  tmp_str = apr_pstrdup(style_pool, this_book->request_rec->unparsed_uri);
  Tcl_SetVar(tcl_interp, name, tmp_str, 0);
  //Request uri
  name = apr_pstrdup(style_pool, "request_rec(uri)");
  tmp_str = apr_pstrdup(style_pool, this_book->request_rec->uri);
  Tcl_SetVar(tcl_interp, name, tmp_str, 0);
  //Request filename
  name = apr_pstrdup(style_pool, "request_rec(filename)");
  tmp_str = apr_pstrdup(style_pool, this_book->request_rec->filename);
  Tcl_SetVar(tcl_interp, name, tmp_str, 0);
  //Request path_info
  name = apr_pstrdup(style_pool, "request_rec(path_info)");
  tmp_str = apr_pstrdup(style_pool, this_book->request_rec->path_info);
  Tcl_SetVar(tcl_interp, name, tmp_str, 0);
  //Request args
  name = apr_pstrdup(style_pool, "request_rec(args)");
  tmp_str = apr_pstrdup(style_pool, this_book->request_rec->args);
  Tcl_SetVar(tcl_interp, name, tmp_str, 0);
 }
 if (anastasia_info.standalone==1) {
  (*this_style)->timeout = 5000;
 } else {
  (*this_style)->timeout = 50;
 }
//Last thing to do is read in the anv file.
 //Load in the anv file
 // write_log("findling load config 9-6"); 																																																																																																																																																																																								
 // write_log(anv_filename);
 result = Tcl_EvalFile(tcl_interp, anv_filename);
 //write_log("findling load config 9-6-1a");
 if (result!=TCL_OK) {
  tcl_res = (char *) Tcl_GetStringResult(tcl_interp);
  write_output((*this_style), this_book, apr_psprintf(style_pool, "<p><font color=red>Error compiling Anastasia anv file \"%s\": %s in line %d. </font></p>",  anv_filename, Tcl_GetStringResult(tcl_interp), Tcl_GetErrorLine(tcl_interp)));
 }
 // write_log("findling load config 9-6-1b");
 //Now set up the default settings
 //Now create the url from the request structure
 if (this_book->request_rec==NULL && (strcmp(anastasia_info.anastasia_url, "")==0)) {//anastasia_info.anastasia_url==NULL || is otiose
  tcl_res = apr_psprintf(style_pool, "http://127.0.0.1/AnaServer?");
 } else if (strcmp(anastasia_info.anastasia_url, "")!=0) {//anastasia_info.anastasia_url!=NULL &&  oitios
  tcl_res = apr_psprintf(style_pool, "%s", anastasia_info.anastasia_url);
 } else if (this_book->request_rec->parsed_uri.port_str!=NULL) {
  tcl_res = apr_psprintf(style_pool, "http://%s:%s%s?", this_book->request_rec->hostname, this_book->request_rec->parsed_uri.port_str, this_book->request_rec->parsed_uri.path);
 } else {
  tcl_res = apr_psprintf(style_pool, "http://%s%s?", this_book->request_rec->hostname, this_book->request_rec->parsed_uri.path);
 }
// write_log("findling load config 9-6-1");
 Tcl_SetVar(tcl_interp, "url", tcl_res, 0);
 tcl_res = (char *) Tcl_GetVar(tcl_interp, "startSGML", 0);
 if (tcl_res==NULL) {
  (*this_style)->startSGML = apr_psprintf(style_pool, "&lt;");
 } else {
  (*this_style)->startSGML = apr_psprintf(style_pool, "%s", tcl_res);
 }
 tcl_res = (char *) Tcl_GetVar(tcl_interp, "endSGML", 0);
 if (tcl_res==NULL) {
  (*this_style)->endSGML = apr_psprintf(style_pool, "&gt;");
 } else {
  (*this_style)->endSGML = apr_psprintf(style_pool, "%s", tcl_res);
 }
 tcl_res = (char *) Tcl_GetVar(tcl_interp, "closeSGML", 0);
 if (tcl_res==NULL) {
  (*this_style)->closeSGML = apr_psprintf(style_pool, "/");
 } else {
  (*this_style)->closeSGML = apr_psprintf(style_pool, "%s", tcl_res);
 }
 tcl_res = (char *) Tcl_GetVar(tcl_interp, "outputSiblings", 0);
 if (tcl_res==NULL || tcl_res[0]!='1') {
  (*this_style)->outputSiblings = false;
 } else {
  (*this_style)->outputSiblings = true;
 }
//  write_log("findling load config 9-6-2");
 tcl_res = (char *) Tcl_GetVar(tcl_interp, "outputRight", 0);
 if (tcl_res==NULL || tcl_res[0]!='1') {
  (*this_style)->outputRight = false;
 } else {
  (*this_style)->outputRight = true;
 }
 tcl_res = (char *) Tcl_GetVar(tcl_interp, "outputSGML", 0);
 if (tcl_res==NULL || tcl_res[0]!='1') {
  (*this_style)->outputSGML = false;
 } else {
  (*this_style)->outputSGML = true;
 }
 tcl_res = (char *) Tcl_GetVar(tcl_interp, "tocLevels", 0);
 if (tcl_res==NULL) {
  (*this_style)->outputTOC = 0;
 } else {
  (*this_style)->outputTOC = atoi(tcl_res);
 }
 tcl_res = (char *) Tcl_GetVar(tcl_interp, "outputMax", 0);
 if (tcl_res==NULL) {
  (*this_style)->outputMax = 64000;
 } else {
  (*this_style)->outputMax = atoi(tcl_res);
 }
//  write_log("findling load config 9-6-3");
 tcl_res = (char *) Tcl_GetVar(tcl_interp, "timeout", 0);
 if (tcl_res==NULL) {
#if defined(ANASTANDALONE)
  (*this_style)->timeout = 5000;
#else
  (*this_style)->timeout = 50;
#endif
 } else {
  (*this_style)->timeout = atoi(tcl_res);
 }
 tcl_res = (char *) Tcl_GetVar(tcl_interp, "globalHttppost", 0);
// write_log("findling load config 9-7");
 if (tcl_res==NULL) {
  (*this_style)->globalHttppost = 0;
 } else {
  (*this_style)->globalHttppost = 1;
 }
 return true;
}

//set default values and map the crucial extension functions for this style interpreter to their C implementations
int init_ana_interp(style_ptr *this_style, process_book_ptr this_book) {
 char *result, str[50];
 //add sgml extensions to this interpreter
 Tcl_CreateCommand((*this_style)->tcl_interp, "puts", (Tcl_CmdProc*) ana_puts, NULL, NULL);
 Tcl_CreateCommand((*this_style)->tcl_interp, "attr", (Tcl_CmdProc*) get_ana_attr, NULL, NULL);
 Tcl_CreateCommand((*this_style)->tcl_interp, "giName", (Tcl_CmdProc*) get_ana_name, NULL, NULL);
 Tcl_CreateCommand((*this_style)->tcl_interp, "giPath", (Tcl_CmdProc*) get_ana_path, NULL, NULL);
 Tcl_CreateCommand((*this_style)->tcl_interp, "attrList", (Tcl_CmdProc*) get_ana_attr_list, NULL, NULL);
 Tcl_CreateCommand((*this_style)->tcl_interp, "attrNames", (Tcl_CmdProc*) get_ana_attr_names, NULL, NULL);
 Tcl_CreateCommand((*this_style)->tcl_interp, "contentType", (Tcl_CmdProc*) get_ana_content_type, NULL, NULL);
 Tcl_CreateCommand((*this_style)->tcl_interp, "content", (Tcl_CmdProc*) get_ana_content, NULL, NULL);
 Tcl_CreateCommand((*this_style)->tcl_interp, "child", (Tcl_CmdProc*) get_ana_child, NULL, NULL);
 Tcl_CreateCommand((*this_style)->tcl_interp, "ancestor", (Tcl_CmdProc*) get_ana_ancestor, NULL, NULL);
 Tcl_CreateCommand((*this_style)->tcl_interp, "leftSibling", (Tcl_CmdProc*) get_ana_lsibling, NULL, NULL);
 Tcl_CreateCommand((*this_style)->tcl_interp, "rightSibling", (Tcl_CmdProc*) get_ana_rsibling, NULL, NULL);
 Tcl_CreateCommand((*this_style)->tcl_interp, "descendant", (Tcl_CmdProc*) get_ana_descendant, NULL, NULL);
 Tcl_CreateCommand((*this_style)->tcl_interp, "sDataName", (Tcl_CmdProc*) get_ana_sDataName, NULL, NULL);
 Tcl_CreateCommand((*this_style)->tcl_interp, "sDataValue", (Tcl_CmdProc*) get_ana_sDataValue, NULL, NULL);
 Tcl_CreateCommand((*this_style)->tcl_interp, "contentLength", (Tcl_CmdProc*) get_ana_content_length, NULL, NULL);
 Tcl_CreateCommand((*this_style)->tcl_interp, "fileLength", (Tcl_CmdProc*) get_ana_file_length, NULL, NULL);
 Tcl_CreateCommand((*this_style)->tcl_interp, "previous", (Tcl_CmdProc*) get_ana_previous_el, NULL, NULL);
 Tcl_CreateCommand((*this_style)->tcl_interp, "next", (Tcl_CmdProc*) get_ana_next_el, NULL, NULL);
 Tcl_CreateCommand((*this_style)->tcl_interp, "countSG", (Tcl_CmdProc*) get_count_sg, NULL, NULL); 
 Tcl_CreateCommand((*this_style)->tcl_interp, "findSGElement", (Tcl_CmdProc*) get_find_sg_el, NULL, NULL); 
 Tcl_CreateCommand((*this_style)->tcl_interp, "findSGText", (Tcl_CmdProc*) get_find_sg_text, NULL, NULL); 
 Tcl_CreateCommand((*this_style)->tcl_interp, "startSGElement", (Tcl_CmdProc*) get_start_sg, NULL, NULL); 
 Tcl_CreateCommand((*this_style)->tcl_interp, "endSGElement", (Tcl_CmdProc*) get_end_sg, NULL, NULL);  
 Tcl_CreateCommand((*this_style)->tcl_interp, "prevChars", (Tcl_CmdProc*) get_prev_chars, NULL, NULL); 
 Tcl_CreateCommand((*this_style)->tcl_interp, "theseChars", (Tcl_CmdProc*) get_these_chars, NULL, NULL); 
 Tcl_CreateCommand((*this_style)->tcl_interp, "nextChars", (Tcl_CmdProc*) get_next_chars, NULL, NULL);
 Tcl_CreateCommand((*this_style)->tcl_interp, "makeKWIC", (Tcl_CmdProc*) make_kwic, NULL, NULL);
 Tcl_CreateCommand((*this_style)->tcl_interp, "setCookie", (Tcl_CmdProc*) get_ana_setCookie, NULL, NULL);
 Tcl_CreateCommand((*this_style)->tcl_interp, "getCookie", (Tcl_CmdProc*) get_ana_getCookie, NULL, NULL);
 Tcl_CreateCommand((*this_style)->tcl_interp, "setHeader", (Tcl_CmdProc*) ana_set_header, NULL, NULL);
 Tcl_CreateCommand((*this_style)->tcl_interp, "getCookieNames", (Tcl_CmdProc*) get_ana_getCookieNames, NULL, NULL);
 Tcl_CreateCommand((*this_style)->tcl_interp, "setContentType", (Tcl_CmdProc*) set_content_type, NULL, NULL);
 Tcl_CreateCommand((*this_style)->tcl_interp, "writeErrorMessage", (Tcl_CmdProc*) ana_writeErrorMessage, NULL, NULL);
 Tcl_CreateCommand((*this_style)->tcl_interp, "globalHttppost", (Tcl_CmdProc*) ana_global_httppost, NULL, NULL);
#if defined(ANA_MYSQL)
        //add the mysql commands
        Tcl_CreateCommand((*this_style)->tcl_interp, "dbConnect", (Tcl_CmdProc*) ana_db_connect, NULL, NULL);
        Tcl_CreateCommand((*this_style)->tcl_interp, "dbDisconnect", (Tcl_CmdProc*) ana_db_disconnect, NULL, NULL);
        Tcl_CreateCommand((*this_style)->tcl_interp, "dbQuery", (Tcl_CmdProc*) ana_db_query, NULL, NULL);
        Tcl_CreateCommand((*this_style)->tcl_interp, "dbFreeResult", (Tcl_CmdProc*) ana_db_free_result, NULL, NULL);
        Tcl_CreateCommand((*this_style)->tcl_interp, "dbGetResults", (Tcl_CmdProc*) ana_db_get_results, NULL, NULL);
#endif
        //and, another command to let us write out vbase
 Tcl_CreateCommand((*this_style)->tcl_interp, "makeVBase", (Tcl_CmdProc*) make_ana_vbase, NULL, NULL);
 Tcl_CreateCommand((*this_style)->tcl_interp, "seachVBase", (Tcl_CmdProc*) search_ana_vbase, NULL, NULL);
// Tcl_CreateCommand((*this_style)->tcl_interp, "combineVBaseFiles", (Tcl_CmdProc*) combine_ana_vbase, NULL, NULL); we aren't interested in vbase in this
 //make sure we don't get an interactive window
 result= (char *) Tcl_SetVar((*this_style)->tcl_interp, "tcl_interactive", "0", TCL_GLOBAL_ONLY);
 if (result) result= (char *) Tcl_SetVar((*this_style)->tcl_interp, "outputSGML", "0", 0);
 if (result) result= (char *) Tcl_SetVar((*this_style)->tcl_interp, "outputMax", "32000", 0);
 if (result) result= (char *) Tcl_SetVar((*this_style)->tcl_interp, "startSGML", "<", 0);
 if (result) result= (char *) Tcl_SetVar((*this_style)->tcl_interp, "endSGML", ">", 0);
 if (result) result= (char *) Tcl_SetVar((*this_style)->tcl_interp, "closeSGML", "/", 0);
 if (result) result= (char *) Tcl_SetVar((*this_style)->tcl_interp, "tocLevels", "0", 0);
 if (result) result= (char *) Tcl_SetVar((*this_style)->tcl_interp, "outputSiblings", "0", 0);
 if (result) result= (char *) Tcl_SetVar((*this_style)->tcl_interp, "outputRight", "0", 0);
 if (this_book->request_rec==NULL && (strcmp(anastasia_info.anastasia_url, "")==0)) { //anastasia_info.anastasia_url==NULL || otiose
  result = apr_psprintf((*this_style)->style_pool, "http://127.0.0.1/AnaServer?");
 } else if (strcmp(anastasia_info.anastasia_url, "")!=0) { // anastasia_info.anastasia_url!=NULL &&  otiose
  result = apr_psprintf((*this_style)->style_pool, "%s", anastasia_info.anastasia_url);
 } else if (this_book->request_rec->parsed_uri.port_str!=NULL) {
  result = apr_psprintf((*this_style)->style_pool, "http://%s:%s%s?", this_book->request_rec->hostname, this_book->request_rec->parsed_uri.port_str, this_book->request_rec->parsed_uri.path);
 } else {
  result = apr_psprintf((*this_style)->style_pool, "http://%s%s?", this_book->request_rec->hostname, this_book->request_rec->parsed_uri.path);
 }
 Tcl_SetVar((*this_style)->tcl_interp, "url", result, 0);
 //Setup the bookname and sgml directory from the main interpreter
 Tcl_SetVar((*this_style)->tcl_interp, "bookname", (*this_style)->book_name, 0);
 sprintf(str, "%lu", (size_t) this_book);
 Tcl_SetVar((*this_style)->tcl_interp, "this_book_ptr", str, 0);
 sprintf(str, "%lu", (size_t) (*this_style));
 Tcl_SetVar((*this_style)->tcl_interp, "this_style_ptr", str, 0);
 sprintf(str, "%lu", (size_t) this_book->current_pool);
 Tcl_SetVar((*this_style)->tcl_interp, "current_pool", str, 0);
 return true;
}

//sets up begin and end procs
int do_start_end_ana_file(int this_el, char *param, style_ptr this_style, process_book_ptr book_now) {
 int result;
 char *tcl_res, *this_out;
 char el_no_str[32];
 tcl_res = (char *) Tcl_SetVar(this_style->tcl_interp, "text", "", 0);
 sprintf(el_no_str, " %d ", this_el);
 Tcl_SetVar(this_style->tcl_interp, "ana_cmd", param, 0);
// write_log(apr_psprintf(this_style->style_pool, "in do_start_end_ana_file xx 1 %s", param));
 strcpy(this_style->element_name, param);
// write_log("in do_start_end_ana_file 1a");
 char buf[2048];
 sprintf(buf, "%s \"%s\"%s\"%s\"", param, this_style->book_name, el_no_str, this_style->file_name);
// write_log("in do_start_end_ana_file 1b");
// write_log(apr_psprintf(this_style->style_pool, "in do_start_end_ana_file 1b 1 %s bookname %s, filename %s, element %s", param, this_style->book_name,  this_style->file_name, el_no_str));
//result=Tcl_VarEval(this_style->tcl_interp, param, " \"", this_style->book_name, "\"", el_no_str, "\"", this_style->file_name, "\"",(char *) NULL);
//ap_rputs("about to exec", this_style->rec);
// ap_rputs(buf, this_style->rec);
 result=Tcl_Eval(this_style->tcl_interp, buf);
//return 1;
// write_log("in do_start_end_ana_file 1c");
 if (result==TCL_OK) {
  tcl_res= (char *) Tcl_GetVar(this_style->tcl_interp, "text", 0);
  if ((tcl_res[0]) && (!write_output(this_style, book_now, tcl_res))) return 0;
  tcl_res= (char *) Tcl_GetVar(this_style->tcl_interp, "finish", 0);
 //  write_log("in do_start_end_ana_file 1d	");
  if ((tcl_res) && (tcl_res[0]=='1')) return 2;
                tcl_res= (char *) Tcl_GetVar(this_style->tcl_interp, "hide", 0);
                if (tcl_res!=NULL && !strcmp(tcl_res, "children")) {
                    book_now->hide=1;
                } else if (tcl_res!=NULL && !strcmp(tcl_res, "content")) {
                    book_now->hide=2;
                } else {
                    book_now->hide=0;
                }
 } else {
  tcl_res = (char *) Tcl_GetStringResult(this_style->tcl_interp);
  this_out=apr_psprintf(this_style->style_pool, "<p><font color=red>Error processing Anastasia style in file \"%s\" for element \"%s\": %s in line %d</font></p>\n", this_style->file_name, param, tcl_res, Tcl_GetErrorLine(this_style->tcl_interp));
  write_output(this_style, book_now, this_out);
 }
 //check finish: if it is set, return 2 and SKIP calling content
//  write_log("in do_start_end_ana_file 2");
 return 1;
}

int output_ana_toc(int this_el, int levs_sought, int levs_found, style_ptr this_style, process_book_ptr this_book) {
 sgml_obj_ptr this_sgml_el;
 int next_el;
 int orig_levs_found=levs_found;
 do {
  if (!get_next_sgml_obj(&this_sgml_el, this_el, this_book)) return false;
  next_el=(this_sgml_el)->rsib;
  //do before and after...only IF this element has a titlespec 
  if ((has_tspec(this_sgml_el,this_style, this_book)) && (!process_style(this_sgml_el, this_style, this_book, "before"))) return false;
  switch (is_toc_el(this_sgml_el, this_style, this_book)) {
   case 0: return false;  //error
   case 1:            //found, but go onto case 2 in case further levels needed, so look in the children
    levs_found++;  
   case 2:  { //not found, or continuing on from case 1;
    if (((this_sgml_el)->child) && (levs_found<levs_sought) && (!output_ana_toc((this_sgml_el)->child, this_style->outputTOC, levs_found, this_style, this_book))) return 0;
    break;
   }
   case 3: {  //hide element: so do not look in children 
    break;
   }
  }
  //we will check for finish variable here: if it is set, we stop right now... (again, can only be set in elements which have titlespecs)
  this_el=next_el;   
  if (has_tspec(this_sgml_el,this_style, this_book)) {
   switch (process_style(this_sgml_el, this_style, this_book, "after")) {
    case 0: return false;
    case 4: {
     this_el=0;
    }
   } 
  }
  //do the right siblings, then; return levs_found to original value on entry to this level
  levs_found=orig_levs_found;
 } while (this_el);
 return true;
}

int has_tspec(sgml_obj_ptr this_sgml_el, style_ptr this_style, process_book_ptr this_book) {
 char tcl_cmdstr[512];
 char *this_out;
 if (find_tcl_cmd(this_style->tcl_interp, this_sgml_el->path, tcl_cmdstr)) {
  //process it!
  char el_no_str[32];
  char *tcl_res;
  int result;
  tcl_res = (char *) Tcl_SetVar(this_style->tcl_interp, "titleSpec", "", 0);
  sprintf(el_no_str, " %d ", this_sgml_el->number);
  //used to ensure that when we are asked to process content of an element, that the
  //element is NOT a parent of this element (which could cause true nastiness)
  this_book->current_element_no = this_sgml_el->number;
  result=Tcl_VarEval(this_style->tcl_interp, tcl_cmdstr, el_no_str, "toc", (char *) NULL);
  if (result==TCL_OK) {
   tcl_res = (char *) Tcl_GetVar(this_style->tcl_interp, "titleSpec", 0);
   if (tcl_res[0]) {
    return true;
   } else {
    return false;
   }
  } else {
   tcl_res = (char *) Tcl_GetStringResult(this_style->tcl_interp);
   this_out=apr_psprintf(this_style->style_pool, "<p><font color=red>Error processing Anastasia style in file \"%s\" for element \"%s\": %s in line %d</font></p>\n", this_style->file_name, tcl_cmdstr, tcl_res, Tcl_GetErrorLine(this_style->tcl_interp));
   write_log(this_out);
  }
 }
 return false;
}

int find_tcl_cmd(Tcl_Interp *style_interp, text_obj *path, char *tcl_cmdstr) {
 Tcl_CmdInfo infoPtr;
 int the_result;
 char path_cpy[512];
 //if this is #sdata or #pcdata: remove #...
 strcpy(path_cpy, (*path).ptr);
 strcpy(tcl_cmdstr, (*path).ptr);
 the_result=Tcl_GetCommandInfo(style_interp,path_cpy, &infoPtr);
 if (the_result) {
  //Set up the command
  Tcl_SetVar(style_interp, "ana_cmd", path_cpy, 0);
  strcpy(tcl_cmdstr, path_cpy);
  return true;
 }
 while (remove_to_token(tcl_cmdstr,",", path_cpy)) {
  the_result=Tcl_GetCommandInfo(style_interp,tcl_cmdstr, &infoPtr);
  if (the_result) {
   Tcl_SetVar(style_interp, "ana_cmd", tcl_cmdstr, 0);
   return true;
  }
 }
        //Couldn't find the command.
        strcpy(tcl_cmdstr, "anaGlobal");
        the_result = Tcl_GetCommandInfo(style_interp, tcl_cmdstr, &infoPtr);
        if (the_result) {
            Tcl_SetVar(style_interp, "ana_cmd", tcl_cmdstr, 0);
            return true;
        }
 return false;
}

void update_curr_search(int this_el, process_book_ptr this_book) {
 int c;
 for (c=this_book->current_search.ntexts_done; c<this_book->current_search.ntexts_found; c++) {
  void *place= (void *)((unsigned long) this_book->search_texts_found + (c * sizeof(struct bounds_obj))); 
  struct bounds_obj the_return;
  memcpy(&the_return, place, sizeof(struct bounds_obj));
  if (the_return.start_el>=this_el) {
    this_book->current_search.start_el=the_return.start_el;
    this_book->current_search.start_off=the_return.start_off;
    this_book->current_search.end_el=the_return.end_el;
    this_book->current_search.end_off=the_return.end_off;
    this_book->current_search.ntexts_done=c;
    return;
  }
 }
 this_book->current_search.ntexts_done=c;
 this_book->in_search=0;  //ie, all searches processed!
}

int is_toc_el(sgml_obj_ptr this_sgml_el, style_ptr this_style, process_book_ptr this_book) {
 int el_sought, last_el;
 switch (process_style(this_sgml_el, this_style, this_book, "toc")) {
  case 0: return 0;  //error!
  case 1: return 2;  //no titleSpec found.  Go on looking.
  case 2: return 3;  //hide children found
  case 3:   {
   //got it.  find it... 
   char title_spec[512], this_el[512];
   char *tcl_res;
   tcl_res = (char *) Tcl_GetVar(this_style->tcl_interp, "titleSpec", 0);
   strcpy(title_spec, tcl_res);
   el_sought=this_sgml_el->number;
   while (remove_to_token(title_spec,",", this_el)) {
    switch (find_child(el_sought,1, this_el, &el_sought, this_book)) {  
      case 0: return 0;   //error!
      case 1: return 2;  //Not found, no error.  don't give a warning.  Up to users to figure out something is wrong
      case 2: ;   //ok; just carry on
    }
   }
   //will be only one element left in this. don't give a warning.  Up to users to figure out something is wrong
   switch (find_child(el_sought,1, title_spec, &el_sought, this_book)) {
      case 0: return 0;   //error!
      case 1: return 2;  //Not found, no error.  don't give a warning.  Up to users to figure out something is wrong
      case 2: ;   //ok; just carry on
   }
   //after getting here: el_sought must hold the number of the element we are looking for.  Output it!! (NOT its siblings also)
   if (!output_element(el_sought, false, false, this_style, &last_el, NULL, this_book)) return 0;
   return 1;        //successfully found and output
  }
 }
 return 1;   //actually should never get here
}

//return values: 0 is fatal error; 1 indicates style processed successfully for before, after and content contexts 
//1 may also indicate safe return after error
//2 indicates hide children style found; 3 indicates titleSpec found
//4 indicates finish variable set (can only happen in context before or after: not in content context
//5 indicated hide content!
int process_style(sgml_obj_ptr this_sgml_el, style_ptr this_style,  process_book_ptr this_book, char *param) {
 char tcl_cmdstr[512];
 char *tcl_res;
 char *this_out;
 int result;
 //is a style defined for this element?
 if (find_tcl_cmd(this_style->tcl_interp, this_sgml_el->path, tcl_cmdstr) ) {
 //process it!
  char el_no_str[32];
  tcl_res = (char *) Tcl_SetVar(this_style->tcl_interp, "text", "", 0);
  tcl_res = (char *) Tcl_SetVar(this_style->tcl_interp, "finish", "", 0);
  tcl_res = (char *) Tcl_SetVar(this_style->tcl_interp, "titleSpec", "", 0);
  if (!strcmp("content", param))  tcl_res = (char *) Tcl_SetVar(this_style->tcl_interp, "hide", "", 0);
  if (!strcmp("toc", param))  {
   tcl_res = (char *) Tcl_SetVar(this_style->tcl_interp, "titleSpec", "", 0);
  }
  sprintf(el_no_str, " %d ", this_sgml_el->number);
  this_book->current_element_no = this_sgml_el->number;
                //used to ensure that when we are asked to process content of an element, that the
                //element is NOT a parent of this element (which could cause true nastiness)
                //Add in the current element name
                strcpy(this_style->element_name, tcl_cmdstr);
  result=Tcl_VarEval(this_style->tcl_interp, tcl_cmdstr, el_no_str, param, (char *) NULL);
  if (result==TCL_OK) {
   if (!strcmp("toc", param))  {  //table of contents call
    //first check hide: if hide, we ignore any titleSpecs and hide the whole element, as usual
    tcl_res = (char *) Tcl_GetVar(this_style->tcl_interp, "hide", 0);
    if (tcl_res!=NULL && !strcmp("children", tcl_res)) {
                                    this_book->hide = 1;
                                    return 2;
                                } else if (tcl_res!=NULL && !strcmp("content", tcl_res)) {
                                    this_book->hide = 2;
                                    return 5;
                                }
    tcl_res = (char *) Tcl_GetVar(this_style->tcl_interp, "titleSpec", 0);
    if (tcl_res[0]) return 3;
    else return 1;
   } else {  //before, after, or content calls.  We check for finish variable in empty elements and in after calls
    tcl_res = (char *) Tcl_GetVar(this_style->tcl_interp, "text", 0);
    if ((tcl_res[0]) &&
      (!write_output(this_style, this_book, tcl_res))) return 0;
     // check finish if this is an empty element and finish set (by definition, must be in before context only)
    if ((this_sgml_el->cont_type)==0) {
     tcl_res = (char *) Tcl_GetVar(this_style->tcl_interp, "finish", 0);
     if (!strcmp("1", tcl_res)) return 4;
    }
    if (!strcmp("after", param)) {
     tcl_res = (char *) Tcl_GetVar(this_style->tcl_interp, "finish", 0);
     if (!strcmp("1", tcl_res)) 
      return 4;
    }
    tcl_res = (char *) Tcl_GetVar(this_style->tcl_interp, "hide", 0);
    if (tcl_res!=NULL && !strcmp("children", tcl_res)) {
     this_book->hide = 1;
     return 2;
    } else if (tcl_res!=NULL && !strcmp("content", tcl_res)) {
     this_book->hide = 2;
     return 5;
    } else {
     this_book->hide = 0;
    }
   }
  } else {
   tcl_res = (char *) Tcl_GetStringResult(this_style->tcl_interp);
   this_out=apr_psprintf(this_style->style_pool, "<p><font color=red>Error processing Anastasia style in file \"%s\" for element \"%s\": %s in line %d</font></p>\n", this_style->file_name, tcl_cmdstr, tcl_res, Tcl_GetErrorLine(this_style->tcl_interp));
   write_output(this_style, this_book, this_out);
   return 0;   
  }
  return 1;
 }
 return 1;
}

//this one really does the work....we are opening a new element
//process this element AND all its right siblings (if output_sibs is true; else just put out this element and its children
//added 16 June 2000: we now will move up deal with ALL right part of doc if output_right is set in style sheet
//note that this_sgml_el is guaranteed to remain 'in scope' because subelements will 'look' up to it at each access,
//as they have to derive their complete path. 
//maybe there is a circumstance where this is NOT true.  If so, will cope with it when it arises
//in that case, one would have to make a local copy of each element as it was accessed.  Cumbersome and inelegant.
//altered slightly: keep number of next el as local variable; call get_next_sgml_obj in each of the functions just 
//to be sure that it is still in scope.  The processing overhead should be very slight
//no need to call in output start as we have just called get_next_sgml_obj
//note special value 4 returned in output: this indicates finish variable set in those.  We return
//number of this element to the caller
//we don't think the levels variable does anything!!!
//note that output_right can ONLY be true when called from toplevel make reader file.  All nested calls to this must set it as false
//now too: if outputMax is exceeded, chuck it back

int output_element(int this_el, int outputSiblings, int outputRight, style_ptr this_style, int *last_el, bounds_ptr bounds, process_book_ptr this_book) {
 sgml_obj_ptr this_sgml_el=NULL, temp_sgml_el=NULL;
 char *this_out, *tcl_return;
 int startEl, orighide=this_book->hide;
 int next_el;
 do  {
  startEl = 0;
  *last_el=this_el;
  if (!get_next_sgml_obj(&temp_sgml_el, this_el, this_book)) return 0;
  if (!clone_sgml_obj(temp_sgml_el, &this_sgml_el, this_book, __FILE__, __LINE__)) return 0;
  next_el=(this_sgml_el)->rsib;
                orighide = this_book->hide;
  switch(output_start(this_sgml_el, this_style, this_book)) {
  case 0: {
    dispose_clone_sgml_obj(&this_sgml_el, this_book);
    return 0;
    }
  case 4: {
    dispose_clone_sgml_obj(&this_sgml_el, this_book);
    return 4;
    }
  }
   if (this_style->file_spec->len>this_style->outputMax) {
   this_out = apr_psprintf(this_style->style_pool, "<p><font color=red>OutputMax value exceeded while writing element number %d (a \"%s\") in style file \"%s\".  Check settings for outputSiblings, outputRight, and use of the \"finish\" variable.</font></p> ", (this_sgml_el)->number, (this_sgml_el)->gi->ptr, this_style->file_name);
   write_output(this_style, this_book, this_out);
   dispose_clone_sgml_obj(&this_sgml_el, this_book);
   return 0;
  }
  //Check the memory to see we have had a problem allocating memory allong the way
  if (this_book->anastasia_memory.memory_error) {
   this_out = apr_psprintf(this_style->style_pool, "<p><font color=red>Memory usage exceeded while writing element number %d (a \"%s\") in style file \"%s\".  Check settings for outputSiblings, outputRight, and use of the \"finish\" variable.</font></p> ", (this_sgml_el)->number, (this_sgml_el)->gi->ptr, this_style->file_name);
   write_output(this_style, this_book, this_out);
   dispose_clone_sgml_obj(&this_sgml_el, this_book);
   return 0;
  }
  //only output content and end if this is not an empty element
  if ((this_sgml_el->cont_type)!=0) {
     switch(output_content(this_el, this_style, last_el, bounds, this_book)) {
   case 0: {
     dispose_clone_sgml_obj(&this_sgml_el, this_book);
     return 0;
     }
   case 4: {
     dispose_clone_sgml_obj(&this_sgml_el, this_book);
     return 4;
     }
   }
   if (this_style->file_spec->len>this_style->outputMax) {
    this_out = apr_psprintf(this_style->style_pool, "<p><font color=red>OutputMax value exceeded while writing element number %d (a \"%s\") in style file \"%s\".  Check settings for outputSiblings, outputRight, and use of the \"finish\" variable.</font></p> ", (this_sgml_el)->number, (this_sgml_el)->gi->ptr, this_style->file_name );
    write_output(this_style, this_book, this_out);
    dispose_clone_sgml_obj(&this_sgml_el, this_book);
    return 0;
   }
   //Check the memory to see we have had a problem allocating memory allong the way
   if (this_book->anastasia_memory.memory_error) {
    this_out = apr_psprintf(this_style->style_pool, "<p><font color=red>Memory usage exceeded while writing element number %d (a \"%s\") in style file \"%s\".  Check settings for outputSiblings, outputRight, and use of the \"finish\" variable.</font></p> ", (this_sgml_el)->number, (this_sgml_el)->gi->ptr, this_style->file_name);
    write_output(this_style, this_book, this_out);
    dispose_clone_sgml_obj(&this_sgml_el, this_book);
    return 0;
   }
   *last_el=this_el; //could have been reset in error
   switch(output_end(this_el, this_style, this_book)) {
   case 0: {
     dispose_clone_sgml_obj(&this_sgml_el, this_book);
     return 0;
     }
   case 4: {
     dispose_clone_sgml_obj(&this_sgml_el, this_book);
     return 4;
     }
   }
   if (this_style->file_spec->len>this_style->outputMax) {
    this_out = apr_psprintf(this_style->style_pool, "<p><font color=red>OutputMax value exceeded while writing element number %d (a \"%s\") in style file \"%s\".  Check settings for outputSiblings, outputRight, and use of the \"finish\" variable.</font></p> ", (this_sgml_el)->number, (this_sgml_el)->gi->ptr, this_style->file_name );
    dispose_clone_sgml_obj(&this_sgml_el, this_book);
    write_output(this_style, this_book, this_out);
    return 0;
   }
   //Check the memory to see we have had a problem allocating memory allong the way
   if (this_book->anastasia_memory.memory_error) {
    this_out = apr_psprintf(this_style->style_pool, "<p><font color=red>Memory usage exceeded while writing element number %d (a \"%s\") in style file \"%s\".  Check settings for outputSiblings, outputRight, and use of the \"finish\" variable.</font></p> ", (this_sgml_el)->number, (this_sgml_el)->gi->ptr, this_style->file_name);
    write_output(this_style, this_book, this_out);
    dispose_clone_sgml_obj(&this_sgml_el, this_book);
    return 0;
   }
  }
  *last_el=this_el;
  this_el=next_el;  
  //put in here a trap: if output_right is set, and this_el is 0, then move up to right sibling of ancestor
  //but don't do this if the last element was the root, ie *last_el==0, coz this will already have been output
  //note we have to output end of ancestor!!! rather crucial... 
  if (!this_el && (*last_el) && outputSiblings && outputRight) {
   //get the ancestor, output its end, then its right sibling
   do {
    dispose_clone_sgml_obj(&this_sgml_el, this_book);
    if (!get_next_sgml_obj(&temp_sgml_el, *last_el, this_book)) return 0;
    clone_sgml_obj(temp_sgml_el, &this_sgml_el, this_book, __FILE__, __LINE__);
    this_el=(this_sgml_el)->ancestor;
    //must output the end of this.  Cannot possibly be an empty element! (could be root; yes, still output its end)
    switch(output_end(this_el, this_style, this_book)) {
    case 0: {
      dispose_clone_sgml_obj(&this_sgml_el, this_book);
      return 0;
      }
    case 4: {
      dispose_clone_sgml_obj(&this_sgml_el, this_book);
      return 4;
      }
    }
    if (this_style->file_spec->len>this_style->outputMax) {
     this_out = apr_psprintf(this_style->style_pool, "<p><font color=red>OutputMax value exceeded while writing element number %d (a \"%s\") in style file \"%s\".  Check settings for outputSiblings, outputRight, and use of the \"finish\" variable.</font></p> ", (this_sgml_el)->number, (this_sgml_el)->gi->ptr, this_style->file_name );
     dispose_clone_sgml_obj(&this_sgml_el, this_book);
     write_output(this_style, this_book, this_out);
     return 0;
    }
    //Check the memory to see we have had a problem allocating memory allong the way
    if (this_book->anastasia_memory.memory_error) {
     this_out = apr_psprintf(this_style->style_pool, "<p><font color=red>Memory usage exceeded while writing element number %d (a \"%s\") in style file \"%s\".  Check settings for outputSiblings, outputRight, and use of the \"finish\" variable.</font></p> ", (this_sgml_el)->number, (this_sgml_el)->gi->ptr, this_style->file_name);
     write_output(this_style, this_book, this_out);
     dispose_clone_sgml_obj(&this_sgml_el, this_book);
     return 0;
    }
    dispose_clone_sgml_obj(&this_sgml_el, this_book);
    if (!get_next_sgml_obj(&temp_sgml_el, this_el, this_book)) return 0;
    clone_sgml_obj(temp_sgml_el, &this_sgml_el, this_book, __FILE__, __LINE__);
    next_el=(this_sgml_el)->rsib;
    *last_el=this_el;
   } while (!next_el && this_el);  //keep looping up until we have a right sib, so long as this element is not the root
   this_el=next_el;
  }
  dispose_clone_sgml_obj(&this_sgml_el, this_book);
  //If we check the startEl variable here and see if its been set so that we can jump from one part in the document
  //to another... could be very useful.
  tcl_return = (char *) Tcl_GetVar(this_style->tcl_interp, "startEl", 0);
  if (tcl_return) {
   //if not successful: there will be a warning and all will go just as before
   if (is_valid_el(tcl_return, "URLPARAM", &temp_sgml_el, this_book, this_style)) {
    dispose_clone_sgml_obj(&this_sgml_el, this_book);
    clone_sgml_obj(temp_sgml_el, &this_sgml_el, this_book, __FILE__, __LINE__);
   }
   //Unset the startEl to prevent other procedures getting confused later on
   Tcl_Eval(this_style->tcl_interp, "unset startEl");
   this_el = this_sgml_el->number;
   //Need to skip the to the next element
   startEl = 1;
  }
                this_book->hide = orighide;
 } while ((this_el && outputSiblings) || startEl);
 return 1;
}

int output_start(sgml_obj_ptr this_sgml_el, style_ptr this_style, process_book_ptr this_book) {
 int result;
 attr_ptr this_attr;
 result = process_style(this_sgml_el, this_style, this_book, "before");
 if (!result) return 0;  //error
 //before we get here we will read the style setting in the anv file
 if (this_style->outputSGML && result<4) {
  if ((!strncmp(this_sgml_el->gi->ptr,"SDATA", this_sgml_el->gi->len)) || (!strncmp(this_sgml_el->gi->ptr,"PCDATA", this_sgml_el->gi->len))) {
   //don't write these out at start or end but in content...
   return result;
  } else {
   write_output(this_style, this_book, apr_psprintf(this_style->style_pool, "%s%s", this_style->startSGML, this_sgml_el->gi->ptr));
   if (this_sgml_el->n_attrs>0)  {
    this_attr = this_sgml_el->attrs;
    while (this_attr!=0) {
     write_output(this_style, this_book, apr_psprintf(this_style->style_pool, " %s=\"%s\"", this_attr->name->ptr, this_attr->value->ptr));
     this_attr = this_attr->nattr;
    }
   }
   write_output(this_style, this_book, apr_psprintf(this_style->style_pool, "%s", this_style->endSGML));
  }
 }
 return result;
}

//note that we have defined that the finish element can ONLY be called in before (for empty elements) and after contexts
//so we only need to check for return val of 4 in nested output element calls
int output_content(int this_el, style_ptr this_style, int *last_el, bounds_ptr bounds, process_book_ptr this_book) {
 sgml_obj_ptr this_sgml_el=NULL, temp_sgml_el=NULL;
 char *this_out;
 int result;
 int prev_search_el, prev_search_offset;
  //keep the element in scope
 if (!get_next_sgml_obj(&temp_sgml_el, this_el, this_book)) return 0;
 if (!clone_sgml_obj(temp_sgml_el, &this_sgml_el, this_book, __FILE__, __LINE__)) return 0;
 //before we get here we will read the style setting in the anv file
 //just write out the pcdata if that is what we have (no children!) and return
 //is the pcdata already in the element?
 //if we come back with 2: hide children!!
 //now, if we are in a search, matters get complex... could be in a search when we start this element
 //could be sdata, where we are hiding the content; if so, and it is the end of the search: call end proc here
 switch (process_style(this_sgml_el, this_style, this_book, "content")) {
  case 0: {
   dispose_clone_sgml_obj(&this_sgml_el, this_book);
   return 0;  //error!
                        }
  case 2:
   //Does this need to be this_el>=this_book->current_search.end_el in case we miss out the element that
   //the search ends in
   //Need to check previous search as well.
   previous_search(this_book, &prev_search_el, &prev_search_offset);
   if ((this_book->in_search) && (this_el>=this_book->current_search.end_el))  {
    sgml_obj_ptr pc_clone;
    if (!clone_sgml_obj(this_sgml_el,&pc_clone, this_book, __FILE__, __LINE__)) {
      dispose_clone_sgml_obj(&this_sgml_el, this_book);
      return 0;
    }
    dispose_clone_sgml_obj(&this_sgml_el, this_book);
    //Now add on the ,found to the path
    add_str_to_text_obj(pc_clone->path, ",found", this_book, pc_clone->object_pool);
    if (!process_style(pc_clone, this_style, this_book, "after")) {
     dispose_clone_sgml_obj(&pc_clone, this_book);
     return 0;
    }
    increment_curr_search(this_book);
    dispose_clone_sgml_obj(&pc_clone, this_book);
   } else if ((this_el>=(unsigned long) prev_search_el) && (prev_search_el!=0)) {
    //We should finish a search here.
    sgml_obj_ptr pc_clone;
    if (!clone_sgml_obj(this_sgml_el,&pc_clone, this_book, __FILE__, __LINE__)) {
      dispose_clone_sgml_obj(&this_sgml_el, this_book);
      return 0;
    }
    dispose_clone_sgml_obj(&this_sgml_el, this_book);
    //Now add on the ,found to the path
    add_str_to_text_obj(pc_clone->path, ",found", this_book, pc_clone->object_pool);
    if (!process_style(pc_clone, this_style, this_book, "after")) {
     dispose_clone_sgml_obj(&pc_clone, this_book);
     return 0;
    }
    dispose_clone_sgml_obj(&pc_clone, this_book);
   } else {
    dispose_clone_sgml_obj(&this_sgml_el, this_book);
   }
 }
 if (bounds) {
  if (bounds->end_el<this_el || (bounds->end_el==this_el && bounds->end_off==0)) {
   dispose_clone_sgml_obj(&this_sgml_el, this_book);
   return 4;  //should have stopped already
  }
 }
        if (this_book->hide==1) {
            return 1;
        }
 if (!strncmp((this_sgml_el)->gi->ptr,"PCDATA", (this_sgml_el)->gi->len) && this_book->hide!=2) {
  int temp_el;
  int temp_offset;
  previous_search(this_book, &temp_el, &temp_offset);
  //O.k. are we in a search and does the search at least begin in this element?
  if (((this_book->in_search) && (this_book->current_search.start_el==this_el)) || ((temp_el==(int) this_el) && temp_el!=0)) {
   //Probably don't have to worry about bounds in this part.. or do we?
   //O.k. this is pretty simple all we have to do is output whats before the find
   //output the start of the find, then output whats after.. and possible the end of the find as well.
   size_t to_write = 0;
   sgml_obj_ptr pc_clone;
   //O.k. first check to see if we need to close off an unfinished search
   if (temp_el==(int) this_el) {
    //Yep so.
    to_write = temp_offset + 1;
//    (this_sgml_el)->pcdata->ptr[to_write]='\0';
    (this_sgml_el)->pcdata->len = to_write;
    read_str_exfile(this_sgml_el->pcdata, this_sgml_el->start_data , this_book->in_sgv, this_style, this_book, this_style->style_pool);
    this_out = apr_psprintf(this_style->style_pool, "%.*s", (int) (this_sgml_el)->pcdata->len, (this_sgml_el)->pcdata->ptr);
    write_output(this_style, this_book, this_out);
    //Close the found element
    if (!clone_sgml_obj(this_sgml_el,&pc_clone, this_book, __FILE__, __LINE__)) {
      dispose_clone_sgml_obj(&this_sgml_el, this_book);
      return 0;
    }
    add_str_to_text_obj(pc_clone->path, ",found", this_book, pc_clone->object_pool);
    if (!process_style(pc_clone, this_style, this_book, "after")) {
     dispose_clone_sgml_obj(&pc_clone, this_book);
     return 0;
    }
    dispose_clone_sgml_obj(&pc_clone, this_book);
   }
   if (this_book->current_search.start_off!=0) {
    //Write out the first bit of text.
    previous_search(this_book, &temp_el, &temp_offset);
    if (temp_el!=(int) this_el) { temp_offset=0; }
    to_write = this_book->current_search.start_off - temp_offset + 1;
//    (this_sgml_el)->pcdata->ptr[to_write]='\0';
    (this_sgml_el)->pcdata->len = to_write;
    read_str_exfile(this_sgml_el->pcdata, this_sgml_el->start_data + temp_offset , this_book->in_sgv, this_style, this_book, this_style->style_pool);
    this_out = apr_psprintf(this_style->style_pool, "%.*s", (int) (this_sgml_el)->pcdata->len, (this_sgml_el)->pcdata->ptr);
    write_output(this_style, this_book, this_out);
   }
   while (this_book->current_search.start_el==this_el) {
    //Write out the start of the find
    if (!clone_sgml_obj(this_sgml_el,&pc_clone, this_book, __FILE__, __LINE__)) {
      dispose_clone_sgml_obj(&this_sgml_el, this_book);
      return 0;
    }
    add_str_to_text_obj(pc_clone->path, ",found", this_book, pc_clone->object_pool);
    if (!process_style(pc_clone, this_style, this_book, "before")) {
     dispose_clone_sgml_obj(&pc_clone, this_book);
     return 0;
    }
    dispose_clone_sgml_obj(&pc_clone, this_book);
    //Write out the text found
    if (this_book->current_search.end_el==this_el) {
     to_write = this_book->current_search.end_off - this_book->current_search.start_off + 1;
//     (this_sgml_el)->pcdata->ptr[to_write]='\0';
     (this_sgml_el)->pcdata->len = to_write;
     read_str_exfile(this_sgml_el->pcdata, this_sgml_el->start_data + this_book->current_search.start_off, this_book->in_sgv, this_style, this_book, this_style->style_pool);
     this_out = apr_psprintf(this_style->style_pool, "%.*s", (int) (this_sgml_el)->pcdata->len, (this_sgml_el)->pcdata->ptr);
     write_output(this_style, this_book, this_out);
     if (!clone_sgml_obj(this_sgml_el,&pc_clone, this_book, __FILE__, __LINE__)) {
       dispose_clone_sgml_obj(&this_sgml_el, this_book);
       return 0;
     }
     add_str_to_text_obj(pc_clone->path, ",found", this_book, pc_clone->object_pool);
     if (!process_style(pc_clone, this_style, this_book, "after")) {
      dispose_clone_sgml_obj(&pc_clone, this_book);
      return 0;
     }
     dispose_clone_sgml_obj(&pc_clone, this_book);
    }
    next_search(this_book, &temp_el, &temp_offset);
    if (temp_el!=(int) this_el) { temp_offset=0; }
    //Now write out an text thats left..
    if (this_book->current_search.end_el==this_el) {
     to_write = this_sgml_el->sgrep_end - this_sgml_el->sgrep_start - this_book->current_search.end_off - temp_offset + 1;
    } else {
     to_write = this_sgml_el->sgrep_end - this_sgml_el->sgrep_start - this_book->current_search.start_off - temp_offset + 1;
    }
    if (temp_el==(int) this_el) {
     to_write = temp_offset - this_book->current_search.end_off + 1;
    }
    //Need to increase the pcdata size
//    (this_sgml_el)->pcdata->ptr[to_write]='\0';
    (this_sgml_el)->pcdata->len = to_write;
    if (this_book->current_search.end_el==this_el) {
     read_str_exfile(this_sgml_el->pcdata, this_sgml_el->start_data + this_book->current_search.end_off, this_book->in_sgv, this_style, this_book, this_style->style_pool);
    } else {
     read_str_exfile(this_sgml_el->pcdata, this_sgml_el->start_data + this_book->current_search.start_off, this_book->in_sgv, this_style, this_book, this_style->style_pool);
    }
    this_out = apr_psprintf(this_style->style_pool, "%.*s", (int) (this_sgml_el)->pcdata->len, (this_sgml_el)->pcdata->ptr);
    write_output(this_style, this_book, this_out);
    increment_curr_search(this_book);
   }
  } else {
   if ((!bounds) || ((bounds->start_el!=this_el) && (bounds->end_el!=this_el))) {
    (this_sgml_el)->pcdata->len=(this_sgml_el)->end_data - (this_sgml_el)->start_data + 1;
    if (!set_text_obj_size(this_sgml_el->pcdata, this_book, this_sgml_el->object_pool)) {
     dispose_clone_sgml_obj(&this_sgml_el, this_book);
     return 0;
    }
    apr_off_t _where = (this_sgml_el)->start_data;
    apr_file_seek(this_book->in_sgv, APR_SET, &_where);
   } else {  //use bounds to control just how much is got
    int chars_to_get=(this_sgml_el)->end_data - (this_sgml_el)->start_data;
    apr_off_t _where = (this_sgml_el)->start_data;
    apr_file_seek(this_book->in_sgv, APR_SET, &_where);
    if (bounds->start_el==this_el)  {
     _where = (this_sgml_el)->start_data + bounds->start_off;
     apr_file_seek(this_book->in_sgv, APR_SET, &_where);
     chars_to_get-=bounds->start_off;
    }
    if (bounds->end_el==this_el)  chars_to_get-=((this_sgml_el)->end_data - (this_sgml_el)->start_data) - bounds->end_off;
    (this_sgml_el)->pcdata->len=chars_to_get + 1;
    if (!set_text_obj_size(this_sgml_el->pcdata, this_book, this_sgml_el->object_pool)) {
     dispose_clone_sgml_obj(&this_sgml_el, this_book);
     return 0;
    }
   }
   apr_size_t _count = sizeof(char) * ((this_sgml_el)->pcdata->len - 1);
   apr_file_read(this_book->in_sgv, (this_sgml_el)->pcdata->ptr, &_count);
   if (_count != (sizeof(char) * ((this_sgml_el)->pcdata->len - 1))) {
    write_output(this_style, this_book, "Error reading Anastasia .sgv file.\n");
    dispose_clone_sgml_obj(&this_sgml_el, this_book);
    return 0;
   }
   (this_sgml_el)->pcdata->ptr[(this_sgml_el)->pcdata->len - 1]='\0';
   this_out = apr_psprintf(this_style->style_pool, "%.*s", (int)((this_sgml_el)->pcdata->len - 1), (this_sgml_el)->pcdata->ptr);
   write_output(this_style, this_book, this_out);
   if ((bounds) && (bounds->end_el==this_el)) {
    dispose_clone_sgml_obj(&this_sgml_el, this_book);
    return 4;
   }
   dispose_clone_sgml_obj(&this_sgml_el, this_book);
   return 1;
  }
 } else if (!strncmp((this_sgml_el)->gi->ptr,"SDATA", (this_sgml_el)->gi->len) && this_book->hide!=2) {
  if (this_style->outputSGML) {
   this_out = apr_psprintf(this_style->style_pool, "&amp;%.*s;", (int)((this_sgml_el)->attrs->name->len - 1), (this_sgml_el)->attrs->name->ptr);
   write_output(this_style, this_book, this_out);
   dispose_clone_sgml_obj(&this_sgml_el, this_book);
   return 1;
  }
  else {
   this_out = apr_psprintf(this_style->style_pool, "%.*s", (int) ((this_sgml_el)->attrs->value->len - 1), (this_sgml_el)->attrs->value->ptr);
   write_output(this_style, this_book, this_out);
  }
 }
 if ((this_sgml_el)->child) {
  result = output_element((this_sgml_el)->child, true, false, this_style, last_el, bounds, this_book);
  dispose_clone_sgml_obj(&this_sgml_el, this_book);
  return result;
 }
 dispose_clone_sgml_obj(&this_sgml_el, this_book);
 return 1;
}

//if process style returns 4: finish met in the after context -- return 4...
int output_end(int this_el, style_ptr this_style, process_book_ptr this_book) {
 sgml_obj_ptr this_sgml_el;
 char *this_out;
 //keep the element in scope
  if (!get_next_sgml_obj(&this_sgml_el, this_el, this_book)) return 0;
 if (this_style->outputSGML) {
  if ((strncmp(this_sgml_el->gi->ptr,"SDATA", this_sgml_el->gi->len)) 
   &&  (strncmp(this_sgml_el->gi->ptr,"PCDATA", this_sgml_el->gi->len))
   && (this_sgml_el->cont_type))  {
   this_out = apr_psprintf(this_style->style_pool, "%s%s%s%s", this_style->startSGML, this_style->closeSGML, this_sgml_el->gi->ptr, this_style->endSGML);
   write_output(this_style, this_book, this_out);
  }
 }
  // here we will read the style setting in the anv file
 return(process_style(this_sgml_el, this_style, this_book, "after"));
}

void increment_curr_search(process_book_ptr this_book_ptr) {
 void *place;
 struct bounds_obj *herenow;
 struct bounds_obj the_return;
 this_book_ptr->current_search.ntexts_done++;
 if (this_book_ptr->current_search.ntexts_found==this_book_ptr->current_search.ntexts_done) {
  this_book_ptr->in_search=0;
  this_book_ptr->current_search.start_el=-1;
  return;
 }
 place= (void *)((unsigned long) this_book_ptr->search_texts_found + (this_book_ptr->current_search.ntexts_done * sizeof(struct bounds_obj)));
        herenow= (struct bounds_obj *) place; 
 memcpy(&the_return, place, sizeof(struct bounds_obj));
 this_book_ptr->current_search.start_el=the_return.start_el;
  this_book_ptr->current_search.start_off=the_return.start_off;
  this_book_ptr->current_search.end_el=the_return.end_el;
  this_book_ptr->current_search.end_off=the_return.end_off;
}

void previous_search(process_book_ptr this_book_ptr, int *prev_end_el, int *prev_end_offset) {
 void *place;
 struct bounds_obj *herenow;
 struct bounds_obj the_return;
 if (this_book_ptr->current_search.ntexts_done==0) {
  *prev_end_el = 0;
  *prev_end_offset = 0;
 } else {
  place= (void *)((unsigned long) this_book_ptr->search_texts_found + ((this_book_ptr->current_search.ntexts_done -1) * sizeof(struct bounds_obj)));
        herenow= (struct bounds_obj *) place; 
  memcpy(&the_return, place, sizeof(struct bounds_obj));
  *prev_end_el = the_return.end_el;
  *prev_end_offset = the_return.end_off;
 }
}

void next_search(process_book_ptr this_book_ptr, int *next_end_el, int *next_end_offset) {
 void *place;
 struct bounds_obj *herenow;
 struct bounds_obj the_return;
 if ((this_book_ptr->current_search.ntexts_done + 1)==this_book_ptr->current_search.ntexts_found) {
  *next_end_el = 0;
  *next_end_offset = 0;
 } else {
  place= (void *)((unsigned long) this_book_ptr->search_texts_found + ((this_book_ptr->current_search.ntexts_done + 1) * sizeof(struct bounds_obj)));
        herenow= (struct bounds_obj *) place; 
  memcpy(&the_return, place, sizeof(struct bounds_obj));
  *next_end_el = the_return.start_el;
  *next_end_offset = the_return.start_off;
 }
}


int get_el_sgp_rel_pos(int *new_start_el, int sgp_pos, process_book_ptr this_book_ptr) {
 int mid_el, left_el, right_el, rsib_sstart, lsib_send;
 int found=0;
 sgml_obj_ptr current_el, left_sib_el, right_sib_el;
 *new_start_el = 0;
 //Get the middle element in this book
 mid_el = this_book_ptr->no_elements / 2;
 if (!get_next_sgml_obj(&current_el, mid_el, this_book_ptr)) { return 0; }
 left_el = 0;
 right_el = this_book_ptr->no_elements;
 //Now check if we should branch left, right or if we are in the element
 while (!found) {
  //Need to check the ancestor element as well
  if (current_el->lsib!=0) {
   if (!get_next_sgml_obj(&left_sib_el, current_el->number-1, this_book_ptr)) { return 0; }
   lsib_send = left_sib_el->sgrep_end;
  } else {
   lsib_send = 0;
  }
  if (current_el->rsib!=0) {
   if (!get_next_sgml_obj(&right_sib_el, current_el->number+1, this_book_ptr)) { return 0; }
   rsib_sstart = right_sib_el->sgrep_start;
  } else {
   rsib_sstart = 0;
  }
  if (sgp_pos<current_el->sgrep_start && (sgp_pos<=lsib_send || lsib_send==0)) {
   //Branch left
   right_el = current_el->number;
   mid_el = (right_el - left_el) / 2 + left_el;
   if (mid_el==current_el->number) {
    //We going to the same place...
    found = 1;
    continue;
   }
   if (!get_next_sgml_obj(&current_el, mid_el, this_book_ptr)) { return 0; }
  } else if (sgp_pos>current_el->sgrep_end && (sgp_pos>=rsib_sstart || rsib_sstart==0)) {
   //Branch right
   left_el = current_el->number;
   mid_el = (right_el - left_el) / 2 + left_el;
   if (mid_el==current_el->number) {
    //We going to the same place...
    found = 1;
    continue;
   }
   if (!get_next_sgml_obj(&current_el, mid_el, this_book_ptr)) { return 0; }
  } else if (sgp_pos>current_el->sgrep_start && sgp_pos<current_el->sgrep_end) {
   left_el = current_el->number;
   mid_el = (right_el - left_el) / 2 + left_el;
   if (mid_el==current_el->number) {
    //We going to the same place...
    found = 1;
    continue;
   }
   if (!get_next_sgml_obj(&current_el, mid_el, this_book_ptr)) { return 0; }
  } else {
   //In the element
                        if (sgp_pos==current_el->sgrep_end) {
                            current_el = right_sib_el;
                        } else if (current_el->sgrep_start==current_el->sgrep_end && sgp_pos>current_el->sgrep_end) {
                            current_el = right_sib_el;
			} else if (current_el->cont_type==0 && sgp_pos>current_el->sgrep_end) {
			    /* If this element is an empty element and the end is less than the sgp_pos, it's the right sibling. */
			    current_el = right_sib_el;
      }
   found = 1;
  }
 }
 //O.k. here we should be in the element we are searching for
 if (sgp_pos==current_el->sgrep_start && current_el->child!=0) {
  *new_start_el = current_el->child;
 } else {
  *new_start_el = current_el->number;
 }
 return 1;
}

//O.k. this function should take an offset in the sgp file and return the element and offset in this element that the
//position in the sgp file denotes.
//Right one more stab at this using the divide and conquer technique.
int get_sgp_rel_pos(int *new_start_el, int *new_start_offset, int sgp_pos, process_book_ptr process_book_ptr) {
 sgml_obj_ptr current_el;
 //This should return a PCDATA or SDATA
 if (!get_el_sgp_rel_pos(new_start_el, sgp_pos, process_book_ptr)) { return 0; }
 if (!get_next_sgml_obj(&current_el, *new_start_el, process_book_ptr)) { return 0; }
 //Work out the offset from with in this element
 *new_start_el = current_el->number;
 *new_start_offset = sgp_pos-current_el->sgrep_start;
 return 1;
}

//Lets deprecate this function and replace its use with the get_rel_pos function
//we have had several goes at getting this one right...
int get_real_rel_pos(int *new_start_el, int *new_start_offset, process_book_ptr this_book_ptr) {
 sgml_obj_ptr found_el;
 unsigned int offset_now=*new_start_offset;
 int got_it=0;
 if (!get_next_sgml_obj(&found_el, *new_start_el, this_book_ptr)) return false;
 while (offset_now>=0 && !got_it) {
  if (!strcmp(found_el->gi->ptr, "PCDATA") || !strcmp(found_el->gi->ptr, "SDATA")) {
//  if (!strcmp(found_el->gi->ptr, "PCDATA")) {
   if  (offset_now <= (found_el->end_data - found_el->start_data)) 
    got_it=1;
   else offset_now-=(found_el->end_data - found_el->start_data);
  }
  if ((!got_it) && (!get_next_sgml_obj(&found_el, found_el->number+1, this_book_ptr))) return false;
 }
 *new_start_el=found_el->number;
 if (offset_now < 0) {
  offset_now=0;
 }
 *new_start_offset=offset_now;
 return true;
}

//much simpler approach...
/* this_sgml_el  = the current element (is this needed?)
 offset    = offset from the current element that the search starts
 nchars    = number of chars to get
 new_start_el  = returns the element the search stopped in
 new_start_offset = returns the offset in the search element
 this_book   = the current_book
 //If we pass in the start element of the book we can find ANY element..
*/
int get_rel_pos(sgml_obj_ptr this_sgml_el, int offset, int nchars, int *new_start_el, int *new_start_offset, process_book_ptr this_book) {
 sgml_obj_ptr found_el=NULL, rsib_el=NULL, child_el=NULL;
 int found = 0;
 int temp=0;
 int rfound = 0;
 long int pos;
 if (this_sgml_el==NULL) {
  pos = 0 + offset + nchars;
 } else {
  pos = this_sgml_el->start_data + offset + nchars;
 }
 //If we try and read before the document start at the begining
 if (pos<0)
 {
  pos = 0;
 }
 //Start at the top and work our way down
 if (!get_next_sgml_obj(&child_el, 0, this_book)) {
  //Well something went wrong didn't it?
  *new_start_offset = offset;
  *new_start_el = this_sgml_el->number;
  return false;
 }
 while (!found && child_el!=NULL) {
  //Check to see if the offset is within the current range
  if (pos>=child_el->start_data && pos<=child_el->end_data) {
   //Yes so try the first child..
   if (found_el!=NULL) {
    dispose_clone_sgml_obj(&found_el, this_book);
   }
   if (child_el!=NULL) {
    dispose_clone_sgml_obj(&child_el, this_book);
   }
   clone_sgml_obj(child_el, &found_el, this_book, __FILE__, __LINE__);
   if (child_el->child==0 || pos==child_el->start_data || pos==child_el->end_data) {
    //Theses no child so we must be at the bottom of the tree..
    //And therefore found what we are looking for
    if (found_el!=NULL) { dispose_clone_sgml_obj(&found_el, this_book); }
    found_el = child_el;
    found = 1;    
   } else {
    dispose_clone_sgml_obj(&found_el, this_book);
    found_el = NULL;
    temp = child_el->child;
    dispose_clone_sgml_obj(&child_el, this_book);
    if (!get_next_sgml_obj(&child_el, temp, this_book)) {
     //Well something went wrong didn't it?
     *new_start_offset = offset;
     *new_start_el = this_sgml_el->number;
     return false;
    }
    found = 0;
   }
  } else {
   //Nope so loop through the rightSiblings to see what we find..
   if (child_el->rsib==0) {
    //Theres no right sibling... we should never get here
    write_log(apr_psprintf(this_book->current_pool, "Error in get_rel_pos for this_sgml_el %d - child_el %d.\n", (int) this_sgml_el->number, (int) child_el->number));
    found = 1;
    if (found_el!=NULL) { dispose_clone_sgml_obj(&found_el, this_book); }
    found_el = NULL;
    dispose_clone_sgml_obj(&child_el, this_book);
    break;
   }
   temp = child_el->rsib;
   dispose_clone_sgml_obj(&child_el, this_book);
   if (!get_next_sgml_obj(&rsib_el, temp, this_book)) {
    //Well something went wrong didn't it?
    *new_start_offset = offset;
    *new_start_el = this_sgml_el->number;
    return false;
   }
   rfound = 0;
   while (!rfound && rsib_el!=NULL) {
    //Check the range of this element
    if (pos>rsib_el->start_data && pos<rsib_el->end_data) {
     //Found it with in the range so break the loop
     if (child_el!=NULL) { dispose_clone_sgml_obj(&child_el, this_book); }
     child_el = rsib_el;
     rfound = 1;
    } else if (pos==rsib_el->start_data || pos==rsib_el->end_data) {
     if (found_el!=NULL) { dispose_clone_sgml_obj(&found_el, this_book); }
     found_el = rsib_el;
     rfound = 1;
     found = 1;
    } else {
     if (rsib_el->rsib==0) {
     }
     temp = rsib_el->rsib;
     dispose_clone_sgml_obj(&rsib_el, this_book);
     if (!get_next_sgml_obj(&rsib_el, temp, this_book)) {
      //Well something went wrong didn't it?
      *new_start_offset = offset;
      *new_start_el = this_sgml_el->number;
      return false;
     }
     if (rsib_el->number==0) {
      child_el = rsib_el;
      rfound = 1;
     }
    }
   }
  }
 }
 if (found_el==NULL) {
  *new_start_offset = 0;
  *new_start_el = 0;
  if (child_el!=NULL) {
   dispose_clone_sgml_obj(&child_el, this_book);
  }
  if (rsib_el!=NULL) {
   dispose_clone_sgml_obj(&rsib_el, this_book);
  }
  if (found_el!=NULL) {
   dispose_clone_sgml_obj(&found_el, this_book);
  }
  return false;
 } else {
  //O.k. we have the element that we searched for so now to work out the required offset
  *new_start_offset = pos - found_el->start_data;
  *new_start_el = found_el->number;
  if (child_el!=NULL) {
   dispose_clone_sgml_obj(&child_el, this_book);
  }
  if (rsib_el!=NULL) {
   dispose_clone_sgml_obj(&rsib_el, this_book);
  }
  if (found_el!=NULL) {
   dispose_clone_sgml_obj(&found_el, this_book);
  }
  return true;
 }
}

//given a set of bounds and a style file name..go format it and put it in tc_return
//note that we disable is_curr_search in this, to prevent side effects (undesirable outputting of element fragments, etc)
int format_arbitrary_span(char *spec_str, char **tc_return, bounds_ptr bounds, int context, process_book_ptr this_book_ptr, pool *current_pool) {
  int last_el;
  style_ptr format_style;
  int orig_in_search=this_book_ptr->in_search;
  buffer_ptr format_fspec;
  pool *format_pool = 0; apr_pool_create(&format_pool, current_pool);
  this_book_ptr->in_search = 0;
  init_output_buffer(&format_fspec, this_book_ptr, format_pool);
//  write_log("after tcl style 1");
  if (!make_new_tcl_style(spec_str, this_book_ptr->bookname, &format_style, format_fspec, this_book_ptr, false, false)) {
   delete_output_buffer(&format_fspec, this_book_ptr);
            apr_pool_destroy(format_pool);
   this_book_ptr->in_search=orig_in_search;
   return false;
  }
// write_log("after tcl style 2");
   //now, we start.  If it is prev chars, first go up the tree and come back down it...
     if ((context == 0) && (!walk_down_tree(format_style, bounds->start_el, this_book_ptr))) {
   delete_output_buffer(&format_fspec, this_book_ptr);
   apr_pool_destroy(format_pool);
   this_book_ptr->in_search=orig_in_search;
   return false;
  }
  //output it!
  if (!output_element(bounds->start_el, true, true, format_style, &last_el, bounds, this_book_ptr)) {
   delete_output_buffer(&format_fspec, this_book_ptr);
   apr_pool_destroy(format_pool);
   this_book_ptr->in_search=orig_in_search;
   return false;
  }
  if ((context == 2) && (!walk_up_tree(format_style, bounds->end_el, this_book_ptr))) {
   delete_output_buffer(&format_fspec, this_book_ptr);
   apr_pool_destroy(format_pool);
   this_book_ptr->in_search=orig_in_search;
   return false;
  }
  *tc_return=Tcl_Alloc(format_fspec->len);
  if (format_fspec->len==0) {
   (*tc_return)[0]='\0';
  } else {
   strncpy(*tc_return, format_fspec->ptr,format_fspec->len-1);
   (*tc_return)[format_fspec->len-1]='\0';
  }
  this_book_ptr->in_search=orig_in_search;
  delete_output_buffer(&format_fspec, this_book_ptr);
  apr_pool_destroy(format_pool);
  return true;
}

//given an element number: goes up to root and down, formatting every element with 'before' call
int walk_down_tree(style_ptr this_style, int new_start_el, process_book_ptr this_book_ptr) {
 int hold_ids[64]; //if we are 64 levels deep!!!
 int this_el=new_start_el;
 sgml_obj_ptr this_sgml_el;
 int c;
 for (c=0;this_el;c++) {
  hold_ids[c]=this_el;
  if (!get_next_sgml_obj(&this_sgml_el, this_el, this_book_ptr)) return 0;
  this_el=this_sgml_el->ancestor;
 }
 hold_ids[c]=0;
 for ( ;c>0; c--) {  //note we do NOT call start for first el (this would be calling it twice!)
  if (!get_next_sgml_obj(&this_sgml_el, hold_ids[c], this_book_ptr)) return 0;
  if (!output_start(this_sgml_el, this_style, this_book_ptr)) return 0;
 }
 return 1;
}

//given an element number: goes up to root, formatting every element with 'after' call
int walk_up_tree(style_ptr this_style, int new_start_el, process_book_ptr this_book_ptr) {
 sgml_obj_ptr this_sgml_el;
 if (!get_next_sgml_obj(&this_sgml_el,new_start_el, this_book_ptr)) return 0;
 while (this_sgml_el->number) {  //note we do NOT call end for first el (this would be calling it twice!)
  if (!get_next_sgml_obj(&this_sgml_el,this_sgml_el->ancestor, this_book_ptr)) return 0;
  if (!output_end(this_sgml_el->number, this_style, this_book_ptr)) return 0;
 }
 return 1;
}

//These three functions should really check for utf-8 characters..
//Basically once we have the returned character, check if it matches 10xxxxxx,
//if so count back a maximum of 5 characters to find something that matches 11xxxxxx
//this one is used to return where we should start and end from, going back in the file
int calc_prev_chars(struct bounds_obj *bounds, int offset, sgml_obj_ptr this_sgml_el, int nchars, process_book_ptr this_book) {
 int new_start_el, orig_start_el=this_sgml_el->number;
 unsigned int new_start_offset, orig_offset=offset;
 int pos = 0, nchrs = 0;
 unsigned char tchrs[10];
 sgml_obj_ptr found_el;
        memset(tchrs, 0, 10);
 if (offset==-1) {
  offset=(this_sgml_el)->end_data - (this_sgml_el)->start_data;
 }
 if (!get_real_rel_pos(&orig_start_el, (int *) &orig_offset, this_book)) return false;
 if (!get_next_sgml_obj(&this_sgml_el, orig_start_el, this_book)) return false;
 if (!get_rel_pos(this_sgml_el, orig_offset, -nchars, &new_start_el, (int *) &new_start_offset, this_book)) return false;
 if (!get_real_rel_pos(&new_start_el, (int *) &new_start_offset, this_book)) return false;
 //set the bounds
 //or...start el COULD be end of sdata.  If so, skip on to start of next element
 if (!get_next_sgml_obj(&found_el, new_start_el, this_book)) return false;
 if (!strcmp(found_el->gi->ptr, "SDATA") && new_start_offset==(found_el->end_data - found_el->start_data)) {
  new_start_el=found_el->number+1;
  new_start_offset=1;
  if (!get_real_rel_pos(&new_start_el, (int *) &new_start_offset, this_book)) return false;
 }
 if (!get_next_sgml_obj(&this_sgml_el, new_start_el, this_book)) return false;
 pos = this_sgml_el->start_data + new_start_offset - 9;
 if (pos<0) {
  nchrs = -pos;
  pos = 0;
 } else {
  nchrs = 10;
 }
 apr_off_t _where = pos;
 apr_file_seek(this_book->in_sgv, APR_SET, &_where);
 apr_size_t _count = sizeof(char) * nchrs;
 apr_file_read(this_book->in_sgv, tchrs, &_count);
 //Check the first charcter and see if we are in the middle of a character
 if (_count==0 || tchrs[(_count-1)] < 0x80) {
  //Nope so simple return the original bounds
  bounds->start_el=new_start_el;
  bounds->start_off=new_start_offset;
  bounds->end_el=orig_start_el;
  bounds->end_off=orig_offset;//now do it...
  return true;
 }
 //Else we need to loop back and find the start of this character
 //Now loop through backwards and try and find the start of the closest character
 for (pos = (_count-1); pos >= 0; pos--) {
  if (tchrs[pos] > 0xC0 || tchrs[pos] < 0x80) {
   //Found the begining chr of this character
   pos++;
   break;
  }
 }
 if (pos == _count) {
  bounds->start_el=new_start_el;
  bounds->start_off=new_start_offset;
  bounds->end_el=orig_start_el;
  bounds->end_off=orig_offset;//now do it...
  return true;
 } else {
  if (!get_rel_pos(this_sgml_el, 0, (new_start_offset + pos - _count), &new_start_el, (int *) &new_start_offset, this_book)) return false;
  bounds->start_el = new_start_el;
  bounds->start_off = new_start_offset;
  bounds->end_el = orig_start_el;
  bounds->end_off = orig_offset;
  return true;
 }
}

//this one is used to return where we should start and end from, going from point to point in the file
int calc_these_chars(struct bounds_obj *bounds, int offset1, int offset2, sgml_obj_ptr this_sgml_el1, sgml_obj_ptr this_sgml_el2, process_book_ptr this_book_ptr) {
 int start_el=(this_sgml_el1)->number, end_el=(this_sgml_el2)->number;
 int start_offset=offset1, end_offset=offset2;
 int pos = 0, nchrs = 0;
 unsigned char tchrs[10];
 sgml_obj_ptr found_el, this_sgml_el;
        memset(tchrs, 0, 10);
 if (offset1==-1) start_offset=(this_sgml_el1)->end_data- (this_sgml_el1)->start_data;
 if (offset2==-1) end_offset=(this_sgml_el2)->end_data- (this_sgml_el2)->start_data;
 if (offset1==0) 
 {
  if (!get_next_sgml_obj(&found_el, start_el+1, this_book_ptr))
  	start_el = found_el->number;  //mistake here with semi colon in wrong place
 } else {
  if (!get_real_rel_pos(&start_el, &start_offset, this_book_ptr)) return false;
 }
  //could be we are at the end of an sdata; skip forward to next ...
 if (!get_next_sgml_obj(&found_el, start_el, this_book_ptr)) return false;
 if (!strcmp(found_el->gi->ptr, "SDATA") && (unsigned long) start_offset==(found_el->end_data - found_el->start_data)) {
  start_el=found_el->number+1;
  start_offset=0;
  if (!get_real_rel_pos(&start_el, (int *) &start_offset, this_book_ptr)) return false;
 }
 if (!get_real_rel_pos(&end_el, (int *) &end_offset, this_book_ptr)) return false;
 if (!get_next_sgml_obj(&found_el, end_el, this_book_ptr)) return false;
 if (!strcmp(found_el->gi->ptr, "SDATA") && (unsigned long) end_offset==(found_el->end_data - found_el->start_data)) {
  end_el=found_el->number-1;
  end_offset=0;
  if (!get_real_rel_pos(&end_el, (int *) &end_offset, this_book_ptr)) return false;
 }
        //set the bounds
 bounds->start_el=start_el;
 bounds->start_off=start_offset;
        //Check to see if the start bound is in the middle of a character
 //Now lets check for utf8 characters
 if (!get_next_sgml_obj(&this_sgml_el, start_el, this_book_ptr)) return false;
 pos = this_sgml_el->start_data + start_offset - 1;
 nchrs = 10;
 apr_off_t _where = pos;
 apr_file_seek(this_book_ptr->in_sgv, APR_SET, &_where);
 apr_size_t _count = sizeof(char) * nchrs;
 apr_file_read(this_book_ptr->in_sgv, tchrs, &_count);
 //Check the first charcter and see if we are in the middle of a character
 if (_count==0 || tchrs[0] < 0x80) {
  //Nope so simple return the original bounds
 } else {
  //Else we need to loop back and find the start of this character
  //Now loop through backwards and try and find the start of the closest character
  for (pos = (_count-1); pos >= 0; pos--) {
  if (tchrs[pos] > 0xC0 || tchrs[pos] < 0x80) {
   //Found the begining chr of this character
   pos++;
   break;
  }
            }
            if (pos != _count) {
                    if (!get_rel_pos(this_sgml_el, 0, (start_offset + pos -1), &start_el, &start_offset, this_book_ptr)) return false;
                    bounds->start_el = start_el;
                    bounds->start_off = start_offset;
            }
        }
 bounds->end_el=end_el;
 bounds->end_off=end_offset;//now do it...
 if (!get_next_sgml_obj(&this_sgml_el, end_el, this_book_ptr)) return false;
 pos = this_sgml_el->start_data + end_offset - 1;
 nchrs = 10;
 _where = pos;
 apr_file_seek(this_book_ptr->in_sgv, APR_SET, &_where);
        memset(tchrs, 0, 10);
 _count = sizeof(char) * nchrs;
 apr_file_read(this_book_ptr->in_sgv, tchrs, &_count);
 //Check the first charcter and see if we are in the middle of a character
 if (_count==0 || tchrs[0] < 0x80) {
  //Nope so simple return the original bounds
 } else {
            //Else we need to loop back and find the start of this character
            for (pos = 1; pos < _count ; pos++) {
                    if (tchrs[pos] > 0xC0 || tchrs[pos] < 0x80) {
                            //Found the begining of the next chr of this character
                            break;
                    }
            }
            if (pos != _count) {
                    if (!get_rel_pos(this_sgml_el, 0, (end_offset + pos -1), &end_el, &end_offset, this_book_ptr)) return false;
                    bounds->end_el = end_el;
                    bounds->end_off = end_offset;
            }
        }
        return true;
}

//this one is used to return where we should start and end from, going forward in the file
int calc_next_chars(struct bounds_obj *bounds, int offset, sgml_obj_ptr this_sgml_el, int nchars, process_book_ptr this_book) {
 int new_end_el, orig_start_el=this_sgml_el->number;
 int pos = 0, nchrs = 0;
 unsigned char tchrs[10];
 int new_end_offset, orig_offset=offset;
        memset(tchrs, 0, 10);
 if  (offset==-1) {
  orig_offset=offset=(this_sgml_el)->end_data - (this_sgml_el)->start_data;
 }
 if (!get_real_rel_pos(&orig_start_el, &orig_offset, this_book)) return false;
 if (!get_next_sgml_obj(&this_sgml_el, orig_start_el, this_book)) return false;
  //set the bounds .. could be starting at end of sdata..
 if (!strcmp(this_sgml_el->gi->ptr, "SDATA") && offset==(signed) ((this_sgml_el)->end_data - (this_sgml_el)->start_data)) {
  orig_start_el+=1;
  orig_offset=0;
  if (!get_real_rel_pos(&orig_start_el, &orig_offset, this_book)) return false;
 }
 if (!get_rel_pos(this_sgml_el, orig_offset, nchars, &new_end_el, &new_end_offset, this_book)) return false;
 if (!get_real_rel_pos(&new_end_el, &new_end_offset, this_book)) return false;
 //Now lets check for utf8 characters
 if (!get_next_sgml_obj(&this_sgml_el, new_end_el, this_book)) return false;
 pos = this_sgml_el->start_data + new_end_offset - 1;
 nchrs = 10;
 apr_off_t _where = pos;
 apr_file_seek(this_book->in_sgv, APR_SET, &_where);
 apr_size_t _count = sizeof(char) * nchrs;
 apr_file_read(this_book->in_sgv, tchrs, &_count);
 //Check the first charcter and see if we are in the middle of a character
 if (_count==0 || tchrs[0] < 0x80) {
  //Nope so simple return the original bounds
  bounds->start_el=orig_start_el;
  bounds->start_off=orig_offset;
  bounds->end_el=new_end_el;
  bounds->end_off=new_end_offset;//now do it...
  return true;
 }
 //Else we need to loop back and find the start of this character
 //Now loop through backwards and try and find the start of the closest character
 for (pos = 1; pos < _count ; pos++) {
  if (tchrs[pos] > 0xC0 || tchrs[pos] < 0x80) {
   //Found the begining of the next chr of this character
   break;
  }
 }
 if (pos == _count) {
  bounds->start_el=orig_start_el;
  bounds->start_off=orig_offset;
  bounds->end_el=new_end_el;
  bounds->end_off=new_end_offset;//now do it...
  return true;
 } else {
  if (!get_rel_pos(this_sgml_el, 0, (new_end_offset + pos -1), &new_end_el, &new_end_offset, this_book)) return false;
  bounds->start_el = orig_start_el;
  bounds->start_off = orig_offset;
  bounds->end_el = new_end_el;
  bounds->end_off = new_end_offset;
  return true;
 }
 bounds->start_el=orig_start_el;
 bounds->start_off=orig_offset;
 bounds->end_el=new_end_el;
 bounds->end_off=new_end_offset;//now do it...
 return true;
}

//this is a variant form of the above, tuned for make_kwic and for speed
//differs from above in: we already have the style file; we already have buffer content; we don't use tc return
// we don't use context as we always walk up and down the tree before and after
// we also have already stored the style values
int format_kwic_span(bounds_ptr bounds, style_ptr kwic_style, process_book_ptr this_book) {
 int in_search=0;
 int last_el=0;
 if (!walk_down_tree(kwic_style, bounds->start_el, this_book)) return false;
 //output it!
 in_search = this_book->in_search;
 this_book->in_search = 0;
 if (!output_element(bounds->start_el, true, true, kwic_style, &last_el, bounds, this_book)) return false;
 this_book->in_search = in_search;
 if  (!walk_up_tree(kwic_style, bounds->end_el, this_book)) return false;
 return true;
}

void delete_tcl_style(style_ptr this_style, process_book_ptr this_book_ptr) {
 ana_free(&(this_book_ptr->anastasia_memory), this_style); 
}

void send_http_header(request_rec *r, process_book_ptr this_book_ptr)
{
 //O.k.now we have to write out the header information
 //Write out the cookies
 if (r!=NULL && this_book_ptr!=NULL && this_book_ptr->cookies!=NULL)
 {
  apr_table_do(send_cookies, r, this_book_ptr->cookies, NULL);
 }

 //Write out the other headers
 if (r!=NULL && this_book_ptr!=NULL && this_book_ptr->headers!=NULL)
 {
  apr_table_do(send_ana_headers, r, this_book_ptr->headers, NULL);
 }

 //Write out the content type
 if (r!=NULL && this_book_ptr->content_type!=NULL) {
  r->content_type = apr_pstrdup(r->pool, this_book_ptr->content_type);
 } 
 if (r!=NULL) {
//     ap_send_http_header(r);
    }
}

int send_ana_headers(void *rec, const char *key, const char *value)
{
 request_rec *this_rec = rec;
 apr_table_add(this_rec->headers_out, key, value);
 //If the key is a location, we need to send out a 302 response
 if (strcmp(key, "Location")==0)
 {
  this_rec->status = HTTP_MOVED_TEMPORARILY;
 }
 return 1;
}

int send_cookies(void *rec, const char *key, const char *value)
{
 request_rec *this_rec = rec;
 //For the moment try just setting the cookie, ignore the expire time
 apr_table_add(this_rec->headers_out, "Set-Cookie", key);
 return 1;
}
