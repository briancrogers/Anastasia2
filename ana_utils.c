#include "ana_common.h"
#include "ana_utils.h"
#include "ana_browsegrove.h"
#include <apr_strings.h>
#include <apr_file_io.h>
#include <ctype.h>

//extern char ap_server_root[MAX_STRING_LEN];

int set_up_sgrep_index(process_book_ptr this_book_ptr);

//O.k. with the text_obj's well allocate it it's own pool then if we need to
//adjust the size of it well simple create a new pool the right size a delete the old one.
int init_text_obj(text_ptr *t_obj, process_book_ptr this_book_ptr, pool *this_pool, char *file, int line) {
 *t_obj = (struct text_o *) ana_malloc(&(this_book_ptr->anastasia_memory), sizeof(struct text_o), file, line);
 if (*t_obj==NULL) {
  return false;
 }
 (*t_obj)->text_o_pool = NULL;
 (*t_obj)->parent_pool = NULL;
 (*t_obj)->ptr = (char *) ana_malloc(&(this_book_ptr->anastasia_memory), PTRBUFF * sizeof(char), file, line);
 if ((*t_obj)->ptr==NULL) {
  ana_free(&(this_book_ptr->anastasia_memory), *t_obj);
  return false;
 }
 (*t_obj)->allocated = PTRBUFF * sizeof(char);
 (*t_obj)->len = 1;
 (*t_obj)->ptr[0] = '\0';
 return true;
}

int add_str_to_text_obj(text_ptr t_obj, char *add_str, process_book_ptr this_book_ptr, pool *this_pool) {
 unsigned long len_str =0;
 unsigned long a;
 if (add_str!=NULL && strcmp("", add_str)!=0) {
  len_str = strlen(add_str) + t_obj->len;
 } else {
  len_str = t_obj->len;
 }
 if (len_str > t_obj->allocated) {
  a = len_str / PTRBUFF + 1;
  t_obj->ptr = (char *) ana_realloc(&(this_book_ptr->anastasia_memory), (void **) &(t_obj->ptr), t_obj->allocated, PTRBUFF * sizeof(char) * a, __FILE__, __LINE__);
  t_obj->allocated = PTRBUFF * a;
  if (t_obj->ptr==NULL) {
   return false;
  }
 }
 t_obj->len = len_str;
 strcat(t_obj->ptr, add_str);
 return true;
}

void dispose_txt_obj(text_ptr *t_obj, process_book_ptr this_book_ptr) {
 //Does nothing really but kept just incase we ever need to do some clean up
 ana_free(&(this_book_ptr->anastasia_memory), ((*t_obj)->ptr));
 (*t_obj)->len = 0;
 (*t_obj)->parent_pool = NULL;
 (*t_obj)->text_o_pool = NULL;
 ana_free(&(this_book_ptr->anastasia_memory), (*t_obj));
 *t_obj = NULL;
 return;
}

//puts what is in a up to b in c; takes it out of a
int remove_to_token(char *a, char *b, char *c) {
 char *result;
 long len1, len2;
 result = strstr(a, b);
 if (result == NULL) return false;
 len1 = result - a;
 strncpy(c,a,len1);
 len2 = strlen(a) - len1;
 memmove( a, a + len1 + 1, len2);
 c[len1] = '\0';
 return true;
}

int fill_in_nums(int argc, char **argv, int *spec_num1, int *spec_num2, char *ana_cmd, style_ptr this_style_ptr, process_book_ptr this_book) {
 char *end_ptr;
 long int ret_val;
 if (argc==3) return true;
 ret_val=strtoll(argv[3], &end_ptr, 10);
 if (!end_ptr[0]) {
  *spec_num1=ret_val;
 } else {
  char *this_out;
  this_out=apr_psprintf(this_style_ptr->style_pool, "<p><font color=red>Error processing Anastasia style TCL extension function \"%s\" in style file \"%s\" for element \"%s\": the third parameter, \"%s\" is not a number</font></p>\n", argv[0], this_style_ptr->file_name, ana_cmd, argv[3]);
  write_output(this_style_ptr, this_book, this_out);
 }
 if (argc==4) return true;
 ret_val=strtoll(argv[4], &end_ptr, 10);
 if (!end_ptr[0]) {
  *spec_num2=ret_val;
 } else {
  char *this_out;
  this_out=apr_psprintf(this_style_ptr->style_pool, "<p><font color=red>Error processing Anastasia style TCL extension function \"%s\" in style file \"%s\" for element \"%s\": the fourth parameter, \"%s\" is not a number\n</font></p>", argv[0], this_style_ptr->file_name, ana_cmd, argv[4]);
  write_output(this_style_ptr, this_book, this_out);
 }
 return true;
}

//use this to remove \r chars (which foul up printf commands) and convert hex to characters
//used to reformat http strings received from the server
void reform_http_string(char *the_str, char *param_str)
{
 int c=0, d=0, e, f;
 int len_str=strlen(the_str);
 for (c=0, d=0; c<len_str; c++)
  {
   if (the_str[c]=='%')  {
    if (the_str[c+1]<=57) e=the_str[c+1]-48;
     else e=the_str[c+1]-55;
    if (the_str[c+2]<=57) f=the_str[c+2]-48;
     else f=the_str[c+2]-55;
    param_str[d++]=(e*16) + f;
    c+=2;
   }
   else if (the_str[c]=='+') param_str[d++] = ' ';
   else if (the_str[c]=='\r') param_str[d++] = ' ';
   else param_str[d++]=the_str[c];
  }
 param_str[d]='\0';  //cut off final / supplied by IE
}  


void write_log(const char *err_msg) {
#if !defined(ANASTANDALONE)
 ap_log_error(APLOG_MARK, APLOG_NOERRNO|APLOG_NOTICE, 0, anastasia_info.server_rec, "%s", err_msg);
#else
 printf(err_msg);
#endif
 return;
}

//Returns 0 on success, 1 on error and 2 on book unavailable
int create_process_book(process_book_ptr *this_process_book, style_ptr *this_style_ptr, Tcl_Interp *this_interp, char *bookname, char *http_post_args, request_rec *request_rec, pool *current_pool) {
 const char *str, *tmp_str;
 char *tstr, ncstr[1024];
 int a=0;
 process_book_ptr *current_books;
 //O.k. all we need to do is allocate some memory for the book and then set it up.
 //could be call from inside an anv file.  so we need a current pool
 if (!current_pool) {
//  write_log("in create proecess book 1");
  str = Tcl_GetVar(this_interp, "current_pool", TCL_GLOBAL_ONLY);
  if (str!=NULL) {
   current_pool = (pool *) strtoll(str, NULL, 10);
//   write_log(str);
//   write_log(apr_psprintf(anastasia_info.server_pool, "current pool: pointer is %s", str));
  }
 }
 //Need to check to see if we are creating a new book or opening one that already exists
//  write_log("in create proecess book 2");
 
 if (this_interp!=NULL) {
  //So we need to get out the current book, check that against the name being passed in.
        
  str = Tcl_GetVar(this_interp, "this_book_ptr", TCL_GLOBAL_ONLY);
  if (str!=NULL) {
   //*this_process_book = (process_book_ptr) atol(str);
   *this_process_book = (process_book_ptr) strtoll(str, NULL, 10);
//   write_log((*this_process_book)->bookname);
//   write_log(apr_psprintf(anastasia_info-pool, 
  } else {
   *this_process_book = NULL;
   *this_style_ptr = NULL;
   return 1;
  }
 //  write_log("in create proecess book 3");
 
  str =Tcl_GetVar(this_interp, "this_style_ptr", TCL_GLOBAL_ONLY);
  if (str!=NULL) {
   //*this_style_ptr = (style_ptr) atol(str);
   *this_style_ptr = (style_ptr) strtoll(str, NULL, 10);
  } else {
   *this_process_book = NULL;
   *this_style_ptr = NULL;
   return 1;
  }
  	//this seems unneccessary -- so bypassed now
/*  str = Tcl_GetVar(this_interp, "current_books", TCL_GLOBAL_ONLY);
  if (str!=NULL) {
   //current_books = (process_book_ptr *) atol(str);
   current_books = (process_book_ptr *) strtoll(str, NULL, 10);
  } else {
   current_books = NULL;
  }
  //Now get the right book out, or if not create it
  str = Tcl_GetVar(this_interp, "bookname", TCL_GLOBAL_ONLY);
  if (bookname!=NULL) {
   //Use the name of the book passed in
   a = 0;
   *this_process_book = current_books[a++];
   
   while (*this_process_book!=NULL && strcmp((*this_process_book)->bookname, bookname)!=0 && a!=NUMBOOKS) {
    *this_process_book = current_books[a++];
   }
  } else if (str!=NULL) {
   //Else use the bookname from the interp.
   //I don't see the point of the current_books variable. I guess this might come from the day when we refer to multiple books...
   a=0;
//   write_log("in create proecess book 5");
   bookname = apr_pstrdup(current_pool, str);
//   write_log(bookname);
//    write_log("in create proecess book 6");
   *this_process_book = current_books[a++];
//   write_log("in create proecess book 6a");
   while (*this_process_book!=NULL && strcmp((*this_process_book)->bookname, str)!=0 && a!=NUMBOOKS) {
//    write_log("in create proecess book 6b");
    *this_process_book = current_books[a++];
   }
  } else {
   //No book set so use the current one
   return 0;
  } */
//  write_log("in create proecess book 7");
  str = Tcl_GetVar(this_interp, "bookname", TCL_GLOBAL_ONLY);
  if (*this_process_book!=NULL && strcmp((*this_process_book)->bookname, str)==0) {
   //Found the book!
//   write_log("in create proecess book 8");
   return 0;
  }
  //O.k. now we need to create the book.
 } else {
  current_books = NULL;
 }
//  write_log("in create proecess book 9");
  (*this_process_book) = (process_book_ptr) apr_pcalloc(current_pool, sizeof(struct process_book_inf));
//  write_log("in create proecess book 10");
  if ((*this_process_book)==NULL) {
  return 1;
 }
 (*this_process_book)->bookname = apr_pstrdup(current_pool, bookname);
 (*this_process_book)->book_interp = Tcl_CreateInterp();
 Tcl_Preserve((*this_process_book)->book_interp);
 Tcl_FindExecutable(NULL);
#if defined(ANA_MYSQL)
 mysql_library_init(0, NULL, NULL);
 mysql_init(&(*this_process_book)->database);
#endif
 //Load in the global config file
//  write_log("in create proecess book 6-4");
 load_in_global_config_file((*this_process_book)->book_interp, current_pool, 0);
//   write_log("in create proecess book 6-5");
 tstr = apr_psprintf(current_pool, "books(%s)", bookname);
//   write_log("in create proecess book 6-6");
 str = Tcl_GetVar((*this_process_book)->book_interp, tstr, 0);
 if (str==NULL) {
  *this_process_book = NULL;
  return 1;
 }
 (*this_process_book)->dir = apr_pstrdup(current_pool, str);
 str = ap_make_full_path(current_pool, (*this_process_book)->dir, (*this_process_book)->bookname);
 strcpy(ncstr, str);
 ap_no2slash(ncstr);
 (*this_process_book)->fs_idx =  apr_psprintf(current_pool, "%s.idx", ncstr);
 (*this_process_book)->fs_ngv =  apr_psprintf(current_pool, "%s.ngv", ncstr);
 (*this_process_book)->fs_rgv =  apr_psprintf(current_pool, "%s.rgv", ncstr);
 (*this_process_book)->fs_sgv =  apr_psprintf(current_pool, "%s.sgv", ncstr);
 (*this_process_book)->in_search = 0;
 (*this_process_book)->hide = 0;
 (*this_process_book)->http_post_args = apr_pstrdup(current_pool, http_post_args);
 //Now lets open up the files
 if (!ana_stream_open((*this_process_book)->fs_idx, &(*this_process_book)->in_idx, APR_FOPEN_READ|APR_FOPEN_BINARY, current_pool)) {
  this_process_book = NULL;
  return 1;
 }
 if (!ana_stream_open((*this_process_book)->fs_ngv, &(*this_process_book)->in_ngv, APR_FOPEN_READ|APR_FOPEN_BINARY, current_pool)) {
  this_process_book = NULL;
  return 1;
 }
 if (!ana_stream_open((*this_process_book)->fs_rgv, &(*this_process_book)->in_rgv, APR_FOPEN_READ|APR_FOPEN_BINARY, current_pool)) {
  this_process_book = NULL;
  return 1;
 }
 if (!ana_stream_open((*this_process_book)->fs_sgv, &(*this_process_book)->in_sgv, APR_FOPEN_READ|APR_FOPEN_BINARY, current_pool)) {
  this_process_book = NULL;
  return 1;
 }
 //Work out how many elements are in this book
//   write_log("starting point 6-6");
 apr_off_t _where = 0;
 apr_file_seek((*this_process_book)->in_rgv, APR_END, &_where);
 (*this_process_book)->rgv_size = _where;
 (*this_process_book)->no_elements = ((*this_process_book)->rgv_size-4)/16;
 //Finally lets allocate a 2MB memory pool for this book
  if (!init_ana_memory(&(*this_process_book)->anastasia_memory, current_pool, 26214400)) {
// if (!init_ana_memory(&(*this_process_book)->anastasia_memory, current_pool, 20971520)) {
  this_process_book = NULL;
  return 1;
 }
 (*this_process_book)->anastasia_memory.this_book = (void *) *this_process_book;
 //Setup the index objects
 //   write_log("starting point 6-7");
 (*this_process_book)->sgml_objects=init_hash_lilo(1021, 500*1024, create_sgml_obj, delete_sgml_obj, (*this_process_book), current_pool);
//	write_log("starting point 6-8");
  //create the hash lilo for index objects
 (*this_process_book)->index_objects=init_hash_lilo(1021, 500*1024, create_index_obj, delete_index_obj, (*this_process_book), current_pool);
//	write_log("starting point 6-9");
  if ((*this_process_book)->sgml_objects==NULL || (*this_process_book)->index_objects==NULL) {
  this_process_book = NULL;
  return 1;
 }
 (*this_process_book)->search_els_found=ana_malloc(&(*this_process_book)->anastasia_memory, MAXSEARCHRESULTS*sizeof(int), __FILE__, __LINE__);
 (*this_process_book)->search_texts_found=ana_malloc(&(*this_process_book)->anastasia_memory, MAXSEARCHRESULTS*sizeof(struct bounds_obj), __FILE__, __LINE__);
// write_log("starting point 6-10");
 //Right if request request_rec is null it means we are getting the content of another book from inside the main book
 //How do we tell the main book? 
 (*this_process_book)->request_rec = request_rec;
 (*this_process_book)->current_pool = current_pool;
// write_log("starting point 6-11");
 set_up_sgrep_index((*this_process_book));
 (*this_process_book)->starttime = time(NULL);
//  write_log("starting point 6-12");
 //Add the book to the array.
 if (current_books!=NULL) {
  a=0;
  while(current_books[a]!=NULL) { a++; }
  //Should have found the end of the array
  if (a!=NUMBOOKS) { current_books[a]=(*this_process_book); }
 }
 sprintf(ncstr, "%lu", (ulint) current_books);
 Tcl_SetVar((*this_process_book)->book_interp, "current_books", ncstr, 0);
 Tcl_SetVar((*this_process_book)->book_interp, "bookname", (*this_process_book)->bookname, 0);
 tmp_str = apr_psprintf(current_pool, "books(%s)", (*this_process_book)->bookname);
 Tcl_SetVar((*this_process_book)->book_interp, tmp_str, (*this_process_book)->dir, 0);
 str = ap_make_full_path(current_pool, (*this_process_book)->dir, "AnaRead.cfg");
 strcpy(ncstr, str);
//  write_log("starting point 6-13");
 
 ap_no2slash(ncstr);
 a = Tcl_Eval((*this_process_book)->book_interp, apr_psprintf(current_pool, "file exists {%s}", ncstr));
 tmp_str = Tcl_GetStringResult((*this_process_book)->book_interp);
 if (strcmp(tmp_str, "1")==0) {
  a = Tcl_EvalFile((*this_process_book)->book_interp, ncstr);
 }
 (*this_process_book)->cookies = apr_table_make(current_pool, 10);
 (*this_process_book)->headers = apr_table_make(current_pool, 10);
 return 0;
}

void close_ana_book(process_book_ptr this_book, process_book_ptr *current_books, int error) {
 //But not a big deal as apache will close these handles itself later.
 ana_free(&(this_book->anastasia_memory), this_book->lomem_idx->file_list);
 ana_free(&(this_book->anastasia_memory), this_book->lomem_idx);
 ana_free(&(this_book->anastasia_memory), this_book->map_idx);
 ana_free(&(this_book->anastasia_memory), current_books);
 ana_free(&(this_book->anastasia_memory), this_book->search_els_found);
 ana_free(&(this_book->anastasia_memory), this_book->search_texts_found);
 free_ana_memory(&(this_book->anastasia_memory));
#if defined(ANA_MYSQL)
 if (this_book->database.host!=NULL) {
  mysql_close(&this_book->database);
  mysql_library_end();
 }
#endif
 clear_hash_lilo(this_book);
 //Remove the interp
 Tcl_Release(this_book->book_interp);
 Tcl_DeleteInterp(this_book->book_interp);
 return;
}

void close_all_books(process_book_ptr *current_books) {
 int a=0, b=0;
 while (a!=NUMBOOKS && current_books[a]!=NULL && *current_books!=NULL) {
  //Now delete all the interperters and free all the pools for the styles in this book
  b=0;
  while (b!=NUMSTYLES && current_books[a]->styles[b]!=NULL) {
   Tcl_Release(current_books[a]->styles[b]->tcl_interp);
   Tcl_DeleteInterp(current_books[a]->styles[b]->tcl_interp);
   ana_free(&(current_books[a]->anastasia_memory), current_books[a]->styles[b]->file_name);
   ana_free(&(current_books[a]->anastasia_memory), current_books[a]->styles[b]->book_name);
   apr_pool_destroy(current_books[a]->styles[b]->style_pool);
   ana_free(&(current_books[a]->anastasia_memory), current_books[a]->styles[b]);
   current_books[a]->styles[b]=NULL;
   b++;
  }
  close_ana_book(current_books[a], current_books, 0);
  current_books[a]=NULL;
  a++;
 }
}

//opens a file in the current pool so we do not need to close it
int ana_stream_open(char *this_file, apr_file_t **stream, apr_int32_t flags, pool *current_p) {
  char messg[256];
  if (APR_SUCCESS != apr_file_open(stream, this_file, flags, APR_OS_DEFAULT, current_p)) {
   sprintf(messg, "Unable to make the file %s for reading", this_file);
   write_log(messg);
                        switch (errno) {
                            case ENOENT:
                                write_log("ERROR!: File not found.");
                                break;
                            case EACCES:
                                write_log("ERROR!: Permission denied.");
                                break;
                            default:
                                sprintf(messg, "ERROR!: Unknown error code %d.", errno);
                                write_log(messg);
                                break;
                        }
   return false;
        }
 return true;
}

hash_lilo_obj_ptr init_hash_lilo(int htable_size,  int capacity, void *init_callback, void *dispos_callback, process_book_ptr this_book_ptr, pool *current_pool) {
 pool *lilo_pool = 0; apr_pool_create(&lilo_pool, current_pool);
 hash_lilo_obj_ptr hash_lilo= (hash_lilo_obj_ptr) ana_malloc(&(this_book_ptr->anastasia_memory), sizeof(struct hash_lilo_obj_inf), __FILE__, __LINE__);
 memset(hash_lilo, 0, sizeof(struct hash_lilo_obj_inf));
 if (!hash_lilo) return NULL;
 hash_lilo->capacity=capacity;
  hash_lilo->htable= (struct hash_lilo_item_inf **) ana_malloc(&(this_book_ptr->anastasia_memory), htable_size*sizeof(struct hash_lilo_item_inf), __FILE__, __LINE__);
 if (!hash_lilo->htable) {
  return NULL;
 }
 memset(hash_lilo->htable, 0, htable_size*sizeof(struct hash_lilo_item_inf));
 hash_lilo->callback_init=init_callback;
 hash_lilo->callback_delete=dispos_callback;
 hash_lilo->number=0;
 hash_lilo->current=0;
 hash_lilo->htable_size=htable_size;
 hash_lilo->object_pool=lilo_pool;
 return(hash_lilo);
}

sgml_obj_ptr create_sgml_obj(void *this_book, int *allocated, int *sought_num, process_book_ptr this_book_ptr) {
 process_book_ptr this_process_book = (process_book_ptr) this_book;
 sgml_obj_ptr hash_lilo_el=NULL;
 attr_ptr this_attr;
 char *str;
 pool *sgml_pool;
 int in_num;
// write_log(apr_psprintf(this_process_book->current_pool, "create_sgml_obj utils.c 435 making new element 5, %d", *sought_num));
 //Hmm create subpool for this..so when we delete the sgml object we can just delete the pool
 sgml_pool = NULL;
 if (!init_sgml_obj(&hash_lilo_el, (struct process_book_inf *) this_book, sgml_pool, __FILE__, __LINE__)) return false;
//  write_log("making new element 6");

 //have to guesstimate how much space might be needed by strings, attributes, etc
 *allocated=sizeof(struct sgml_obj);
//   write_log("making new element 7");

  //get offset to this element from the rgv file
  //mathematics are: element offset in rgv file is 16 * element number for start data
  //check this is the right number!! if not, something very nasty has occurred
  //this reads in the number, content type, ancestor, etc etc
  //we need to add better failsafe routines here: stop trying to read if this is bad number
//    write_log("making new element 8");
  if (!read_num(&in_num, this_process_book->in_rgv, 16 * (*sought_num))) {
   write_log(apr_psprintf(this_process_book->current_pool, "Error reading Anastasia .rgv file for element number %d.\n", (*sought_num)));
    hash_lilo_el=NULL;
   return NULL;
  }
//  write_log("making new element 9");
  if (in_num!=*sought_num) {
   str = apr_psprintf(this_process_book->current_pool, "Error reading Anastasia .rgv file: seeking element %lu, found %lu instead.  This file may be corrupt.\n", (long unsigned int) *sought_num, (long unsigned int) in_num);
   write_log(str);
   hash_lilo_el=NULL;
   return NULL;
  }
//    write_log("making new element 10");
  if (!read_num(&in_num, this_process_book->in_rgv, (16 * (*sought_num)) + 4)) {
   write_log(apr_psprintf(this_process_book->current_pool, "Error reading Anastasia .rgv file for element number %d.\n", (*sought_num)));
   hash_lilo_el=NULL;
   return NULL;
  }
  if (!read_num(&hash_lilo_el->start_data, this_process_book->in_rgv, (16 * (*sought_num)) + 8)) {
   write_log(apr_psprintf(this_process_book->current_pool, "Error reading Anastasia .rgv file for element number %d.\n", (*sought_num)));
   hash_lilo_el=NULL;
   return NULL;
  }
  if (!read_num(&hash_lilo_el->end_data, this_process_book->in_rgv, (16 * (*sought_num)) + 12)) {
   write_log(apr_psprintf(this_process_book->current_pool, "Error reading Anastasia .rgv file for element number %d.\n", (*sought_num)));
   hash_lilo_el=NULL;
   return NULL;
  }
  //get the data offsets too while we are about it
  //got the offset.  Read the element in now
  //note that this could reshuffle the sgml_objs array, as we re-find the ancestors to the current element
  //this 
//    write_log("making new element 11");
  if (!read_this_el(in_num, this_process_book->in_ngv, this_process_book->in_rgv, hash_lilo_el, this_process_book)) {
   write_log(apr_psprintf(this_process_book->current_pool, "Error reading Anastasia .rgv file for element number %d.\n", (*sought_num)));
   hash_lilo_el=NULL;
   return NULL;
  }
 //always return first element in array
 //actually, no...coz we will reshuffle the array in building the ancestor list
 //now add all the objectsizes
// write_log("making new element 12");
 *allocated+=hash_lilo_el->gi->len;
 *allocated+=hash_lilo_el->path->len;
 this_attr = hash_lilo_el->attrs;
 while (this_attr!=0) {
  *allocated+=this_attr->name->len;
  *allocated+=this_attr->value->len;
  this_attr = this_attr->nattr;
 }
//    write_log("making new element 13");
 hash_lilo_el->is_clone=0;
 return hash_lilo_el;
}

//with apache we don't bother: just kill the subpool.
void delete_sgml_obj(void *this_sgml_obj, int *reclaimed, int *sought, process_book_ptr this_book) {
 attr_ptr this_attr = 0, next_attr=0;
 sgml_obj_ptr delete_obj=(sgml_obj_ptr) this_sgml_obj;
 *reclaimed+=delete_obj->gi->len;
 dispose_txt_obj(&(delete_obj->gi), this_book);
 delete_obj->gi=NULL;
 *reclaimed+=delete_obj->path->len;
 dispose_txt_obj(&(delete_obj->path), this_book);
 delete_obj->path=NULL;
 dispose_txt_obj(&(delete_obj->pcdata), this_book);
 delete_obj->pcdata=NULL;
 dispose_txt_obj(&(delete_obj->bookname), this_book);
 delete_obj->bookname=NULL;
 //O.k. need to delete all the attributes
 this_attr = delete_obj->attrs;
 while (this_attr!=0 && this_attr->nattr!=0) {
  this_attr = this_attr->nattr;
 }
 while (this_attr!=0) {
  *reclaimed+=this_attr->name->len;
  dispose_txt_obj(&(this_attr->name), this_book);
  this_attr->name=NULL;
  *reclaimed+=this_attr->value->len;
  dispose_txt_obj(&(this_attr->value), this_book);
  this_attr->value=NULL;
  //Need to free the attribute struct
  next_attr = this_attr->pattr;
  ana_free(&(this_book->anastasia_memory), (this_attr));
  this_attr->nattr = 0;
  this_attr->pattr = 0;
  this_attr = next_attr;
  if (next_attr!=0) {
   this_attr->nattr = 0;
  }
 }
 *reclaimed+=sizeof(struct sgml_obj);
 //And finally destory the pool
        ana_free(&(this_book->anastasia_memory), delete_obj);
}

void *create_index_obj(void *this_book, int *allocated, int *sought_num, process_book_ptr this_book_ptr) {
 process_book_ptr this_book_process = (process_book_ptr) this_book;
  unsigned char test_rd[16];
 //Create subpool for this indexfilereadptr
 pool *index_pool = NULL;
 IndexFileReadPtr the_read = (struct IndexFileRead *) ana_malloc(&(this_book_process->anastasia_memory), sizeof(struct IndexFileRead), __FILE__, __LINE__);
 if (!the_read) {
  write_log("Error reading Anastasia index object: apparently, insufficient memory.\n");
  return NULL;
 }
 *allocated+=sizeof(struct IndexFileRead);
 the_read->the_str = (unsigned char *) ana_malloc(&(this_book_process->anastasia_memory), 256*sizeof(char), __FILE__, __LINE__);
 if (!the_read->the_str) {
 // write_log("Error reading Anastasia index object: apparently, insufficient memory.\n");
  return NULL;
 }
 *allocated+=256;
 the_read->allocated=256;
 the_read->all_read=0;
 //test with this for now
 //get the number out of the file
 apr_off_t _where = this_book_process->lomem_idx->array_offset+(sizeof(int)**sought_num);
 apr_file_seek(this_book_process->in_idx, APR_SET, &_where);
 apr_size_t _count = sizeof(int);
 apr_file_read(this_book_process->in_idx, (void *)test_rd, &_count);
 the_read->place=get_int(test_rd,0);
 _where = this_book_process->lomem_idx->entries_offset+the_read->place;
 apr_file_seek(this_book_process->in_idx, APR_SET, &_where);
 _count = 256 * sizeof(char);
 apr_file_read(this_book_process->in_idx, (void *)the_read->the_str, &_count);
 the_read->index_pool = index_pool;
 //Now set the lock information to the default
 return(the_read);
 //set read to right bit of the file and away we go
}

/*in apache -- none of the deletion does anything; the object goes when we destroy the pool */
void delete_index_obj(void *this_index_obj, int *reclaimed, int *sought, process_book_ptr this_book) {
 IndexFileReadPtr delete_obj=(IndexFileReadPtr) this_index_obj;
 *reclaimed+=delete_obj->allocated;
 *reclaimed+=sizeof(struct IndexFileRead);
 if (delete_obj->the_str!=NULL) {
  ana_free(&(this_book->anastasia_memory), delete_obj->the_str);
 }
        ana_free(&(this_book->anastasia_memory), delete_obj);
}

int clone_sgml_obj(sgml_obj_ptr this_sgml_el,sgml_obj_ptr *pc_clone, process_book_ptr this_book_ptr, char *file, int line) {
 //Make a subpool for it?
 unsigned int b;
 pool *clone_sgml_pool;
 attr_ptr *this_attr, orig_attr;
 clone_sgml_pool = NULL;
    if (!init_sgml_obj(pc_clone, this_book_ptr, clone_sgml_pool, file, line)) return false;
 (*pc_clone)->cont_type=this_sgml_el->cont_type;
    (*pc_clone)->ancestor=this_sgml_el->ancestor;
    (*pc_clone)->number=this_sgml_el->number;
    (*pc_clone)->lsib=this_sgml_el->lsib;
    (*pc_clone)->rsib=this_sgml_el->rsib;
    (*pc_clone)->child=this_sgml_el->child;
 (*pc_clone)->sgrep_start=this_sgml_el->sgrep_start;
 (*pc_clone)->sgrep_end=this_sgml_el->sgrep_end;
 (*pc_clone)->object_pool = clone_sgml_pool;
 (*pc_clone)->n_attrs = this_sgml_el->n_attrs;
 (*pc_clone)->start_data = this_sgml_el->start_data;
 (*pc_clone)->end_data = this_sgml_el->end_data;
 (*pc_clone)->sgrep_start = this_sgml_el->sgrep_start;
 (*pc_clone)->sgrep_end = this_sgml_el->sgrep_end;
 (*pc_clone)->is_clone = 1;
    if (!add_str_to_text_obj((*pc_clone)->path, this_sgml_el->path->ptr, this_book_ptr, clone_sgml_pool)) return false;
 if (!add_str_to_text_obj((*pc_clone)->gi, this_sgml_el->gi->ptr, this_book_ptr, clone_sgml_pool)) return false;
 if (!add_str_to_text_obj((*pc_clone)->pcdata, this_sgml_el->pcdata->ptr, this_book_ptr, clone_sgml_pool)) return false;
 b = (*pc_clone)->n_attrs;
 if (!strcmp((*pc_clone)->gi->ptr, "SDATA")) {
  b = 1;
 }
 this_attr = &(*pc_clone)->attrs;
        orig_attr = this_sgml_el->attrs;
        while (orig_attr!=NULL) {
  if (!init_attr_obj(this_attr, this_book_ptr, clone_sgml_pool)) return false;
  if (!add_str_to_text_obj((*this_attr)->name, orig_attr->name->ptr, this_book_ptr, clone_sgml_pool)) return false;
  if (!add_str_to_text_obj((*this_attr)->value, orig_attr->value->ptr, this_book_ptr, clone_sgml_pool)) return false;
  this_attr = &((*this_attr)->nattr);
                orig_attr = orig_attr->nattr;
 }
    return true;
}

int clone_index_obj(IndexFileReadPtr this_index, IndexFileReadPtr *index_clone, process_book_ptr this_book_ptr, pool *this_pool) {
 pool *clone_index_pool = NULL;
 *index_clone= (struct IndexFileRead *) ana_malloc(&(this_book_ptr)->anastasia_memory, sizeof(struct IndexFileRead), __FILE__, __LINE__);
 if (*index_clone==NULL) {
  return false;
 }
 (*index_clone)->the_str = (unsigned char *) ana_malloc(&(this_book_ptr->anastasia_memory), (sizeof(char) * this_index->allocated), __FILE__, __LINE__);
 if ((*index_clone)->the_str==NULL) {
  ana_free(&(this_book_ptr->anastasia_memory), *index_clone);
  return false;
 }
 memcpy((*index_clone)->the_str, this_index->the_str, this_index->allocated);
 (*index_clone)->all_read = this_index->all_read;
 (*index_clone)->place = this_index->place;
 (*index_clone)->allocated = this_index->allocated;
 (*index_clone)->index_pool = clone_index_pool;
 return true;
}

int dispose_clone_index_obj(IndexFileReadPtr this_index, process_book_ptr this_book_ptr) {
 ana_free(&(this_book_ptr->anastasia_memory), this_index->the_str);
 ana_free(&(this_book_ptr->anastasia_memory), this_index);
 this_index=NULL;
 return true;
}

int dispose_clone_sgml_obj(sgml_obj_ptr *this_sgml_el, process_book_ptr this_process_book) {
 //Simply destory the subpool
 attr_ptr this_attr = 0, next_attr = 0;
 if (this_sgml_el==NULL || *this_sgml_el==NULL) return false;
 if ((*this_sgml_el)->is_clone==0) {
  return true;
 }
 dispose_txt_obj(&((*this_sgml_el)->gi), this_process_book);
 (*this_sgml_el)->gi=NULL;
 dispose_txt_obj(&((*this_sgml_el)->path), this_process_book);
 (*this_sgml_el)->path=NULL;
 dispose_txt_obj(&((*this_sgml_el)->pcdata), this_process_book);
 (*this_sgml_el)->pcdata=NULL;
 dispose_txt_obj(&((*this_sgml_el)->bookname), this_process_book);
 (*this_sgml_el)->bookname=NULL;
 this_attr = (*this_sgml_el)->attrs;
 while (this_attr!=0) {
                next_attr = this_attr->nattr;
//  if (&this_attr->name!=NULL) { Always true!
  if (strcmp((const char *) this_attr->name, "")!=0) {
   dispose_txt_obj((&this_attr->name), this_process_book);
  }
  this_attr->name=NULL;
//  if (&this_attr->value!=NULL) {
  if (strcmp((const char *) this_attr->value, "")!=0) {
   dispose_txt_obj(&(this_attr->value), this_process_book);
  }
  this_attr->value=NULL;
  ana_free(&(this_process_book->anastasia_memory), (this_attr));
  this_attr = next_attr;
 }
 //And finally destory the pool
  ana_free(&(this_process_book->anastasia_memory), *this_sgml_el);
 this_sgml_el=NULL;
    return true;
}

int init_sgml_obj(sgml_obj_ptr *this_sgml_el,  process_book_ptr this_book, pool *this_pool, char *file, int line) { 
 (*this_sgml_el) = (sgml_obj_ptr) ana_malloc(&(this_book->anastasia_memory), sizeof(struct sgml_obj), __FILE__, __LINE__);
 if (*this_sgml_el==NULL) {
  return false;
 }
 (*this_sgml_el)->object_pool=NULL;
 //Initiate gi name, path
 if (!init_text_obj(&(*this_sgml_el)->gi, this_book, (*this_sgml_el)->object_pool, file, line)) return false;
 if (!init_text_obj(&(*this_sgml_el)->path, this_book, (*this_sgml_el)->object_pool, file, line)) return false;
 if (!init_text_obj(&(*this_sgml_el)->bookname, this_book, (*this_sgml_el)->object_pool, file, line)) return false;
 if (!init_text_obj(&(*this_sgml_el)->pcdata, this_book, (*this_sgml_el)->object_pool, file, line)) return false;
 (*this_sgml_el)->start_data=0;
 (*this_sgml_el)->end_data=0;
 (*this_sgml_el)->hashptr = NULL;
 return true;
}

//reads a number in a platform independent manner
//always use place to set where we are reading from
//if we try to read past the end of the file this causes nasty problems
//so: test that place is not > file size less 4 bytes
int read_num(int *this_no, apr_file_t *in_f, int place) {
 size_t f_size = 0;
 if (in_f==NULL) {
  write_log("Could not open file for reading this is a serious error!.");
  return false;
 } else {
  apr_off_t _where = 0;
  apr_file_seek(in_f, APR_END, &_where);
  f_size=_where;
  if (place> f_size-4) return false;
  _where = place;
  apr_file_seek(in_f, APR_SET, &_where);
  apr_size_t _count = sizeof(int);
  apr_file_read(in_f, this_no, &_count);
  *this_no = upend(*this_no);
  if (_count != sizeof(int)) {
   return false;
  } else {
   return true;
  }
 }
}

int read_this_el(int place, apr_file_t *src_ngv, apr_file_t *src_rgv,  sgml_obj_ptr this_sgml_el, process_book_ptr this_book) {
 attr_ptr *this_attr, prev_attr;
 //replace by something better later ... no bigendian conversion here
 apr_off_t _where = place;
 apr_file_seek(src_ngv, APR_SET, &_where);
 //read in eight numbers, including numbers for sgrep end data
 //ints are FOUR bytes...
// write_log(apr_psprintf(anastasia_info.server_pool, "reading element utils 749 number %d", place));
 apr_size_t _count = sizeof(int) * 8;
 apr_file_read(src_ngv, &this_sgml_el->number, &_count);
 this_sgml_el->number = upend(this_sgml_el->number);
 this_sgml_el->ancestor = upend(this_sgml_el->ancestor);
 this_sgml_el->lsib = upend(this_sgml_el->lsib);
// write_log("reading element 2");
 this_sgml_el->rsib = upend(this_sgml_el->rsib);
 this_sgml_el->child = upend(this_sgml_el->child);
 this_sgml_el->cont_type = upend(this_sgml_el->cont_type);
 this_sgml_el->sgrep_start = upend(this_sgml_el->sgrep_start);
 this_sgml_el->sgrep_end = upend(this_sgml_el->sgrep_end);
 
 if (_count != (sizeof(int) * 8)) {
  write_log("Error reading Anastasia .ngv file.\n");
  return false;
 }
 if (!read_num_str(this_sgml_el->gi, src_ngv, this_book, this_sgml_el->object_pool)) {
  write_log("Error reading Anastasia .ngv file.\n");
  return false;
 }
 //set content type to sdata and pcdata (in future: we should revise groveMaker so no need to reset-- some speed advantage
 //because tcl can't process # characters: rename as SDATA and PCDATA
 if (!strncmp(this_sgml_el->gi->ptr,"#SDATA", this_sgml_el->gi->len)
  || !strncmp(this_sgml_el->gi->ptr,"#PCDATA", this_sgml_el->gi->len)) {
  this_sgml_el->n_attrs=0;
  if (!strncmp(this_sgml_el->gi->ptr,"#SDATA", this_sgml_el->gi->len)) {
   this_sgml_el->cont_type=5;
   if (!empty_text_obj(&(this_sgml_el->gi), this_book, this_sgml_el->object_pool)) return false;
   if (!add_str_to_text_obj(this_sgml_el->gi,"SDATA", this_book, this_sgml_el->object_pool)) return false;
  } 
  if (!strncmp(this_sgml_el->gi->ptr,"#PCDATA", this_sgml_el->gi->len)) {
   this_sgml_el->cont_type=6;
   if (!empty_text_obj(&(this_sgml_el->gi), this_book, this_sgml_el->object_pool)) return false;
   if (!add_str_to_text_obj(this_sgml_el->gi,"PCDATA", this_book, this_sgml_el->object_pool)) return false;
  }
 }
 else {
  _where = 0;
  apr_file_seek(src_ngv, APR_CUR, &_where);
  if (!read_num(&(this_sgml_el)->n_attrs, src_ngv, _where)) {
    write_log("Error reading Anastasia .ngv file.\n");
    return false;
  }
 }
 //we put sdata into the attribute value string, sdata name into sdata name string
 if (!strncmp(this_sgml_el->gi->ptr,"SDATA", this_sgml_el->gi->len)) {
  if (!init_attr_obj(&(this_sgml_el)->attrs, this_book, this_sgml_el->object_pool)) return false;
  if (!read_num_str((this_sgml_el)->attrs->value, src_ngv, this_book, this_sgml_el->object_pool)) {
   write_log("Error reading Anastasia .ngv file.\n");
   return false;
  }
  if (!read_num_str((this_sgml_el)->attrs->name, src_ngv, this_book, this_sgml_el->object_pool)) {
   write_log("Error reading Anastasia .ngv file.\n");
   return false;
  }
 }
 if (this_sgml_el->n_attrs>0)  {
  unsigned int c;
  this_attr = &this_sgml_el->attrs;
  prev_attr = 0;
  for (c=0; c<this_sgml_el->n_attrs; c++) {
   if (!init_attr_obj(this_attr, this_book, this_sgml_el->object_pool)) return false;
   if (!read_num_str((*this_attr)->name, src_ngv, this_book, this_sgml_el->object_pool)) {
    write_log("Error reading Anastasia .ngv file.\n");
    return false;
   }
   if (!read_num_str((*this_attr)->value, src_ngv, this_book, this_sgml_el->object_pool)) {
    write_log("Error reading Anastasia .ngv file.\n");
    return false;
   }
   (*this_attr)->pattr = prev_attr;
   prev_attr = *this_attr;
   this_attr = &((*(*this_attr)).nattr);
  }
 }
 //this one is misbehaving -- I think it is leading us into deep recursivity which we really don't need..
 if (!make_element_path(this_sgml_el, this_book)) {
  return false;
 } 
 return true;
}


int write_output(style_ptr this_style, process_book_ptr this_book_ptr, char *this_str) {
 /* if nothing at all to write out..write out nothing! */
 if (this_style->file_spec!=NULL && this_style->file_spec->ptr!=NULL && this_style->rec==NULL) {
  //Hmm lets just use apache string functions for this..
  //Should speed up large pages tremebously
  add_str_to_buf(this_style, this_str, this_book_ptr);
 } else {
#if !defined(ANASTANDALONE)
  ap_rputs(this_str, this_style->rec);
#else
                printf(this_str);
#endif
  this_style->file_spec->len += strlen(this_str);
 }
 return true;
}

/* add string to handle: increases size of handle, returns new size */
/* routine for only increasing hndlsize if necessary*/
int add_str_to_hndl(char **hndl, char *str,long *hndl_size, pool *this_pool) {
 long len_str = strlen(str);
 int mem_blocks_now, mem_blocks_new;
 //after much fiddling:  this is incremented in 128 char lots
 //original handle is set to be just big enough.  This is incremented as we need
 mem_blocks_now=(*hndl_size/ANA_HNDL_BLOCK)+ 1;
 mem_blocks_new=((*hndl_size+len_str)/ANA_HNDL_BLOCK)+1;
 if (mem_blocks_new>mem_blocks_now) { 
  char *tmp=apr_pstrndup(this_pool, *hndl, *hndl_size);
  //make new handle +1024 bigger than before
  hndl= (char **) apr_pcalloc(this_pool, mem_blocks_new*ANA_HNDL_BLOCK);
  strcpy(*hndl, tmp);
  strncat(*hndl, str, len_str);
 } else {
  strncat(*hndl, str, len_str);
 }
 *hndl_size+=len_str;
 return true;
}

int get_int(const unsigned char *ptr, int ind) {
 int ret_val;
    ptr+=ind*4;
    ret_val= (ptr[0]<<24) | (ptr[1]<<16) | (ptr[2]<<8) | ptr[3];
    return (ret_val);
}

int upend (int number) {
 /* Little endian, big endian conversion. */
 union {
  int num;
  char bytes[4];
 } bill,ben;
 int x;
 if (anastasia_info.big_endian) {
  return number;
 } else {
  bill.num=number;
  for (x=0;x<4;x++) ben.bytes[x]=bill.bytes[3-x];
  return ben.num;
 }
}

int read_num_str(text_ptr t_obj, apr_file_t *src_ngv, process_book_ptr this_book_ptr, pool *this_pool) {
 apr_off_t _where = 0;
 apr_file_seek(src_ngv, APR_CUR, &_where);
 if (!read_num(&t_obj->len, src_ngv, _where)) return false;
 t_obj->len++;
 if (t_obj->len>PTRBUFF) {
  if (!set_text_obj_size(t_obj, this_book_ptr, this_pool)) return false;
 }
 apr_size_t _count = sizeof(char) * (t_obj->len-1);
 apr_file_read(src_ngv, t_obj->ptr, &_count);
 if (_count != (sizeof(char) * (t_obj->len-1))) return false;
 t_obj->ptr[t_obj->len-1]='\0';
 return true;
}

int empty_text_obj(text_ptr *t_obj, process_book_ptr this_book_ptr, pool *this_pool) {
 pool *temp_pool = NULL;
 dispose_txt_obj(t_obj, this_book_ptr);
 if (!init_text_obj(t_obj, this_book_ptr, temp_pool, __FILE__, __LINE__)) return false;
 return true;
}

int init_attr_obj(attr_ptr *this_attr, process_book_ptr this_book, pool *this_pool) {
 *this_attr=(attr_ptr) ana_malloc(&(this_book->anastasia_memory), sizeof(struct attr_obj), __FILE__, __LINE__);
 (*this_attr)->nattr = 0;
 (*this_attr)->pattr = 0;
 if ((!init_text_obj(&(*this_attr)->name, this_book, this_pool, __FILE__, __LINE__)) || (!init_text_obj(&(*this_attr)->value, this_book, this_pool, __FILE__, __LINE__))) {
  return false;
 } else {
  return true;
 }
}

int set_text_obj_size(text_ptr t_obj, process_book_ptr this_book_ptr, pool *this_pool) {
 unsigned long new_len=t_obj->len / PTRBUFF;
 //Remove the old t_obj->ptr is needed
 if (t_obj->ptr!=NULL) {
  ana_free(&(this_book_ptr->anastasia_memory), (t_obj->ptr));
 }
 t_obj->ptr = (char *) ana_malloc(&(this_book_ptr->anastasia_memory), ((new_len + 1) * PTRBUFF * sizeof(char)), __FILE__, __LINE__);
 t_obj->allocated = (new_len + 1) * PTRBUFF;
 return true;
}

//this guarantees that ancestor elements stay in scope, as we build the path
//has to be made to work in ALL circumstances (including toc).  THerefore, build it dynamically.
//so: first construct the path back to the root by reading parents of this element
//then track down the path concatenating gis
int make_element_path(sgml_obj_ptr this_sgml_el, process_book_ptr this_book) { //this calls itself from the top... pretty dumb idea really..
 int c;
 sgml_obj_ptr check_el=this_sgml_el;
 int this_el_num=this_sgml_el->number;
 int these_levels[n_levels];
 //if path already built: do nothing
// write_log(apr_psprintf(anastasia_info.server_pool, "Fetching all the elements to make_element_path, utils 948 for element %d", this_el_num));
 if (this_sgml_el->path->len>1) return 1;
 for (c=0; c<n_levels && this_el_num; c++) {
  these_levels[c]=this_el_num;
  this_el_num=check_el->ancestor;
  if (!get_next_sgml_obj(&check_el, this_el_num, this_book)) return 0;
 }
 these_levels[c]=0;  //got to the root
 //now, start adding the path to each element in turn
 for (; c>0; c--) {
  if (!get_next_sgml_obj(&check_el, these_levels[c], this_book)) return 0;
  if ((check_el) && (!add_str_to_text_obj(this_sgml_el->path, check_el->gi->ptr, this_book, this_sgml_el->object_pool))) return 0;
  if  (!add_str_to_text_obj(this_sgml_el->path, ",", this_book, this_sgml_el->object_pool)) return 0;
 }
 //finally add gi for this element
//  write_log(apr_psprintf(anastasia_info.server_pool, "Got the element path for element %d: %s, element is %s", this_el_num, this_sgml_el->path->ptr, this_sgml_el->gi->ptr));

 if  (!add_str_to_text_obj(this_sgml_el->path, this_sgml_el->gi->ptr, this_book, this_sgml_el->object_pool)) return 0;
 return 1;
}

//push this sgml obj onto front of array; push last accessed off the end and destroy if full; read in the element if we have to
int get_next_sgml_obj(sgml_obj_ptr *this_sgml_el, int sought_el, process_book_ptr this_book) {
 sgml_obj_ptr ret_data;
// write_log(apr_psprintf(anastasia_info.server_pool, "get_next_sgml_obj, utils.c 968 About to fetch..xml for %d", sought_el));
 ret_data = (struct sgml_obj *) get_hash_lilo_obj(this_book->sgml_objects, sought_el, NULL, this_book, 0);
// write_log(apr_psprintf(anastasia_info.server_pool, "Fetch..xml.. for %d",sought_el));
 *this_sgml_el=ret_data;
 if (*this_sgml_el==NULL) {
  //write_log("Error getting xml element.\n");
  return false;
 } else {
  return true;
 }
}

int init_txt_hndl(char ***txt, long *txt_len,  pool *this_pool) {
 char **ptr1;
 char *ptr2;
 size_t mem_req = ANA_HNDL_BLOCK * sizeof(char);
 //we set up this memory in MEM_BLOCK size blocks
 int mem_blocks=(mem_req/ANA_HNDL_BLOCK)+1;
 ptr2= (char *) apr_pcalloc(this_pool, mem_blocks*ANA_HNDL_BLOCK);
 ptr1= (char **) apr_pcalloc(this_pool, sizeof(char *));
 *ptr1 = ptr2;
 *txt = ptr1;
 *txt_len = 1;
 return true;
}

int read_str_exfile(text_ptr t_obj, int place, apr_file_t *in, style_ptr this_style, process_book_ptr this_book, pool *this_pool) {
 if  (!set_text_obj_size(t_obj, this_book, this_pool)) return 0;
 apr_off_t _where = place;
 apr_file_seek(in, APR_SET, &_where);
 apr_size_t _count = sizeof(char) * (t_obj->len-1);
 apr_file_read(in, t_obj->ptr, &_count);
 if (_count != sizeof(char) * (t_obj->len-1)) {
  write_output(this_style, this_book, "Error reading Anastasia .sgv file.\n");
  return 0;
 } 
 (t_obj->ptr)[t_obj->len - 1]='\0';
 return 1;
}

int http_inf_callback(void *ti, const char *name, const char *value) {
 char str[200];
 sprintf(str, "httpinf(%s)", name);
 Tcl_SetVar((Tcl_Interp *) ti, str, (char *) value, 0);
 return true;
}

int valid_num_param(char *num_str, char *nparam, char *callcmd, int *the_num, style_ptr this_style, process_book_ptr this_book) {
 char *end_ptr;
 *the_num=strtoll(num_str, &end_ptr, 10);
 //if alleged number string contains non-digits!!
 if (end_ptr[0]) {
  //if this: we are coming out of a anareader while loading a parameter
  char *this_out=apr_psprintf(this_style->style_pool, "Error processing Anastasia style TCL extension function \"%s\" in style file \"%s\" for element \"%s\": the %s parameter, which should indicate a number, contained the string \"%s\".  It should contain digits only.  Check the style file\n", callcmd, this_style->file_name, callcmd, nparam, end_ptr);
  write_output(this_style, this_book, this_out);
  *the_num=0;
  return false;
 }
 return true;
}

/*my proudest piece of programming.  Really smart object store handler*/
/*designed this way: if sought_str is NULL look for the designated object by number; else by str */
/*returns a void ptr to whatever object is addressed by this.  Caller has to coax that to shape to get what is wanted*/
//O.k. include an extra parameter to show what sort of object this is, either sgml or index object
//For the moment 0=sgml, 1=index...
void *get_hash_lilo_obj(hash_lilo_obj_ptr obj_ptr, int sought_num, char *sought_str, process_book_ptr this_book_ptr, int type) {
 //first: look to see if this is empty.  If so get the one we want and set first and last to point at it
 void *ret_data = 0;
 int got_it=0, allocated=0;
 hash_lilo_ptr *this_obj, temp_obj=0, prev_hash_obj=NULL;
 int hash_index, reclaimed;
 this_obj = NULL;
// write_log(apr_psprintf(anastasia_info.server_pool, "get_hash_lilo_obj 4, ana_utils.c 1041, element %d, %d", sought_num, type));
 if (!sought_str) {
  hash_index=sought_num%obj_ptr->htable_size;
  this_obj=&obj_ptr->htable[hash_index];
  while ((*this_obj!=NULL) && !got_it) {
    /* There already is entries with same hash value */
   if (sought_num!=(*this_obj)->comp_int) {
       /* No match */
       prev_hash_obj=*this_obj;
       this_obj=(hash_lilo_ptr *) &(*this_obj)->next_hash;
   } else {
    /* got it.  So return the data */
    got_it=1;
    ret_data= (*this_obj)->the_data;
    temp_obj = *this_obj;
     }  
  }
 } 
//  write_log("looking for a new object 4-1");
 if (got_it && (*this_obj)!=obj_ptr->first_lilo) {
  //its in the lilo and not currentyly first.  OK so shift it from its current position
  //this makes no difference whatever to the hash arrays
  hash_lilo_ptr next_lilo=(*this_obj)->next_lilo;
  hash_lilo_ptr previous_lilo=(*this_obj)->previous_lilo;
//  write_log("looking for a new object 4-1a");
  //adjust fore and aft lilo elements
  //we should never ever have this, but who knows...
  if (previous_lilo==NULL) {
//   write_log("this should not happen");
  } else {
   previous_lilo->next_lilo=next_lilo;
  }
  if (next_lilo!=NULL) 
   next_lilo->previous_lilo=previous_lilo;
  //if it is the last one: replace last lilo
  if ((*this_obj)==obj_ptr->last_lilo) {
   obj_ptr->last_lilo=previous_lilo;
  }
  (*this_obj)->previous_lilo=NULL;
  (*this_obj)->next_lilo = obj_ptr->first_lilo;
  temp_obj = *this_obj;
 }
 else if (!got_it) {
  /* we have not found one.  Call the initialize function to make a new one */
//  write_log("make a new object");
  ret_data=obj_ptr->callback_init(this_book_ptr, &allocated, &sought_num, this_book_ptr);
 // write_log("make a new object 2");
  if (ret_data==NULL) {
   return 0;
  }
  temp_obj=new_hash_lilo_item(ret_data, prev_hash_obj,sought_num, sought_str, this_book_ptr, obj_ptr->object_pool); 
//  write_log("make a new object 3");
  if (temp_obj==NULL) {
   obj_ptr->callback_delete(ret_data, &reclaimed, &sought_num, this_book_ptr);
   obj_ptr->current-=reclaimed;
   return 0;
  }
  temp_obj->allocated = allocated;
//   write_log("make a new object 3-1");
 
  //Now to stop problems with deleting hash lilo that we might be trying to 
  //refer to add this hash lilo to the array first.
  temp_obj->previous_hash=NULL;
  hash_index=sought_num%obj_ptr->htable_size;
  this_obj=&obj_ptr->htable[hash_index];
  while (*this_obj!=NULL && (*this_obj)->next_hash!=NULL) {
      this_obj=(hash_lilo_ptr *) &(*this_obj)->next_hash;
  }
  if (*this_obj==NULL) {
   *this_obj = temp_obj;
  } else {
   (*this_obj)->next_hash = temp_obj;
   temp_obj->previous_hash = (*this_obj);
  }
//   write_log("make a new object 3-2");
 
  //here we check we have room for this new object. If we don't, just wipe out from lilo till we do
  while ((obj_ptr->current+allocated>obj_ptr->capacity) && (obj_ptr->last_lilo)) {
   int reclaimed=0;
   hash_lilo_ptr last_lilo=obj_ptr->last_lilo;
   hash_lilo_ptr prev_lilo_el=last_lilo->previous_lilo;
   hash_lilo_ptr previous_hash=last_lilo->previous_hash;
   hash_lilo_ptr next_hash=last_lilo->next_hash;   
   int hash_index = last_lilo->comp_int%obj_ptr->htable_size;
   //in apache: all we have to do is delete the memory pool for the last lilo
   //apache must return the pool of what we are to delete
   if (last_lilo->the_data==NULL) {
    write_log("We have found a null value in the hash_lilo... this is very bad.");
   } else {
    obj_ptr->callback_delete(last_lilo->the_data, &reclaimed, &sought_num, this_book_ptr);
   }
   obj_ptr->number--;
   if (previous_hash!=NULL) {
    previous_hash->next_hash=next_hash;
   } else {
    //this is the first one in the hash table.  So set first entry to be next hash
    obj_ptr->htable[hash_index]=next_hash;
   }
   if (next_hash!=NULL)
    next_hash->previous_hash=previous_hash;
 //  write_log("make a new object 3-3");
   if (previous_hash==NULL && next_hash==NULL) {  //this is the only item in this position in the hash array: so none now
    //get its hash index
    obj_ptr->htable[hash_index]=NULL;
   }
   obj_ptr->last_lilo=prev_lilo_el;
   if (prev_lilo_el!=NULL) {
    prev_lilo_el->next_lilo=NULL;
   }
   if (obj_ptr->last_lilo==NULL)
    obj_ptr->first_lilo=NULL;  //probably should never happen.  But just in case..
   ana_free(&(this_book_ptr->anastasia_memory), last_lilo);
   obj_ptr->current-=reclaimed;
  }
  obj_ptr->current+=allocated;
  obj_ptr->number++;
 }
 if (temp_obj==NULL) {
  return NULL;
 }
//  write_log("make a new object 3-4a");
 if (!obj_ptr->first_lilo) {
  obj_ptr->first_lilo=temp_obj;
  obj_ptr->last_lilo=temp_obj;
 } else if (temp_obj!=obj_ptr->first_lilo) {
  if (temp_obj->next_lilo!=0) {
   temp_obj->next_lilo->previous_lilo = temp_obj->previous_lilo;
  }
  if (temp_obj->previous_lilo!=0) {
   temp_obj->previous_lilo->next_lilo = temp_obj->next_lilo;
  }
  //Removed it from where it was..
  if (temp_obj==obj_ptr->last_lilo) {
   obj_ptr->last_lilo = temp_obj->previous_lilo;
   if (temp_obj->previous_lilo!=0) {
    temp_obj->previous_lilo->next_lilo = NULL;
   }
  }
  //Check the last lilo
  //Now place it at the beginning
  temp_obj->previous_lilo = obj_ptr->last_lilo;
  temp_obj->next_lilo = obj_ptr->first_lilo;
  obj_ptr->first_lilo->previous_lilo = temp_obj;
  temp_obj->previous_lilo = NULL;
  obj_ptr->first_lilo = temp_obj;
 }
// write_log("make a new object 3-4");
 return ret_data;
}

hash_lilo_ptr new_hash_lilo_item(void *the_data, hash_lilo_ptr prev_hash_obj, int comp_int, char *comp_str, process_book_ptr this_book_ptr, pool *current_pool) {
 pool *hash_pool = NULL;
 hash_lilo_ptr this_item= (struct hash_lilo_item_inf *) ana_malloc(&(this_book_ptr->anastasia_memory), sizeof(struct hash_lilo_item_inf), __FILE__, __LINE__);
 if (this_item==NULL) {
  return NULL;
 }
 this_item->comp_int=comp_int;
 this_item->previous_hash=prev_hash_obj;
 this_item->next_hash=NULL;
 this_item->previous_lilo=NULL;
 this_item->next_lilo=NULL;
 if (comp_str) {
  this_item->comp_str= (char *) ana_malloc(&(this_book_ptr->anastasia_memory), (sizeof(char)*strlen(comp_str)), __FILE__, __LINE__);
  strcpy((char *) this_item->comp_str, comp_str);
  }
 else this_item->comp_str=NULL;
 this_item->hash_pool = hash_pool;
 this_item->the_data=the_data;
 this_item->check=0;
 return(this_item);
}

//given a file: load a specific number of chars; if you cannot, we are at the begining
int load_ex_file_off_back(apr_file_t *in, int start_pt, int chars_req, char *buf_read, int *chars_read, process_book_ptr this_book_ptr) {
 int chars_to_get;
    if (start_pt<(unsigned) chars_req) chars_to_get=start_pt;
  else chars_to_get=chars_req;
 apr_off_t _where = start_pt-chars_to_get;
 apr_file_seek(in, APR_SET, &_where);
 apr_size_t _count = sizeof(char) * chars_to_get;
 apr_file_read(in, buf_read, &_count);
 *chars_read=_count;
 if (*chars_read != sizeof(char) * chars_to_get) {
  pool *temp_pool = 0; apr_pool_create(&temp_pool, this_book_ptr->current_pool);
  char *this_out;
  this_out=apr_psprintf(temp_pool, "Error in reading sgrep file for book \"%s\". The function will fail.", this_book_ptr->bookname);
  write_log(this_out);
  return 0;
 }
 buf_read[*chars_read]='\0';
 return 1;
}

void *ana_ap_realloc(pool *the_p, void *array, size_t size_old, size_t size_new) {
 //make a new block big enough to hold this
    void *temp=apr_pcalloc(the_p, size_new);
if (size_new>=size_old)
        memcpy(temp, array, size_old);
    else //I dont know what to do!
    {
        int fred;
        fred=1;
    }
    return temp;
}

int init_output_buffer(buffer_ptr *output_buf, process_book_ptr this_book_ptr, pool *current_pool) {
 pool *output_pool;
 output_pool = NULL;
 (*output_buf) = (buffer_ptr) ana_malloc(&(this_book_ptr->anastasia_memory), sizeof(struct buffer_o), __FILE__, __LINE__);
 if ((*output_buf)==NULL) {
  return false;
 }
 (*output_buf)->buffer_obj_pool = NULL;
 (*output_buf)->parent_pool = NULL;
 (*output_buf)->ptr = (char *) ana_malloc(&(this_book_ptr->anastasia_memory), sizeof(char) * BUFFERSIZE, __FILE__, __LINE__);
 if ((*output_buf)->ptr==NULL) {
  ana_free(&(this_book_ptr->anastasia_memory), (*output_buf));
  return false;
 }
 strcpy((*output_buf)->ptr, "");
 (*output_buf)->len = 1;
 return true;
}

void add_str_to_buf(style_ptr this_style, char *str, process_book_ptr this_book_ptr) {
 int o_size = this_style->file_spec->len / BUFFERSIZE + 1;
 int n_size = (this_style->file_spec->len + strlen(str)) / BUFFERSIZE + 1;
 if (o_size==0) { o_size = 1; }
 if (n_size > o_size) {
  //Just reallocate some memory for this one..
  this_style->file_spec->ptr = (char *) ana_realloc(&(this_book_ptr->anastasia_memory), (void **) &this_style->file_spec->ptr, (o_size * BUFFERSIZE), (n_size * BUFFERSIZE), __FILE__, __LINE__);
 }
 strcat(this_style->file_spec->ptr, str);
 this_style->file_spec->len += strlen(str);
}

//Is this function actually freeing the memory?
void delete_output_buffer(buffer_ptr *output_buf, process_book_ptr this_book) {
 if ((*output_buf)->ptr!=NULL) {
  ana_free(&(this_book->anastasia_memory), ((*output_buf)->ptr));
 }
 ana_free(&(this_book->anastasia_memory), (*output_buf));
}

void clear_hash_lilo(process_book_ptr this_book) {
 hash_lilo_ptr this_hash_item=0, dele_hash_item=0;
 int a=0, reclaimed=0, sought_num=0;
 clear_sgml_items(this_book);
 ana_free(&(this_book->anastasia_memory), this_book->sgml_objects->htable);
 ana_free(&(this_book->anastasia_memory), this_book->sgml_objects);
 for (a=0; a<1021;a++) {
  this_hash_item = this_book->index_objects->htable[a];
  //Remove this item from the array
  if (this_hash_item!=NULL && this_hash_item->previous_hash!=NULL) {
   printf("Error in hash lilo\n");
  }
  while (this_hash_item!=NULL) {
   if (this_hash_item->next_hash!=NULL) {
    this_hash_item->next_hash->previous_hash = NULL;
   }
   if (this_hash_item->previous_lilo!=NULL) {
    this_hash_item->previous_lilo->next_lilo = this_hash_item->next_lilo;
   }
   if (this_hash_item->next_lilo!=NULL) {
    this_hash_item->next_lilo->previous_lilo = this_hash_item->previous_lilo;
   }
   if (this_hash_item==this_book->sgml_objects->first_lilo) {
    this_book->sgml_objects->first_lilo = this_hash_item->next_lilo;
   }
   if (this_hash_item==this_book->sgml_objects->last_lilo) {
    this_book->sgml_objects->last_lilo = this_hash_item->previous_lilo;
   }
   //Now delete the hash item
   dele_hash_item = this_hash_item;
   this_hash_item = this_hash_item->next_hash;
   //Delete the data
   reclaimed = 0;
   this_book->index_objects->callback_delete(dele_hash_item->the_data, &reclaimed, &sought_num, this_book);
   dele_hash_item->previous_lilo = NULL;
   dele_hash_item->next_lilo = NULL;
   dele_hash_item->previous_hash = NULL;
   dele_hash_item->next_hash = NULL;
   ana_free(&(this_book->anastasia_memory), dele_hash_item);
   this_book->index_objects->current-=reclaimed;
  }
 }
 ana_free(&(this_book->anastasia_memory), this_book->index_objects->htable);
 ana_free(&(this_book->anastasia_memory), this_book->index_objects);
}

void clear_sgml_items(process_book_ptr this_book) {
 hash_lilo_ptr this_hash_item=0, dele_hash_item=0;
 int a=0, reclaimed=0, sought_num=0;
 for (a=0; a<1021;a++) {
  if (this_book->sgml_objects!=NULL) {
   this_hash_item = this_book->sgml_objects->htable[a];
   //Remove this item from the array
   if (this_hash_item!=NULL && this_hash_item->previous_hash!=NULL) {
    printf("Error in hash lilo\n");
   }
  }
  while (this_hash_item!=NULL) {
   if (this_hash_item->next_hash!=NULL) {
    this_hash_item->next_hash->previous_hash = NULL;
   }
   if (this_hash_item->previous_lilo!=NULL) {
    this_hash_item->previous_lilo->next_lilo = this_hash_item->next_lilo;
   }
   if (this_hash_item->next_lilo!=NULL) {
    this_hash_item->next_lilo->previous_lilo = this_hash_item->previous_lilo;
   }
   if (this_hash_item==this_book->sgml_objects->first_lilo) {
    this_book->sgml_objects->first_lilo = this_hash_item->next_lilo;
   }
   if (this_hash_item==this_book->sgml_objects->last_lilo) {
    this_book->sgml_objects->last_lilo = this_hash_item->previous_lilo;
   }
   //Now delete the hash item
   dele_hash_item = this_hash_item;
   this_hash_item = this_hash_item->next_hash;
   //Delete the data
   reclaimed = 0;
	if (dele_hash_item->the_data!=0) {
	//if there has been a memory error: this may not have been completely allocated.  Need to check that the data has been properly assigned or we get an error
		sgml_obj_ptr delete_obj=(sgml_obj_ptr) dele_hash_item->the_data;
		if ((delete_obj)->number==0 && (delete_obj)->child==0) {
			//this one is corrupt!!
		} else {
			this_book->sgml_objects->callback_delete(dele_hash_item->the_data, &reclaimed, &sought_num, this_book);
		}
	}
   dele_hash_item->previous_lilo = NULL;
   dele_hash_item->next_lilo = NULL;
   dele_hash_item->previous_hash = NULL;
   dele_hash_item->next_hash = NULL;
   ana_free(&(this_book->anastasia_memory), dele_hash_item);
   this_book->sgml_objects->current-=reclaimed;
  }
  this_book->sgml_objects->htable[a] = NULL;
 }
}

int check_process_book(process_book_ptr this_book, style_ptr this_style_ptr, char *command, char* line, int lineno) {
 //Do some basic checks on the book
 char str[1024];
 //Create an array of ana_mem_info structures
#if defined(WIN32)
 MEMORYSTATUS meminfo;
 GlobalMemoryStatus(&meminfo);
#endif
 if (this_book==NULL) {
  sprintf(str, "Some bad voodoo is going on somewhere.... the process_book_ptr is NULL! in line - %s (%d).\n", line, lineno);
  write_log(str);
  return false;
 }
 if (this_book->in_idx==NULL || this_book->in_ngv==NULL || this_book->in_rgv==NULL || this_book->in_sgv==NULL) {
  sprintf(str, "Some bad voodoo is going on somewhere.... one of the file handles is NULL! in line - %s (%d).\n", line, lineno);
  write_log(str);
  return false;
 }
 if (this_book->sgml_objects==NULL) {
  sprintf(str, "Some bad voodoo is going on somewhere.... the sgml objects array is NULL! in line - %s (%d).\n", line, lineno);
  write_log(str);
  return false;
 }
 if (this_book->index_objects==NULL) {
  sprintf(str, "Some bad voodoo is going on somewhere.... the index objects array is NULL! in line - %s (%d).\n", line, lineno);
  write_log(str);
  return false;
 }
 //Check the memory usage
 if (this_book->anastasia_memory.memory_error==1) {
  sprintf(str, "<p><font color=\"red\">Error processing Anastasia style TCL extension function \"%s\", memory usage exceded.</p>\n", command);
  write_output(this_style_ptr, this_book, str);
  sprintf(str, "<p><font color=\"red\">Memory used %ld out of %ld total memory.</font></p>\n", this_book->anastasia_memory.mem_used, this_book->anastasia_memory.mem_allocated); 
  write_output(this_style_ptr, this_book, str);
  sprintf(str, "<p><font color=\"red\">%ld out of %ld blocks of memory used.</font></p>\n", this_book->anastasia_memory.number_of_blocks_used, this_book->anastasia_memory.number_of_blocks); 
  write_output(this_style_ptr, this_book, str);
  return TCL_ERROR;
 }
 //Check the timeout variable
 if (this_style_ptr->timeout + this_book->starttime <= time(NULL)) {
  sprintf(str, "<p><font color=\"red\">Error processing Anastasia style TCL extension function \"%s\", timeout occurred.</p>\n", command);
  write_output(this_style_ptr, this_book, str);
  return TCL_ERROR;
 }
 return TCL_OK;
}

//Right we need to check that all the objs in the hash are in the lilo as well... and the otherway round of course.
void check_hash_lilo(process_book_ptr this_book) {
 hash_lilo_ptr this_hash_item;
 int a;
 for (a=0; a<1021; a++) {
  this_hash_item = this_book->sgml_objects->htable[a];
  while (this_hash_item!=NULL) {
   if (this_hash_item->comp_str!=0) {
//    printf("Here");
   }
   if (this_hash_item->hash_pool!=0) {
//    printf("Here");
   }
   if (this_hash_item->the_data==0) {
//    printf("Here");
   }
   this_hash_item = this_hash_item->next_hash;
  }
 }
 this_hash_item = this_book->sgml_objects->first_lilo;
 while (this_hash_item!=0) {
  if (this_hash_item->check==1) {
//   printf("Here");
  }
  this_hash_item->check = 1;
  this_hash_item = this_hash_item->next_lilo;
 }
 this_hash_item = this_book->sgml_objects->first_lilo;
 while (this_hash_item!=0) {
  this_hash_item->check = 0;
  this_hash_item = this_hash_item->next_lilo;
 }
 return;
}

#if defined(WIN32)
void shortToLong(char *str, int length, char *rstr, int rlength) {
 //Need to extract each level of path from the string and convert it
 //to the long equivalent and rebuild the pathname in the return str
 //Loop through until a "//" is found and then perform a FindFirstFile on it
 int a, b=0, c=0;
 char tempstr[1024];
 char temp2str[1024];
 char *pstr = NULL;
 WIN32_FIND_DATA data;

 strcpy(tempstr, "");
 strcpy(temp2str, "");
 strcpy(rstr, "");
 //Remove any " from the start and end
 for (a=0; a<length; a++) {
  if (str[0]=='"') {
   for (b=1; b<length; b++) {
    str[(b-1)] = str[b];
   }
   if (str[(length-1)]=='"') {
    str[(b-2)]='\0';
   } else {
    str[(b-1)]='\0';
   }
  }
 }
 strncpy(rstr, str, 3);
 strcpy(str, (str+3));
 while ((pstr=strchr(str, 92))!=NULL) {
  //a now contains the position of the next '\'
  strncpy(tempstr, str, (pstr-str));
  tempstr[(pstr-str)]=0;
  strcpy(temp2str, rstr);
  if (rstr[strlen(rstr)-1]!=92) {
   strcat(rstr, "\\");
   strcat(temp2str, "\\");
  }
  strcat(temp2str, tempstr);
  //Now remove it from str
  strcpy(str, (pstr+1));
  //Now rstr should contain the new string
  if (FindFirstFile(temp2str, &data)==INVALID_HANDLE_VALUE) {
   strcat(rstr, tempstr);
  } else {
   strcat(rstr, data.cFileName);
  }
 }
 if (rstr[strlen(rstr)-1]!=92) {
  strcat(rstr, "\\");
 }
 strcpy(tempstr, rstr);
 strcat(tempstr, str);
 if (FindFirstFile(rstr, &data)==INVALID_HANDLE_VALUE) {
  strcat(rstr, str);
 } else {
  strcat(rstr, data.cFileName);
 }
}
#endif

int check_query_cond(int argc, char *the_line, cond_ptr cond, process_book_ptr this_book_ptr, pool *current_pool, style_ptr this_style_ptr) {
        if (the_line[0]=='\0') return 1;
        if (!read_query_cond(the_line, cond, this_book_ptr, current_pool, this_style_ptr)) {
            char *this_out=apr_psprintf(current_pool, "<p><font color=red>Error processing Anastasia VBase search condition \"%s\" in style file \"%s\". The search condition should have the form \"in/not [<2 of/>2 of/==2 of] Hg El Cn [with omword]\" (the conditions in square brackets are optional; the conditions separated by / are alternatives.)</font></p>\n", the_line, this_style_ptr->file_name);
            write_output(this_style_ptr, this_book_ptr, this_out);
            return 0;
 }
    return 1;
}

int read_query_cond(char *the_line, cond_ptr cond, process_book_ptr this_book_ptr, pool *current_pool, style_ptr this_style_ptr)
{
 char keywrd1[32];
 char keywrd2[32];
        char *dupcond =the_line;
 int nread;
        int done=0;
        char sig[128];
        int fnd_indx=0;
 nread=sscanf(the_line, "%4s", keywrd1);
 if (nread!=1) return(0);
 if (!strcmp(keywrd1,"not")) {
  cond->in_out=0;
  the_line+=4;
 }
 else cond->in_out=1;
 nread=sscanf(the_line, "%8s", keywrd1);
 if (nread!=1) return(0);
 if (strcmp(keywrd1,"in")) return(0);
 the_line+=3;  //skip past 'in'
 nread=sscanf(the_line, "%8s", keywrd1);
 if (nread!=1) return(0);
 nread=sscanf(the_line + strlen(keywrd1) + 1, "%8s", keywrd2);
 if (nread!=1) keywrd2[0]='\0';  
                //we dont need this... could be failure to read in case of 'in Hg' type
  //is there a numerical condition? if so: keywrd2="of", keywrd1=the condition
 if (!strcmp(keywrd2,"of")) {
  strcpy (cond->num_str, keywrd1);
  the_line+=strlen(keywrd1) + strlen(keywrd2) + 2;
 }
 else {
  cond->num_str[0]='\0';
 // the_line++;
 }
 cond->others=1;
 strcpy(cond->mss, the_line);
        //now check that these mss are all ok
        while (!done)
        {
            nread=sscanf(the_line, "%12s", sig);
            if (nread!=1) done = 1;
            if ((sig[0]!=' ') && (!done))
                {
                    int c = 0;
                    int sig_fnd = 0;
                    while (isspace(the_line[0]) && the_line[0]) the_line++;
                    the_line+=strlen(sig);
                    if (!strcmp(sig, "\\all"))
                        {
                                sig_fnd = 1;
                                done = 1;
                                for (c=0; (c<(this_book_ptr)->n_mss); c++)
                                        cond->cond_mss[c] = c;
                                cond->nom_mss = (this_book_ptr)->n_mss;
                                fnd_indx = (this_book_ptr)->n_mss;
                        }
                    for (c=0; (c<(this_book_ptr)->n_mss) && (!sig_fnd) && ((this_book_ptr)->sigil_key[c].ptr); c++)
                        if (!strcmp(sig, (this_book_ptr)->sigil_key[c].ptr)) 
                                {
                                        sig_fnd = 1;
                                        (cond->nom_mss)++;
                                        cond->cond_mss[fnd_indx++] = c;
                                }
                    if ((!sig_fnd))
                        {
                                if (!strcmp(sig, "with")) {
                                        nread=sscanf(the_line, "%8s", cond->var_type);
                                        done=1;
                                      
                                }
                                else {
                                        //send an error here
                                        //but press on regardless!
                                        char *this_out=apr_psprintf(current_pool, "<p><font color=red>Error processing Anastasia VBase search condition \"%s\" in style file \"%s\". The witness sigil \"%s\" is not present in the currently loaded VBase -- or perhaps you have omitted the \"of\" after a numeric specifier (as in \">2 of...\"). Check the spelling (remember that sigil names are case sensitive and cannot contain spaces). \n", dupcond, this_style_ptr->file_name,sig);
                                        write_output(this_style_ptr, this_book_ptr, this_out);
                                }
                        }
                }
        }
        //so now, let's adjust the other parts of the search condition -- originally from get_num_conds in Collate
        if (cond->in_out)
  {  
   cond->seek_mss = cond->nom_mss;
   cond->num_cond = OnlyNom;
  }
 else
  {
   cond->seek_mss = 0;
   cond->num_cond = NotAny;
  }
        if (!cond->num_str[0]) {
            cond->active=1;
            return 1;
        }
        else {
            //parse the condition
        int err_fnd =0;
 int dec_seek_num = 0;
 int inc_seek_num = 0;
        int c;

        for (c=0; isspace(cond->num_str[c]); c++);
 if ( (isdigit(cond->num_str[c])) || (cond->num_str[c] == '<') || 
  (cond->num_str[c] == '>') || (cond->num_str[c] == '=') )
  {
   if (cond->num_str[c] == '<')
    {
     if (cond->in_out)
      cond->num_cond = LessNom;
     else
      {
       cond->num_cond = MoreNom;
       dec_seek_num = 1;
      }
     c++;
     if (cond->num_str[c]=='<')
      {
       cond->others=0;
       c++;
      }
    }
   else if (cond->num_str[c] == '>')
    {
     if (cond->in_out)
      cond->num_cond = MoreNom;
     else
      {
       cond->num_cond = LessNom;
       inc_seek_num = 1;
      }
     c++;
     if (cond->num_str[c]=='>')
      {
       cond->others=0;
       c++;
      }
    }
   else if (cond->num_str[c] == '=')
    {
     if (!cond->in_out)
      cond->num_cond = NotNom;
     c++;
     if (cond->num_str[c]=='=')
      {
       cond->others=0;
       c++;
      }
    }
  }
 else err_fnd = true;
 if (!isdigit(cond->num_str[c])) err_fnd = 1;
 if (!err_fnd)
  {
   char num[32];
   register short c1;
   for (c1=0; isdigit(cond->num_str[c]) && (c1 < 32); c++, c1++)
    num[c1] = cond->num_str[c];
   num[c1] = '\0';
   cond->seek_mss = (short) atoi(num);
   if ((cond->seek_mss > cond->nom_mss) && (cond->num_cond != LessNom))
    {
                                    char *this_out=apr_psprintf(current_pool, "<p><font color=red>Error processing Anastasia VBase search condition \"%s\" in style file \"%s\". You have specified the search  for variants be for \"%s\" of the nominated witnesses. You have not nominated so many witnesses!</font></p>\n", dupcond, this_style_ptr->file_name, num);
                                    write_output(this_style_ptr, this_book_ptr, this_out);
                                    return(0);
    }
   if ((cond->seek_mss != 0) && (!cond->in_out) && (cond->num_cond==NotAny))
    cond->num_cond=NotNom;
  }
 for (; isspace(cond->num_str[c]); c++);
 if ((!err_fnd) && (!isdigit(cond->num_str[c])) && (cond->num_str[c] != '\0'))
  err_fnd = 1; 
 if (err_fnd)
  {
                        char *this_out=apr_psprintf(current_pool, "<p><font color=red>Error processing Anastasia VBase search condition \"%s\" in style file \"%s\". Anastasia cannot understand the qualification \"%s\".  This should be in the form \"<2 of/>2 of/==2 of\", where the conditions separated by / are alternatives</font></p>\n", dupcond, this_style_ptr->file_name, cond->num_str);
                        write_output(this_style_ptr, this_book_ptr, this_out);
   return(0);
  }
 if (dec_seek_num) (cond->seek_mss)--;
 if (inc_seek_num) (cond->seek_mss)++;

        }
        cond->active=1;
 return(1);
}

int write_num(unsigned long this_no, apr_file_t *out_f, long int place) {
//write them all out as readable text for now
 long int place_now = 0;
 if (place) {
  apr_off_t _where = 0;
  apr_file_seek(out_f, APR_CUR, &_where);
  place_now=_where;
  _where = place;
  apr_file_seek(out_f, APR_SET, &_where);
 }
 this_no = upend(this_no);
 //to do: test big or little endian and write it out just so
 apr_file_flush(out_f);
 apr_size_t _count = 4;
 apr_file_write(out_f, &this_no, &_count);
 apr_file_flush(out_f);
 if (_count != 4) return 0;
 if (place) {
	  apr_off_t _where = place_now;
	  apr_file_seek(out_f, APR_SET, &_where);
	 }
	 return 1;
	}

	int write_num_str(unsigned long len_str, char *the_str, apr_file_t *out_f) {
	 if (!write_num(len_str, out_f, 0)) return 0;
	 apr_size_t _count = sizeof(char) * len_str;
	 apr_file_write(out_f, the_str, &_count);
	 if (_count != (sizeof(char) * len_str)) return 0;
#if defined(DEBUG)
 //write out string WITHOUT final null; add char return
 _count=apr_file_printf(out_f, "\n");
 if (_count==EOF)  return 0;
#endif
 return 1;
}

void write_error_output(style_ptr this_style, process_book_ptr this_book_ptr, char *this_str) {
    #if defined(ANASTANDALONE)
        printf(this_str);
    #else
        char *str;
        int len;
        len = strlen(this_str);
        len += strlen("<p style=\"color: red\"></p>\r\n");
        str = ana_malloc(&(this_book_ptr->anastasia_memory), sizeof(char) * len, __FILE__, __LINE__);
        sprintf(str, "<p style=\"color: red\">%s</p>\r\n", this_str);
        write_output(this_style, this_book_ptr, str);
        ana_free(&(this_book_ptr->anastasia_memory), str);
    #endif
}

//Check the current directory for book and create the books array.
void createBooksArray(Tcl_Interp *tcl_interp, char *path) {
 char str[2048], str2[2048];
 if (strcmp(path, "")!=0) {
  sprintf(str, " foreach a [glob -types d -nocomplain -path \"%s\" *] {", path);
  sprintf(str2, " %s set dir [lindex [file split \"$a\"] end]; ", str);
  sprintf(str, " %s set filename [file join \"%s\" \"$dir\" AnaGrove \"${dir}.idx\"]; ", str2, path);
  sprintf(str2, "%s if {[file exist \"$filename\"]==1} { set books($dir) \"[file join \"$a\" AnaGrove]\" }", str);
  sprintf(str, " %s }", str2);
 } else {
  strcpy(str, " foreach a [glob -types d -nocomplain *] {");
  sprintf(str2, " %s set filename [file join \"%s\" \"$a\" AnaGrove \"${a}.idx\"]; ", str, anastasia_info.path);
  sprintf(str, "%s if {[file exist \"$filename\"]==1} { set books($a) \"[file join \"%s\" \"$a\" AnaGrove]\" }", str2, anastasia_info.path);
  strcat(str, "}");
 }
 Tcl_Eval(tcl_interp, str);
}

void globalHttppost(Tcl_Interp *tcl_interp) {
 char str[2048];
 strcpy(str," if {[info exist httppost]} {");
 strcat(str, " foreach a [array names httppost] {)");
 strcat(str, "  set $a $httppost($a);");
 strcat(str, " }");
 Tcl_Eval(tcl_interp, str);
}


int compareMemSlots(const void *a, const void *b) {
 ana_mem_info_ptr mem_a = (ana_mem_info_ptr) a;
 ana_mem_info_ptr mem_b = (ana_mem_info_ptr) b;
 if (mem_a->count > mem_b->count) {
  return 1;
 } else if (mem_a->count < mem_b->count) {
  return -1;
 } else {
  return 0;
 } 
}
