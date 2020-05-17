#include "ana_common.h"
#include "ana_commands.h"
#include "ana_browsegrove.h"
#include "ana_utils.h"
#include "sgrep/ana_sgrep.h"
#include <apr_strings.h>
#include <ctype.h>

int get_ana_attr(ClientData client_data, Tcl_Interp* tcl_interp, int argc, char **argv) {
 char *el_num_str=argv[1];
 char *attr_sought=argv[2];
 char *tcl_return;
 sgml_obj_ptr this_sgml_el;
 attr_ptr this_attr;
 process_book_ptr this_book_ptr = NULL;
 style_ptr this_style_ptr = NULL;
 create_process_book(&this_book_ptr, &this_style_ptr, tcl_interp, NULL, NULL, NULL, NULL);
 if (this_book_ptr==NULL || this_style_ptr==NULL) {
  write_log("Error processing Anastasia style TCL extension function \"attr\". Critical Error!.");
  return TCL_ERROR;
 }
 tcl_return=NULL;
 Tcl_ResetResult(tcl_interp);
 if (argc!=3) {
  send_ana_param_error(this_style_ptr, "attr", "two", "one for the element identifier and a second for the name attribute value sought", this_book_ptr);
  return check_process_book(this_book_ptr, this_style_ptr, "attr", __FILE__, __LINE__);
 }
 if (!is_valid_el(el_num_str, "attr", &this_sgml_el, this_book_ptr, this_style_ptr)) {
  return check_process_book(this_book_ptr, this_style_ptr, "attr", __FILE__, __LINE__);
 }
 this_attr = this_sgml_el->attrs;
 while (this_attr!=0) {
  if (!strncmp(attr_sought, this_attr->name->ptr, this_attr->name->len)) {
   tcl_return=Tcl_Alloc(this_attr->value->len);
   strncpy(tcl_return, this_attr->value->ptr, this_attr->value->len);
   Tcl_SetResult(tcl_interp, tcl_return, TCL_DYNAMIC);
   return check_process_book(this_book_ptr, this_style_ptr, "attr", __FILE__, __LINE__);
  }
  this_attr = this_attr->nattr;
 }
 return check_process_book(this_book_ptr, this_style_ptr, "attr", __FILE__, __LINE__);
}

int get_ana_attr_list(ClientData client_data, Tcl_Interp* tcl_interp, int argc, char **argv) {
 char *el_num_str=argv[1];
 char *separator="";
 char *tcl_return;
 unsigned int c = 0;
 sgml_obj_ptr this_sgml_el;
 attr_ptr this_attr;
 text_ptr this_attr_list=NULL;
 pool *current_pool;
 process_book_ptr this_book_ptr = NULL;
 style_ptr this_style_ptr = NULL;
 create_process_book(&this_book_ptr, &this_style_ptr, tcl_interp, NULL, NULL, NULL, NULL);
 if (this_book_ptr==NULL || this_style_ptr==NULL) {
  write_log("Error processing Anastasia style TCL extension function \"attrList\". Critical Error!.");
  return TCL_ERROR;
 }
 tcl_return=NULL;
 Tcl_ResetResult(tcl_interp);
 apr_pool_create(&current_pool, this_book_ptr->current_pool);
 if ((argc<2) || (argc>3)) {
  send_ana_param_error(this_style_ptr, "attrList", "one or two", "one for the element identifier, and a second optional one to surround the attribute values", this_book_ptr);
  return check_process_book(this_book_ptr, this_style_ptr, "attrList", __FILE__, __LINE__);
 }
 if (argc==3) separator=argv[2];
 if (!is_valid_el(el_num_str, "attrList", &this_sgml_el, this_book_ptr, this_style_ptr)) {
  return check_process_book(this_book_ptr, this_style_ptr, "attrList", __FILE__, __LINE__);
 }
 if (!init_text_obj(&this_attr_list, this_book_ptr, current_pool, __FILE__, __LINE__)) {
  return check_process_book(this_book_ptr, this_style_ptr, "attrList", __FILE__, __LINE__);
 }
 this_attr = this_sgml_el->attrs;
 c = 0;
 while (this_attr!=0) {
  if ((c!=0) &&
   (!add_str_to_text_obj(this_attr_list, " ", this_book_ptr, current_pool))) {
   return check_process_book(this_book_ptr, this_style_ptr, "attrList", __FILE__, __LINE__);
  }
  if (!add_str_to_text_obj(this_attr_list, this_attr->name->ptr, this_book_ptr, current_pool)) {
   return check_process_book(this_book_ptr, this_style_ptr, "attrList", __FILE__, __LINE__);
  }
  if (!add_str_to_text_obj(this_attr_list, "=", this_book_ptr, current_pool)) {
   return check_process_book(this_book_ptr, this_style_ptr, "attrList", __FILE__, __LINE__);
  }
  if (!add_str_to_text_obj(this_attr_list, separator, this_book_ptr, current_pool)) {
   return check_process_book(this_book_ptr, this_style_ptr, "attrList", __FILE__, __LINE__);
  }
  if (!add_str_to_text_obj(this_attr_list, this_attr->value->ptr, this_book_ptr, current_pool)) {
   return check_process_book(this_book_ptr, this_style_ptr, "attrList", __FILE__, __LINE__);
  }
  if (!add_str_to_text_obj(this_attr_list, separator, this_book_ptr, current_pool)) {
   return check_process_book(this_book_ptr, this_style_ptr, "attrList", __FILE__, __LINE__);
  }
  c =1;
  this_attr = this_attr->nattr;
 }
 tcl_return=Tcl_Alloc(this_attr_list->len);
 strncpy(tcl_return, this_attr_list->ptr, this_attr_list->len);
 Tcl_SetResult(tcl_interp, tcl_return, TCL_DYNAMIC);
 dispose_txt_obj(&this_attr_list,this_book_ptr);
 this_attr_list = NULL;
 apr_pool_destroy(current_pool);
 return check_process_book(this_book_ptr, this_style_ptr, "attrList", __FILE__, __LINE__);
}

int get_ana_attr_names(ClientData client_data, Tcl_Interp* tcl_interp, int argc, char **argv) {
 char *el_num_str=argv[1];
 unsigned int c = 0;
 sgml_obj_ptr this_sgml_el;
 Tcl_Obj *obj;  //if we ever get more than 100 attributes!!
 Tcl_Obj *listPtr;
 process_book_ptr this_book_ptr = NULL;
 style_ptr this_style_ptr = NULL;
 attr_ptr this_attr;
 create_process_book(&this_book_ptr, &this_style_ptr, tcl_interp, NULL, NULL, NULL, NULL);
 if (this_book_ptr==NULL || this_style_ptr==NULL) {
  write_log("Error processing Anastasia style TCL extension function \"attr_names\". Critical Error!.");
  return TCL_OK;
 }
 Tcl_ResetResult(tcl_interp);
 if (argc!=2)  {
  send_ana_param_error(this_style_ptr, "attrNames", "one", "for the element identifier", this_book_ptr);
  return check_process_book(this_book_ptr, this_style_ptr, "attr_names", __FILE__, __LINE__);
 }
 if (!is_valid_el(el_num_str, "attrNames", &this_sgml_el, this_book_ptr, this_style_ptr)) {
  return check_process_book(this_book_ptr, this_style_ptr, "attr_names", __FILE__, __LINE__);
 }
 this_attr = this_sgml_el->attrs;
 listPtr = Tcl_NewListObj(0, NULL);
 for (c=0; c<this_sgml_el->n_attrs && this_attr!=0; c++) {
  obj = Tcl_NewObj();
  Tcl_SetStringObj(obj, this_attr->name->ptr, (this_attr->name->len - 1));
  Tcl_ListObjAppendList(tcl_interp, listPtr, obj);
  this_attr = this_attr->nattr;
 }
 Tcl_SetObjResult(tcl_interp, listPtr);
 return check_process_book(this_book_ptr, this_style_ptr, "attr_names", __FILE__, __LINE__);
}

int get_ana_ancestor(ClientData client_data, Tcl_Interp* tcl_interp, int argc, char **argv) {
 char *el_num_str=argv[1];
 char *tcl_return;
 char *spec_str, *seek_eltype=NULL;
 int spec_num, result=2;
 int got_str, got_num;
 sgml_obj_ptr this_sgml_el;
 char this_el[512];
 int el_sought;
 pool *current_pool;
 process_book_ptr this_book_ptr = NULL;
 style_ptr this_style_ptr = NULL;
 //write_log("in get_ana_ancestor 1");
 create_process_book(&this_book_ptr, &this_style_ptr, tcl_interp, NULL, NULL, NULL, NULL);
 if (this_book_ptr==NULL || this_style_ptr==NULL) {
  write_log("Error processing Anastasia style TCL extension function \"ancestor\". Critical Error!.");
  return TCL_OK;
 }
 tcl_return=NULL;
 apr_pool_create(&current_pool, this_book_ptr->current_pool);
 got_str=got_num=0;
 tcl_return=NULL;
 Tcl_ResetResult(tcl_interp);
 if ((argc<2) || (argc>4)) {
  send_ana_param_error(this_style_ptr, "ancestor", "one, two, or three", "one for the element identifier, one for the number of the ancestor to be returned, one for the SGML ancestor element sought", this_book_ptr);
  return check_process_book(this_book_ptr, this_style_ptr, "ancestor", __FILE__, __LINE__);
 }
 //fill in strings, if any
 if ((argc>2) && (!fill_in_specs(argc, argv, &got_str, &got_num, &spec_str, &spec_num, "ancestor", this_style_ptr, this_book_ptr))) {
  return check_process_book(this_book_ptr, this_style_ptr, "ancestor", __FILE__, __LINE__);
 }
 if (!is_valid_el(el_num_str, "ancestor", &this_sgml_el, this_book_ptr, this_style_ptr)) {
  return check_process_book(this_book_ptr, this_style_ptr, "ancestor", __FILE__, __LINE__);
 }
 el_sought=this_sgml_el->number;
 //now, find the ancestor
 if (!got_num) spec_num=1;
 if (got_str) {
  seek_eltype=apr_pstrdup(current_pool, spec_str);
  //could be qualified path: look for immediate parents only
  while (remove_to_token(seek_eltype,",", this_el)) {
   switch (find_parent(el_sought,1, this_el, &el_sought, this_book_ptr)) {  
   case 0: {
    result= 0;
    apr_pool_destroy(current_pool);
    return check_process_book(this_book_ptr, this_style_ptr, "ancestor", __FILE__, __LINE__);
   }
   case 1: {
    result=1;
    break; //Not found, no error.  don't give a warning.  Up to users to figure out something is wrong
   }
   case 2: {
    ;   //ok; just carry on
   }
   }
  }
 } 
 //now find the actual one we want
 if (result==2) result=find_ancestor(el_sought,spec_num, seek_eltype, &el_sought, this_book_ptr);
 if (!result) {
  apr_pool_destroy(current_pool);
  return check_process_book(this_book_ptr, this_style_ptr, "ancestor", __FILE__, __LINE__);
 }
 if (result==1) el_sought=0;
 sprintf(this_el,"%d", el_sought);
 tcl_return=Tcl_Alloc(strlen(this_el) + 1);
 strcpy(tcl_return, this_el);
 Tcl_SetResult(tcl_interp, tcl_return, TCL_DYNAMIC);
 apr_pool_destroy(current_pool);
 return check_process_book(this_book_ptr, this_style_ptr, "ancestor", __FILE__, __LINE__);
}

int get_ana_lsibling(ClientData client_data, Tcl_Interp* tcl_interp, int argc, char **argv) {
 char *el_num_str=argv[1];
 char *tcl_return;
 char *spec_str, *seek_eltype=NULL;
 int spec_num, result=2;
 int got_str, got_num;
 sgml_obj_ptr this_sgml_el;
 char this_el[512];
 int el_sought;
 process_book_ptr this_book_ptr = NULL;
 style_ptr this_style_ptr = NULL;
 create_process_book(&this_book_ptr, &this_style_ptr, tcl_interp, NULL, NULL, NULL, NULL);
 if (this_book_ptr==NULL || this_style_ptr==NULL) {
  write_log("Error processing Anastasia style TCL extension function \"leftSibling\". Critical Error!.");
  return TCL_OK;
 }
 tcl_return=NULL;
 got_str=got_num=0;
 Tcl_ResetResult(tcl_interp);
 if ((argc<2) || (argc>4)) {
  send_ana_param_error(this_style_ptr, "leftsibling", "one, two, or three", "one for the element identifier, one for the number of the left sibling to be returned, one for the SGML left sibling element sought", this_book_ptr);
  return check_process_book(this_book_ptr, this_style_ptr, "leftSibling", __FILE__, __LINE__);
 }
 //fill in strings, if any
 if ((argc>2) && (!fill_in_specs(argc, argv, &got_str, &got_num, &spec_str, &spec_num, "leftSibling", this_style_ptr, this_book_ptr))) {
  return check_process_book(this_book_ptr, this_style_ptr, "leftSibling", __FILE__, __LINE__);
 }
 if (!is_valid_el(el_num_str, "left sibling", &this_sgml_el, this_book_ptr, this_style_ptr)) {
  return check_process_book(this_book_ptr, this_style_ptr, "leftSibling", __FILE__, __LINE__);
 }
 el_sought=this_sgml_el->number;
 //now, find the sibling
 if (!got_num) spec_num=1;
 if (got_str) seek_eltype=spec_str;
 //now find the actual one we want
 result=find_lsibling(el_sought,spec_num, seek_eltype, &el_sought, this_book_ptr);
 if (!result) {
  return check_process_book(this_book_ptr, this_style_ptr, "leftSibling", __FILE__, __LINE__);
 }
 if (result==1) el_sought=0;
 sprintf(this_el,"%d", el_sought);
 tcl_return=Tcl_Alloc(strlen(this_el) + 1);
 strcpy(tcl_return, this_el);
 Tcl_SetResult(tcl_interp, tcl_return, TCL_DYNAMIC);
 return check_process_book(this_book_ptr, this_style_ptr, "leftSibling", __FILE__, __LINE__);
}

int get_ana_rsibling(ClientData client_data, Tcl_Interp* tcl_interp, int argc, char **argv) {
 char *el_num_str=argv[1];
 char *tcl_return;
 char *spec_str, *seek_eltype=NULL;
 int spec_num, result=2;
 int got_str, got_num;
 sgml_obj_ptr this_sgml_el;
 char this_el[512];
 int el_sought;
 process_book_ptr this_book_ptr = NULL;
 style_ptr this_style_ptr = NULL;
 create_process_book(&this_book_ptr, &this_style_ptr, tcl_interp, NULL, NULL, NULL, NULL);
 if (this_book_ptr==NULL || this_style_ptr==NULL) {
  write_log("Error processing Anastasia style TCL extension function \"rightSibling\". Critical Error!.");
  return TCL_OK;
 }
 got_str=got_num=0;
 Tcl_ResetResult(tcl_interp);
 if ((argc<2) || (argc>4)) {
  send_ana_param_error(this_style_ptr, "rightsibling", "one, two, or three", "one for the element identifier, one for the number of the right sibling to be returned, one for the SGML right sibling element sought", this_book_ptr);
  return check_process_book(this_book_ptr, this_style_ptr, "rightSibling", __FILE__, __LINE__);
 }
 //fill in strings, if any
 if ((argc>2) && (!fill_in_specs(argc, argv, &got_str, &got_num, &spec_str, &spec_num, "rightSibling", this_style_ptr, this_book_ptr))) {
  return check_process_book(this_book_ptr, this_style_ptr, "rightSibling", __FILE__, __LINE__);
 }
 if (!is_valid_el(el_num_str, "right sibling", &this_sgml_el, this_book_ptr, this_style_ptr)) {
  return check_process_book(this_book_ptr, this_style_ptr, "rightSibling", __FILE__, __LINE__);
 }
 el_sought=this_sgml_el->number;
 //now, find the sibling
 if (!got_num) spec_num=1;
 if (got_str) seek_eltype=spec_str;
 //now find the actual one we want
 result=find_rsibling(el_sought,spec_num, seek_eltype, &el_sought, this_book_ptr);
 if (!result) {
  return check_process_book(this_book_ptr, this_style_ptr, "rightSibling", __FILE__, __LINE__);
 }
 if (result==1) el_sought=0;
 sprintf(this_el,"%d", el_sought);
 tcl_return=Tcl_Alloc(strlen(this_el) + 1);
 strcpy(tcl_return, this_el);
 Tcl_SetResult(tcl_interp, tcl_return, TCL_DYNAMIC);
 return check_process_book(this_book_ptr, this_style_ptr, "rightSibling", __FILE__, __LINE__);
}

int get_ana_descendant(ClientData client_data, Tcl_Interp* tcl_interp, int argc, char **argv) {
 char *el_num_str=argv[1];
 char *tcl_return;
 char *spec_str, *seek_eltype=NULL;
 int spec_num, result=2;
 int got_str, got_num;
 sgml_obj_ptr this_sgml_el;
 char this_el[512];
 int el_sought;
 pool *current_pool;
 process_book_ptr this_book_ptr = NULL;
 style_ptr this_style_ptr = NULL;
 create_process_book(&this_book_ptr, &this_style_ptr, tcl_interp, NULL, NULL, NULL, NULL);
 if (this_book_ptr==NULL || this_style_ptr==NULL) {
  write_log("Error processing Anastasia style TCL extension function \"descendant\". Critical Error!.");
  return TCL_OK;
 }
 apr_pool_create(&current_pool, this_book_ptr->current_pool);
 got_str=got_num=0;
 Tcl_ResetResult(tcl_interp);
 if ((argc<2) || (argc>4)) {
  send_ana_param_error(this_style_ptr, "descendant", "one, two, or three", "one for the element identifier, one for the number of the descendant to be returned, one for the SGML descendant element sought", this_book_ptr);
  return check_process_book(this_book_ptr, this_style_ptr, "descendant", __FILE__, __LINE__);
 }
 //fill in strings, if any
 if ((argc>2) && (!fill_in_specs(argc, argv, &got_str, &got_num, &spec_str, &spec_num, "descendant", this_style_ptr, this_book_ptr))) {
  return check_process_book(this_book_ptr, this_style_ptr, "descendant", __FILE__, __LINE__);
 }
 if (!is_valid_el(el_num_str, "descendant", &this_sgml_el, this_book_ptr, this_style_ptr)) {
  return check_process_book(this_book_ptr, this_style_ptr, "descendant", __FILE__, __LINE__);
 }
 el_sought=this_sgml_el->number;
 //now, find the child
 if (!got_num) spec_num=1;
 if (got_str) {
  seek_eltype=apr_pstrdup(current_pool, spec_str);
  //could be qualified path
  while (remove_to_token(seek_eltype,",", this_el)) {
   switch (find_child(el_sought,1, this_el, &el_sought, this_book_ptr)) {  
   case 0: {
    result= 0;
    apr_pool_destroy(current_pool);
    return check_process_book(this_book_ptr, this_style_ptr, "descendant", __FILE__, __LINE__);
   }
   case 1:{
    result=1;
    break; //Not found, no error.  don't give a warning.  Up to users to figure out something is wrong
   }
   case 2:{
    ;   //ok; just carry on
   }
   }
  }
 }
 //now find the actual one we want
 if (result==2) result=find_descendant(el_sought,spec_num, seek_eltype, &el_sought, this_book_ptr);
 if (!result) {
  apr_pool_destroy(current_pool);
  return check_process_book(this_book_ptr, this_style_ptr, "descendant", __FILE__, __LINE__);
 }
 if (result==1) el_sought=0;
 sprintf(this_el,"%d", el_sought);
 tcl_return=Tcl_Alloc(strlen(this_el) + 1);
 strcpy(tcl_return, this_el);
 Tcl_SetResult(tcl_interp, tcl_return, TCL_DYNAMIC);
 apr_pool_destroy(current_pool);
 return check_process_book(this_book_ptr, this_style_ptr, "descendant", __FILE__, __LINE__);
}


int get_ana_previous_el(ClientData client_data, Tcl_Interp* tcl_interp, int argc, char **argv) {
 char *el_num_str=argv[1];
 char *tcl_return;
 char *spec_str, *seek_eltype=NULL;
 int spec_num, result=2;
 int got_str, got_num;
 sgml_obj_ptr this_sgml_el;
 char this_el[512];
 int el_sought;
 process_book_ptr this_book_ptr = NULL;
 style_ptr this_style_ptr = NULL;
 create_process_book(&this_book_ptr, &this_style_ptr, tcl_interp, NULL, NULL, NULL, NULL);
 if (this_book_ptr==NULL || this_style_ptr==NULL) {
  write_log("Error processing Anastasia style TCL extension function \"previous\". Critical Error!.");
  return TCL_OK;
 }
 tcl_return=NULL;
 got_str=got_num=0;
 Tcl_ResetResult(tcl_interp);
 if ((argc<2) || (argc>4)) {
  send_ana_param_error(this_style_ptr, "previous", "one, two, or three", "one for the element identifier, one for the number of the previous element to be returned, one for the SGML previous element sought", this_book_ptr);
  return check_process_book(this_book_ptr, this_style_ptr, "previous", __FILE__, __LINE__);
 }
 //fill in strings, if any
 if ((argc>2) && (!fill_in_specs(argc, argv, &got_str, &got_num, &spec_str, &spec_num, "previous", this_style_ptr, this_book_ptr))) {
  return check_process_book(this_book_ptr, this_style_ptr, "previous", __FILE__, __LINE__);
 }
 if (!is_valid_el(el_num_str, "previous", &this_sgml_el, this_book_ptr, this_style_ptr)) {
  return check_process_book(this_book_ptr, this_style_ptr, "previous", __FILE__, __LINE__);
 }
 el_sought=this_sgml_el->number;
 //now, find the sibling
 if (!got_num) spec_num=1;
 if (got_str) seek_eltype=spec_str;
    //now find the actual one we want
 result=find_prev_el(el_sought,spec_num, seek_eltype, &el_sought, this_book_ptr);
 if (!result) {
  return check_process_book(this_book_ptr, this_style_ptr, "previous", __FILE__, __LINE__);
 }
 if (result==1) el_sought=0;
 sprintf(this_el,"%d", el_sought);
 tcl_return=Tcl_Alloc(strlen(this_el) + 1);
 strcpy(tcl_return, this_el);
 Tcl_SetResult(tcl_interp, tcl_return, TCL_DYNAMIC);
 return check_process_book(this_book_ptr, this_style_ptr, "previous", __FILE__, __LINE__);
}

//this and next have three params...the id of the el we are in, and optionally the element sought and the sequential number of the element
int get_ana_next_el(ClientData client_data, Tcl_Interp* tcl_interp, int argc, char **argv) {
 char *el_num_str=argv[1];
 char *tcl_return;
 char *spec_str, *seek_eltype=NULL;
 int spec_num, result=2;
 int got_str, got_num;
 sgml_obj_ptr this_sgml_el;
 char this_el[512];
 int el_sought;
 process_book_ptr this_book_ptr = NULL;
 style_ptr this_style_ptr = NULL;
// write_log("in get_ana_next_el");
 create_process_book(&this_book_ptr, &this_style_ptr, tcl_interp, NULL, NULL, NULL, NULL);
 if (this_book_ptr==NULL || this_style_ptr==NULL) {
  write_log("Error processing Anastasia style TCL extension function \"next\". Critical Error!.");
  return TCL_OK;
 }
 got_str=got_num=0;
 Tcl_ResetResult(tcl_interp);
 if ((argc<2) || (argc>4)) {
  send_ana_param_error(this_style_ptr, "next", "one, two, or three", "one for the element identifier, one for the number of the previous element to be returned, one for the SGML previous element sought", this_book_ptr);
  return check_process_book(this_book_ptr, this_style_ptr, "next", __FILE__, __LINE__);
 }
 //fill in strings, if any
 if ((argc>2) && (!fill_in_specs(argc, argv, &got_str, &got_num, &spec_str, &spec_num, "next", this_style_ptr, this_book_ptr))) {
  return check_process_book(this_book_ptr, this_style_ptr, "next", __FILE__, __LINE__);
 }
 if (!is_valid_el(el_num_str, "next", &this_sgml_el, this_book_ptr, this_style_ptr)) {
  return check_process_book(this_book_ptr, this_style_ptr, "next", __FILE__, __LINE__);
 }
 el_sought=this_sgml_el->number;
 //now, find the sibling
 if (!got_num) spec_num=1;
 if (got_str) seek_eltype=spec_str;
    //now find the actual one we want
 result=find_next_el(el_sought,spec_num, seek_eltype, &el_sought, this_book_ptr);
 if ((!result) || (result==1))
  el_sought=0;
 sprintf(this_el,"%d", el_sought);
 tcl_return=Tcl_Alloc(strlen(this_el) + 1);
 strcpy(tcl_return, this_el);
 Tcl_SetResult(tcl_interp, tcl_return, TCL_DYNAMIC);
 return check_process_book(this_book_ptr, this_style_ptr, "next", __FILE__, __LINE__);
}

int get_ana_child(ClientData client_data, Tcl_Interp* tcl_interp, int argc, char **argv) {
 char *el_num_str=argv[1];
 char *tcl_return;
 char *spec_str, *seek_eltype=NULL;
 int spec_num, result=2;
 int got_str, got_num;
 sgml_obj_ptr this_sgml_el;
 char this_el[512];
 int el_sought;
 pool *current_pool;
 process_book_ptr this_book_ptr = NULL;
 style_ptr this_style_ptr = NULL;
// write_log("in get ana child 1");
 create_process_book(&this_book_ptr, &this_style_ptr, tcl_interp, NULL, NULL, NULL, NULL);
// write_log("in get ana child 2");
 if (this_book_ptr==NULL || this_style_ptr==NULL) {
  write_log("Error processing Anastasia style TCL extension function \"child\". Critical Error!.");
  return TCL_OK;
 }
 tcl_return=NULL;
 //write_log("in get ana child 3");
 apr_pool_create(&current_pool, this_book_ptr->current_pool);
 got_str=got_num=0;
 Tcl_ResetResult(tcl_interp);
 //write_log("in get ana child 4");
 if ((argc<2) || (argc>4)) {
  send_ana_param_error(this_style_ptr, "child", "one, two, or three", "one for the element identifier, one for the number of the child to be returned, one for the SGML child element sought", this_book_ptr);
  return check_process_book(this_book_ptr, this_style_ptr, "child", __FILE__, __LINE__);
 }
 //fill in strings, if any
// write_log("in get ana child 5");
 if ((argc>2) && (!fill_in_specs(argc, argv, &got_str, &got_num, &spec_str, &spec_num, "child", this_style_ptr, this_book_ptr))) {
  return check_process_book(this_book_ptr, this_style_ptr, "child", __FILE__, __LINE__);
 }
// write_log("in get ana child 6");
 if (!is_valid_el(el_num_str, "child", &this_sgml_el, this_book_ptr, this_style_ptr)) {
  return check_process_book(this_book_ptr, this_style_ptr, "child", __FILE__, __LINE__);
 }
 el_sought=this_sgml_el->number;
 //now, find the child
 if (!got_num) spec_num=1;
 if (got_str) {
  seek_eltype=apr_pstrdup(current_pool, spec_str);
  //could be qualified path
  while (remove_to_token(seek_eltype,",", this_el)) {
   switch (find_child(el_sought,1, this_el, &el_sought, this_book_ptr)) {  
   case 0:{
    result= 0;
    apr_pool_destroy(current_pool);
    return check_process_book(this_book_ptr, this_style_ptr, "child", __FILE__, __LINE__);
     }
   case 1:{
    result=1;
    break; //Not found, no error.  don't give a warning.  Up to users to figure out something is wrong
     }
   case 2:{
    ;   //ok; just carry on
     }
   }
  }
 }
 //now find the actual one we want
 if (result==2) result=find_child(el_sought,spec_num, seek_eltype, &el_sought, this_book_ptr);
 if (!result) {
  apr_pool_destroy(current_pool);
  return check_process_book(this_book_ptr, this_style_ptr, "child", __FILE__, __LINE__);
 }
 if (result==1) el_sought=0;
 sprintf(this_el,"%d", el_sought);
 tcl_return=Tcl_Alloc(strlen(this_el) + 1);
 strcpy(tcl_return, this_el);
 Tcl_SetResult(tcl_interp, tcl_return, TCL_DYNAMIC);
 apr_pool_destroy(current_pool);

 return check_process_book(this_book_ptr, this_style_ptr, "child", __FILE__, __LINE__);
}

//returns the name of the current sDataElement; should be only one parameter
int get_ana_sDataName(ClientData client_data, Tcl_Interp* tcl_interp, int argc, char **argv) {
 char *el_num_str=argv[1];
 sgml_obj_ptr this_sgml_el;
 char *tcl_return;
 pool *current_pool;
 process_book_ptr this_book_ptr = NULL;
 style_ptr this_style_ptr = NULL;
 create_process_book(&this_book_ptr, &this_style_ptr, tcl_interp, NULL, NULL, NULL, NULL);
 if (this_book_ptr==NULL || this_style_ptr==NULL) {
  write_log("Error processing Anastasia style TCL extension function \"sDataName\". Critical Error!.");
  return TCL_OK;
 }
 tcl_return=NULL;
 apr_pool_create(&current_pool, this_book_ptr->current_pool);
 Tcl_ResetResult(tcl_interp);
 if (argc!=2) {
  send_ana_param_error(this_style_ptr, "sDataName", "one", "for the element identifier", this_book_ptr);
  apr_pool_destroy(current_pool);
  return check_process_book(this_book_ptr, this_style_ptr, "sDataName", __FILE__, __LINE__);
 }
 if (!is_valid_el(el_num_str, "sDataName", &this_sgml_el, this_book_ptr, this_style_ptr)) {
  apr_pool_destroy(current_pool);
  return check_process_book(this_book_ptr, this_style_ptr, "sDataName", __FILE__, __LINE__);
 }
 if (strcmp(this_sgml_el->gi->ptr,"SDATA")) {
                write_error_output(this_style_ptr, this_book_ptr, apr_psprintf(current_pool, "Error processing Anastasia style TCL extension function \"sDataName\" in style file \"%s\" for element \"%s\": this element is a \"<%s>\" and not an SDATA entity.  Check the style file.", this_style_ptr->file_name, el_num_str, this_sgml_el->gi->ptr));
  apr_pool_destroy(current_pool);
  return check_process_book(this_book_ptr, this_style_ptr, "sDataName", __FILE__, __LINE__);
 }
 tcl_return=Tcl_Alloc((this_sgml_el)->attrs->name->len);
 strncpy(tcl_return, (this_sgml_el)->attrs->name->ptr, (this_sgml_el)->attrs->name->len);
 Tcl_SetResult(tcl_interp, tcl_return, TCL_DYNAMIC);
 apr_pool_destroy(current_pool);
 return check_process_book(this_book_ptr, this_style_ptr, "sDataName", __FILE__, __LINE__);
}

int get_ana_sDataValue(ClientData client_data, Tcl_Interp* tcl_interp, int argc, char **argv) {
 char *el_num_str=argv[1];
 sgml_obj_ptr this_sgml_el;
 char *tcl_return;
 pool *current_pool;
 process_book_ptr this_book_ptr = NULL;
 style_ptr this_style_ptr = NULL;
 create_process_book(&this_book_ptr, &this_style_ptr, tcl_interp, NULL, NULL, NULL, NULL);
 if (this_book_ptr==NULL || this_style_ptr==NULL) {
  write_log("Error processing Anastasia style TCL extension function \"sDataValue\". Critical Error!.");
  return TCL_OK;
 }
 tcl_return=NULL;
 apr_pool_create(&current_pool, this_book_ptr->current_pool);
 Tcl_ResetResult(tcl_interp);
 if (argc!=2) {
  send_ana_param_error(this_style_ptr, "sDataValue", "one", "for the element identifier", this_book_ptr);
  apr_pool_destroy(current_pool);
  return check_process_book(this_book_ptr, this_style_ptr, "sDataValue", __FILE__, __LINE__);
 }
 if (!is_valid_el(el_num_str, "sDataName", &this_sgml_el, this_book_ptr, this_style_ptr)) {
  apr_pool_destroy(current_pool);
  return check_process_book(this_book_ptr, this_style_ptr, "sDataValue", __FILE__, __LINE__);
 }
 if (strcmp(this_sgml_el->gi->ptr,"SDATA")) {
                write_error_output(this_style_ptr, this_book_ptr, apr_psprintf(current_pool, "Error processing Anastasia style TCL extension function \"sDataValue\" in style file \"%s\" for element \"%s\": this element is a \"<%s>\" and not an SDATA entity.  Check the style file.", this_style_ptr->file_name, el_num_str, this_sgml_el->gi->ptr)); 
  apr_pool_destroy(current_pool);
  return check_process_book(this_book_ptr, this_style_ptr, "sDataValue", __FILE__, __LINE__);
 }
 tcl_return=Tcl_Alloc((this_sgml_el)->attrs->value->len);
 strncpy(tcl_return, (this_sgml_el)->attrs->value->ptr, (this_sgml_el)->attrs->value->len);
 Tcl_SetResult(tcl_interp, tcl_return, TCL_DYNAMIC);
 apr_pool_destroy(current_pool);
 return check_process_book(this_book_ptr, this_style_ptr, "sDataValue", __FILE__, __LINE__);
}

int get_ana_content_length(ClientData client_data, Tcl_Interp* tcl_interp, int argc, char **argv) {
 char *el_num_str=argv[1];
 sgml_obj_ptr this_sgml_el;
 char *tcl_return;
 char len_str[16];
 int length;
 process_book_ptr this_book_ptr = NULL;
 style_ptr this_style_ptr = NULL;
 create_process_book(&this_book_ptr, &this_style_ptr, tcl_interp, NULL, NULL, NULL, NULL);
 if (this_book_ptr==NULL || this_style_ptr==NULL) {
  write_log("Error processing Anastasia style TCL extension function \"contentLength\". Critical Error!.");
  return TCL_OK;
 }
 tcl_return=NULL;
 Tcl_ResetResult(tcl_interp);
 if (argc!=2) {
  send_ana_param_error(this_style_ptr, "contentLength", "one", "for the element identifier", this_book_ptr);
  return check_process_book(this_book_ptr, this_style_ptr, "contentLength", __FILE__, __LINE__);
 }
 if (!is_valid_el(el_num_str, "contentLength", &this_sgml_el, this_book_ptr, this_style_ptr)) {
  return check_process_book(this_book_ptr, this_style_ptr, "contentLength", __FILE__, __LINE__);
 }
 length=this_sgml_el->end_data - this_sgml_el->start_data; 
 sprintf(len_str, "%d", length);
 tcl_return=Tcl_Alloc(strlen(len_str) + 1);
 strcpy(tcl_return, len_str);
 Tcl_SetResult(tcl_interp, tcl_return, TCL_DYNAMIC);
 return check_process_book(this_book_ptr, this_style_ptr, "contentLength", __FILE__, __LINE__);
}

int get_ana_file_length(ClientData client_data, Tcl_Interp* tcl_interp, int argc, char **argv) {
 char *tcl_return;
 char len_str[16];
 int length;
 process_book_ptr this_book_ptr = NULL;
 style_ptr this_style_ptr = NULL;
 create_process_book(&this_book_ptr, &this_style_ptr, tcl_interp, NULL, NULL, NULL, NULL);
 if (this_book_ptr==NULL || this_style_ptr==NULL) {
  write_log("Error processing Anastasia style TCL extension function \"fileLength\". Critical Error!.");
  return TCL_OK;
 }
 tcl_return=NULL;
 Tcl_ResetResult(tcl_interp);
 if (this_style_ptr->file_spec->ptr && this_style_ptr->file_spec->len>0) {
  length=this_style_ptr->file_spec->len; 
 } else {
  length=0;
 }
 sprintf(len_str, "%d", length);
 tcl_return=Tcl_Alloc(strlen(len_str) + 1);
 strcpy(tcl_return, len_str);
 Tcl_SetResult(tcl_interp, tcl_return, TCL_DYNAMIC);
 return check_process_book(this_book_ptr, this_style_ptr, "fileLength", __FILE__, __LINE__);
}

//corresponds to [attr me name] command where me is element, name is name of attribute.  
//Returns value of attribute as string or "" if no such attribute
int get_ana_name(ClientData client_data, Tcl_Interp* tcl_interp, int argc, char **argv) {
 char *el_num_str=argv[1];
 char *tcl_return;
 sgml_obj_ptr this_sgml_el;
 process_book_ptr this_book_ptr = NULL;
 style_ptr this_style_ptr = NULL;
 create_process_book(&this_book_ptr, &this_style_ptr, tcl_interp, NULL, NULL, NULL, NULL);
 if (this_book_ptr==NULL || this_style_ptr==NULL) {
  write_log("Error processing Anastasia style TCL extension function \"giNme\". Critical Error!.");
  return TCL_OK;
 }
 tcl_return=NULL;
 Tcl_ResetResult(tcl_interp);
 if (argc!=2) {
  send_ana_param_error(this_style_ptr, "giName", "one", "for the element identifier", this_book_ptr);
  return check_process_book(this_book_ptr, this_style_ptr, "giName", __FILE__, __LINE__);
 }
 if (!is_valid_el(el_num_str, "giName", &this_sgml_el, this_book_ptr, this_style_ptr)) {
  return check_process_book(this_book_ptr, this_style_ptr, "giName", __FILE__, __LINE__);
 }
 tcl_return=Tcl_Alloc(this_sgml_el->gi->len);
 strncpy(tcl_return, this_sgml_el->gi->ptr, this_sgml_el->gi->len);
 Tcl_SetResult(tcl_interp, tcl_return, TCL_DYNAMIC);
 return check_process_book(this_book_ptr, this_style_ptr, "giName", __FILE__, __LINE__);
}

int get_ana_path(ClientData client_data, Tcl_Interp* tcl_interp, int argc, char **argv) {
 char *el_num_str=argv[1];
 char *tcl_return;
 sgml_obj_ptr this_sgml_el;
 process_book_ptr this_book_ptr = NULL;
 style_ptr this_style_ptr = NULL;
 create_process_book(&this_book_ptr, &this_style_ptr, tcl_interp, NULL, NULL, NULL, NULL);
 if (this_book_ptr==NULL || this_style_ptr==NULL) {
  write_log("Error processing Anastasia style TCL extension function \"giPath\". Critical Error!.");
  return TCL_OK;
 }
 tcl_return=NULL;
 Tcl_ResetResult(tcl_interp);
 if (argc!=2) {
  send_ana_param_error(this_style_ptr, "giPath", "one", "for the element identifier", this_book_ptr);
  return check_process_book(this_book_ptr, this_style_ptr, "giPath", __FILE__, __LINE__);
 }
 if (!is_valid_el(el_num_str, "giPath", &this_sgml_el, this_book_ptr, this_style_ptr)) {
  return check_process_book(this_book_ptr, this_style_ptr, "giPath", __FILE__, __LINE__);
 }
 tcl_return=Tcl_Alloc(this_sgml_el->path->len);
 strncpy(tcl_return, this_sgml_el->path->ptr, this_sgml_el->path->len);
 Tcl_SetResult(tcl_interp, tcl_return, TCL_DYNAMIC);
 return check_process_book(this_book_ptr, this_style_ptr, "giPath", __FILE__, __LINE__);
}

int get_start_sg(ClientData client_data, Tcl_Interp* tcl_interp, int argc, char **argv) {
 char *el_num_str=argv[1];
 char *tcl_return;
 char this_el[512];
 sgml_obj_ptr this_sgml_el;
 process_book_ptr this_book_ptr = NULL;
 style_ptr this_style_ptr = NULL;
 create_process_book(&this_book_ptr, &this_style_ptr, tcl_interp, NULL, NULL, NULL, NULL);
 if (this_book_ptr==NULL || this_style_ptr==NULL) {
  write_log("Error processing Anastasia style TCL extension function \"startSGElement\". Critical Error!.");
  return TCL_OK;
 }
 tcl_return=NULL;
 Tcl_ResetResult(tcl_interp);
 if (argc!=2) {
  send_ana_param_error(this_style_ptr, "startSGElement", "one", "for the element identifier", this_book_ptr);
  return check_process_book(this_book_ptr, this_style_ptr, "startSGElement", __FILE__, __LINE__);
 }
// write_log("get_start_sg commands 747");
 if (!is_valid_el(el_num_str, "startSGElement", &this_sgml_el, this_book_ptr, this_style_ptr)) {
  return check_process_book(this_book_ptr, this_style_ptr, "startSGElement", __FILE__, __LINE__);
 }
 sprintf(this_el,"%d", this_sgml_el->sgrep_start);
 tcl_return=Tcl_Alloc(strlen(this_el) + 1);
 strcpy(tcl_return, this_el);
 Tcl_SetResult(tcl_interp, tcl_return, TCL_DYNAMIC);
 return check_process_book(this_book_ptr, this_style_ptr, "startSGElement", __FILE__, __LINE__);
}

int get_end_sg(ClientData client_data, Tcl_Interp* tcl_interp, int argc, char **argv) {
 char *el_num_str=argv[1];
 char *tcl_return;
 char this_el[512];
 sgml_obj_ptr this_sgml_el;
 process_book_ptr this_book_ptr = NULL;
 style_ptr this_style_ptr = NULL;
 create_process_book(&this_book_ptr, &this_style_ptr, tcl_interp, NULL, NULL, NULL, NULL);
 if (this_book_ptr==NULL || this_style_ptr==NULL) {
  write_log("Error processing Anastasia style TCL extension function \"endSGElement\". Critical Error!.");
  return TCL_OK;
 }
 Tcl_ResetResult(tcl_interp);
 if (argc!=2) {
  send_ana_param_error(this_style_ptr, "endSGElement", "one", "for the element identifier", this_book_ptr);
  return check_process_book(this_book_ptr, this_style_ptr, "endSGElement", __FILE__, __LINE__);
 }
 if (!is_valid_el(el_num_str, "endSGElement", &this_sgml_el, this_book_ptr, this_style_ptr)) {
  return check_process_book(this_book_ptr, this_style_ptr, "endSGElement", __FILE__, __LINE__);
 }
 sprintf(this_el,"%d", this_sgml_el->sgrep_end);
 tcl_return=Tcl_Alloc(strlen(this_el) + 1);
 strcpy(tcl_return, this_el);
 Tcl_SetResult(tcl_interp, tcl_return, TCL_DYNAMIC);
 return check_process_book(this_book_ptr, this_style_ptr, "endSGElement", __FILE__, __LINE__);
}

//finds the element, returning the id of the first one found and all found in a tcl array
int get_find_sg_el(ClientData client_data, Tcl_Interp* tcl_interp,  int inargc,  char **inargv) {
 const char *tcl_return;
 int c=0, found=0, nresults=1, spec_num1=1, spec_num2=1;
 int d=0;
 int sstrlen = 0, sspos=0;
 char *search_string;
 char this_el[512], cmd_el[512], *str;
 char bname[512];
 pool *current_p;
 process_book_ptr this_book_ptr = NULL;
// write_log("looking in get_find_sg_el");
 style_ptr this_style_ptr = NULL;
        strcpy(bname, inargv[1]);
 create_process_book(&this_book_ptr, &this_style_ptr, tcl_interp, bname, NULL, NULL, NULL);
 apr_pool_create(&current_p, this_style_ptr->style_pool);
 if (this_book_ptr==NULL || this_style_ptr==NULL) {
  write_log("Error processing Anastasia style TCL extension function \"findSGElement\". Critical Error!.");
  apr_pool_destroy(current_p);
  return TCL_OK;
 }
 tcl_return=NULL;
 //do globals to pick up in search error..
 Tcl_ResetResult(tcl_interp);
 if ((inargc<2) || (inargc>5)) {
  send_ana_param_error(this_style_ptr, "findSGElement", "two, three, or four", "one for the book name, one for the SGREP search string, and two optional elements for number of first element to return and number of elements to return", this_book_ptr);
  apr_pool_destroy(current_p);
  return check_process_book(this_book_ptr, this_style_ptr, "findSGElement", __FILE__, __LINE__);
 }
 if ((!this_book_ptr) || (!this_book_ptr->map_idx)) {
                write_error_output(this_style_ptr, this_book_ptr, apr_psprintf(current_p, "No index or sgrep file for book \"%s\" currently loaded, in call from file \"%s\".  Search function findSGElement will fail.", this_book_ptr->bookname, this_style_ptr->file_name));
  apr_pool_destroy(current_p);
  return check_process_book(this_book_ptr, this_style_ptr, "findSGElement", __FILE__, __LINE__);
 }
 //fill in strings, if any
 if ((inargc>3) && (!fill_in_nums(inargc, inargv,&spec_num1, &spec_num2, "findSGElement", this_style_ptr, this_book_ptr))) {
  apr_pool_destroy(current_p);
  return check_process_book(this_book_ptr, this_style_ptr, "findSGElement", __FILE__, __LINE__);
 }
 //check that we are not asking for more than MAXSEARCHRESULTS (not smart to do this..)
 if (spec_num2>MAXSEARCHRESULTS) {
                write_error_output(this_style_ptr, this_book_ptr, apr_psprintf(current_p, "Error in findSGElement call for book \"%s\" currently loaded, in call from file \"%s\".  %i results requested; maximum permissible hits for any one search is %i.", this_book_ptr->bookname, this_style_ptr->file_name, spec_num2, MAXSEARCHRESULTS));
                spec_num2=MAXSEARCHRESULTS;
 }
 //O.k. before we go into this function we need to convert the search string.. 
 //The might be some utf8 characters in there so we need them in the format \#1234; or \#x1D3D;
 sstrlen = strlen(inargv[2]);
 search_string = (char *) apr_palloc(current_p, (sizeof(char) * sstrlen * 10));
// write_log("looking in get_find_sg_el 832");
 for (c=0; c<sstrlen; c++) {
  char chr = inargv[2][c];
  char tstr[10];
  long num = 0;
  int found_utf8 = false;
  //Check the character
  if ((chr&0xF8)==0xF8) {
   //5 byte character
   if (((inargv[2][c+1]&0x80)==0x80) && ((inargv[2][c+2]&0x80)==0x80) && ((inargv[2][c+5]&0x80)==0x80) && ((inargv[2][c+4]&0x80)==0x80)) {
    //Yep where find so convert.
    num = (((chr&0x3)<<24)+((inargv[2][c+1]&0x3F)<<18)+((inargv[2][c+2]&0x3F)<<12)+((inargv[2][c+3]&0x3F)<<6)+(inargv[2][c+4]&0x3F));
    found_utf8 = true;
    c+=4;
   }
  } else if ((chr&0xF0)==0xF0) {
   //4 byte character
   if (((inargv[2][c+1]&0x80)==0x80) && ((inargv[2][c+2]&0x80)==0x80) && ((inargv[2][c+5]&0x80)==0x80)) {
    //Yep where find so convert.
    num = (((chr&0x7)<<18)+((inargv[2][c+1]&0x3F)<<12)+((inargv[2][c+2]&0x3F)<<6)+(inargv[2][c+3]&0x3F));
    found_utf8 = true;
    c+=3;
   }
  } else if ((chr&0xE0)==0xE0) {
   //3 byte character
   if (((inargv[2][c+1]&0x80)==0x80) && ((inargv[2][c+2]&0x80)==0x80)) {
    //Yep where find so convert.
    num = (((chr&0xF)<<12)+((inargv[2][c+1]&0x3F)<<6)+(inargv[2][c+2]&0x3F));
    found_utf8 = true;
    c+=2;
   }
  } else if ((chr&0xC0)==0xC0) {
   //2 byte character
   if ((inargv[2][c+1]&0x80)==0x80) {
    //Yep where find so convert.
    num = (((chr&0x1F)<<6)+(inargv[2][c+1]&0x3F));
    found_utf8 = true;
    c+=1;
   }
  }
  if (found_utf8) {
   sprintf(tstr, "\\#x%x;", (unsigned int) num);
   //Now copy over those characters
   for (d=0; d<strlen(tstr); d++) {
    search_string[sspos++] = tstr[d];
   }
  } else {
   search_string[sspos++] = chr;
  }
 }
 if (search_string==NULL) {
  search_string = apr_pstrdup(current_p, "");
 }
 search_string[sspos] = '\0';
// write_log(apr_psprintf(current_p, "commmands.c 886 looking in get_find_sg_el 885 for %s", search_string));
// search_string=apr_psprintf(current_p,"stag(\"p\")%s", "");
 //do the search! specnum2 comes back with number actually put in results string, nresults total number found -- nresults also is found!
 found=do_find_sg_el(search_string, spec_num1, &spec_num2, &nresults, this_book_ptr, current_p);
 //we get back: number found in whole doc; number actually returned in results array; the results themsleves
//  write_log("looking in get_find_sg_el 890");
 //First check for an error
 if (found)  {
//  write_log("looking in get_find_sg_el 893");
  this_book_ptr->in_search=1;
  sprintf(this_el,"%d", spec_num2);
  Tcl_SetVar(tcl_interp, "sgReturned", this_el, 0);
  sprintf(this_el,"%d", found);
  Tcl_SetVar(tcl_interp, "sgFound", this_el, 0);
  //now we get the elements found out of the array and stick them in the array; also return first one as result
  for (c=0; c<spec_num2; c++) {
   spec_num1=*((int *) (this_book_ptr->search_els_found)+c);
//   write_log(apr_psprintf(current_p, "found %d", spec_num1 ));
   if (c==0) {
    sprintf(this_el,"%d", spec_num1);
    str=Tcl_Alloc(strlen(this_el) + 1);
    strcpy(str, this_el);
    Tcl_SetResult(tcl_interp, str, TCL_DYNAMIC); 
   }
   sprintf(this_el,"%d", spec_num1);
   sprintf(cmd_el,"esElIds(%d)",c + 1);
   tcl_return = Tcl_SetVar(tcl_interp, cmd_el, this_el, 0);
  }
  this_book_ptr->current_search.nelements_done = 0;
  this_book_ptr->current_search.nelements_found = spec_num2;
 } else {
  Tcl_SetVar(tcl_interp, "sgReturned", "0", 0);
  Tcl_SetVar(tcl_interp, "sgFound", "0", 0);
  str=Tcl_Alloc(strlen("0") + 1);
  sprintf(str, "0");
  Tcl_SetResult(tcl_interp, str, TCL_DYNAMIC); 
 }
 apr_pool_destroy(current_p);
 return check_process_book(this_book_ptr, this_style_ptr, "findSGElement", __FILE__, __LINE__);
}


//finds the element, returning the id of the first one found and all found in a tcl array
//revised 0202 to use low memory index model
int get_find_sg_text(ClientData client_data, Tcl_Interp* tcl_interp,  int inargc,  char **inargv) {
 const char *tcl_return;
 int c=0, found=0, nresults=1, spec_num1=1, spec_num2=1;
 int d=0;
 int sstrlen = 0, sspos=0;
 char *search_string;
 char this_el[512], cmd_el[512], *str;
 pool *current_p;
 const char *this_cmd;
 process_book_ptr this_book_ptr = NULL;
 style_ptr this_style_ptr = NULL;
 create_process_book(&this_book_ptr, &this_style_ptr, tcl_interp, inargv[1], NULL, NULL, NULL);
 this_cmd = (const char *) Tcl_GetVar(tcl_interp, "$current_command", 0);
 apr_pool_create(&current_p, this_style_ptr->style_pool);
 if (this_book_ptr==NULL || this_style_ptr==NULL) {
  write_log("Error processing Anastasia style TCL extension function \"findSGText\". Critical Error!.");
  return TCL_OK;
 }
 tcl_return=NULL;
 Tcl_ResetResult(tcl_interp);
 if ((inargc<2) || (inargc>5)) {
  send_ana_param_error(this_style_ptr, "findSGElement", "two, three, or four", "one for the book name, one for the SGREP search string, and two optional elements for number of first hit to return and number of hits to return", this_book_ptr);
  return check_process_book(this_book_ptr, this_style_ptr, "findSGText", __FILE__, __LINE__);
 }
 if ((!this_book_ptr) || (!this_book_ptr->map_idx)) {
                write_error_output(this_style_ptr, this_book_ptr, apr_psprintf(current_p, "No index or sgrep file for book \"%s\" currently loaded, in \"%s\" call from file \"%s\".  Search function findSGElement will fail.", this_book_ptr->bookname, this_cmd, this_style_ptr->file_name));
  return check_process_book(this_book_ptr, this_style_ptr, "findSGText", __FILE__, __LINE__);
 }
 //fill in strings, if any
 if ((inargc>3) && (!fill_in_nums(inargc, inargv,&spec_num1, &spec_num2, "findSGText", this_style_ptr, this_book_ptr))) {
  return check_process_book(this_book_ptr, this_style_ptr, "findSGText", __FILE__, __LINE__);
 }
 if (spec_num2>MAXSEARCHRESULTS) {
                write_error_output(this_style_ptr, this_book_ptr, apr_psprintf(current_p, "Error in findSGText call for book \"%s\" currently loaded, in \"%s\" call from file \"%s\".  %i results requested; maximum permissible hits for any one search is %i.", this_book_ptr->bookname, this_cmd, this_style_ptr->file_name, spec_num2, MAXSEARCHRESULTS));
  spec_num2=MAXSEARCHRESULTS;
 }
  //do the search! specnum2 comes back with number actually put in results string, nresults total number found -- nresults also is found!
 //spec_num1 is the start number, spec_num2 is the number of search results
 //O.k. before we go into this function we need to convert the search string.. 
 //The might be some utf8 characters in there so we need them in the format \#1234; or \#x1D3D;
 sstrlen = strlen(inargv[2]);
 search_string = (char *) apr_palloc(current_p, (sizeof(char) * sstrlen * 10));
 for (c=0; c<sstrlen; c++) {
  char chr = inargv[2][c];
  char tstr[10];
  long num = 0;
  int found_utf8 = false;
  //Check the character
  if ((chr&0xF8)==0xF8) {
   //5 byte character
   if (((inargv[2][c+1]&0x80)==0x80) && ((inargv[2][c+2]&0x80)==0x80) && ((inargv[2][c+5]&0x80)==0x80) && ((inargv[2][c+4]&0x80)==0x80)) {
    //Yep where find so convert.
    num = (((chr&0x3)<<24)+((inargv[2][c+1]&0x3F)<<18)+((inargv[2][c+2]&0x3F)<<12)+((inargv[2][c+3]&0x3F)<<6)+(inargv[2][c+4]&0x3F));
    found_utf8 = true;
    c+=4;
   }
  } else if ((chr&0xF0)==0xF0) {
   //4 byte character
   if (((inargv[2][c+1]&0x80)==0x80) && ((inargv[2][c+2]&0x80)==0x80) && ((inargv[2][c+5]&0x80)==0x80)) {
    //Yep where find so convert.
    num = (((chr&0x7)<<18)+((inargv[2][c+1]&0x3F)<<12)+((inargv[2][c+2]&0x3F)<<6)+(inargv[2][c+3]&0x3F));
    found_utf8 = true;
    c+=3;
   }
  } else if ((chr&0xE0)==0xE0) {
   //3 byte character
   if (((inargv[2][c+1]&0x80)==0x80) && ((inargv[2][c+2]&0x80)==0x80)) {
    //Yep where find so convert.
    num = (((chr&0xF)<<12)+((inargv[2][c+1]&0x3F)<<6)+(inargv[2][c+2]&0x3F));
    found_utf8 = true;
    c+=2;
   }
  } else if ((chr&0xC0)==0xC0) {
   //2 byte character
   if ((inargv[2][c+1]&0x80)==0x80) {
    //Yep where find so convert.
    num = (((chr&0x1F)<<6)+(inargv[2][c+1]&0x3F));
    found_utf8 = true;
    c+=1;
   }
  }
  if (found_utf8) {
   sprintf(tstr, "\\#x%x;", (unsigned int) num);
   //Now copy over those characters
   for (d=0; d<strlen(tstr); d++) {
    search_string[sspos++] = tstr[d];
   }
  } else {
   search_string[sspos++] = chr;
  }
 }
 if (search_string==NULL) {
  search_string = apr_pstrdup(current_p, "");
 }
 search_string[sspos] = '\0';
 found = do_find_sg_text(search_string, spec_num1, &spec_num2, &nresults, this_book_ptr, current_p);
 if (found) {
  struct bounds_obj the_return;
  sprintf(this_el,"%d", spec_num2);
  Tcl_SetVar(tcl_interp, "sgtReturned", this_el, 0);
  sprintf(this_el,"%d", found);
  Tcl_SetVar(tcl_interp, "sgtFound", this_el, 0);
  //now we get the elements found out of the array and stick them in the array
  for (c=0; c<spec_num2; c++) { 
   bounds_ptr the_return= (bounds_ptr) (this_book_ptr->search_texts_found)+c;
//   write_log(apr_psprintf(current_p, "found text start %d", (the_return)->start_el));
   sprintf(this_el,"%d", (the_return)->start_el);
   sprintf(cmd_el,"tsBegEl(%d)",c + 1);
   tcl_return = Tcl_SetVar(tcl_interp, cmd_el, this_el, 0);
   sprintf(this_el,"%d", (the_return)->start_off);
   sprintf(cmd_el,"tsBegTxt(%d)",c + 1);
   tcl_return = Tcl_SetVar(tcl_interp, cmd_el, this_el, 0);
   sprintf(this_el,"%d", (the_return)->end_el);   
   sprintf(cmd_el,"tsEndEl(%d)",c + 1);
   tcl_return = Tcl_SetVar(tcl_interp, cmd_el, this_el, 0);
   sprintf(this_el,"%d", (the_return)->end_off);
   sprintf(cmd_el,"tsEndTxt(%d)",c + 1);
   tcl_return = Tcl_SetVar(tcl_interp, cmd_el, this_el, 0); 
  }
  this_book_ptr->in_search = 1;
  this_book_ptr->current_search.ntexts_done = 0;
  this_book_ptr->current_search.ntexts_found = spec_num2;
  str=Tcl_Alloc(strlen("0") + 1);
  sprintf(str, "0");
  Tcl_SetResult(tcl_interp, str, TCL_DYNAMIC); 
 } else {
  Tcl_SetVar(tcl_interp, "sgtReturned", "0", 0);
  Tcl_SetVar(tcl_interp, "sgtFound", "0", 0);
  str=Tcl_Alloc(strlen("0") + 1);
  sprintf(str, "0");
  Tcl_SetResult(tcl_interp, str, TCL_DYNAMIC); 
 }
 return check_process_book(this_book_ptr, this_style_ptr, "findSGText", __FILE__, __LINE__);
}

int get_count_sg(ClientData client_data, Tcl_Interp* tcl_interp,  int inargc,  char **inargv) {
 int found=0, c=0;
 int d=0;
 int sstrlen = 0, sspos=0;
 char *search_string;
 char *tcl_return, *book=inargv[1], this_el[512];
 pool  *current_p;
 process_book_ptr this_book_ptr = NULL;
 style_ptr this_style_ptr = NULL;
 create_process_book(&this_book_ptr, &this_style_ptr, tcl_interp, inargv[1], NULL, NULL, NULL);
 if (this_book_ptr==NULL || this_style_ptr==NULL) {
  write_log("Error processing Anastasia style TCL extension function \"countSG\". Critical Error!.");
  return TCL_OK;
 }
 apr_pool_create(&current_p, this_style_ptr->style_pool);
 Tcl_ResetResult(tcl_interp);
 if (inargc!=3)  {
  send_ana_param_error(this_style_ptr, "countSG", "two", "one for the name of the book to search, one for the SGREP search string", this_book_ptr);
  tcl_return=Tcl_Alloc(strlen("0") + 1);
  sprintf(tcl_return, "0");
  Tcl_SetResult(tcl_interp, tcl_return, TCL_DYNAMIC);
  return check_process_book(this_book_ptr, this_style_ptr, "countSG", __FILE__, __LINE__);
 }
 if ((!this_book_ptr) || (!this_book_ptr->map_idx)) {
                write_error_output(this_style_ptr, this_book_ptr,apr_psprintf(current_p, "No index file for book \"%s\" currently loaded, in call from file \"%s\".  Search function countSG will fail.", book, this_style_ptr->file_name));
  tcl_return=Tcl_Alloc(strlen("0") + 1);
  sprintf(tcl_return, "0");
  Tcl_SetResult(tcl_interp, tcl_return, TCL_DYNAMIC);
  apr_pool_destroy(current_p);
  return check_process_book(this_book_ptr, this_style_ptr, "countSG", __FILE__, __LINE__);
 }
 //O.k. before we go into this function we need to convert the search string.. 
 //The might be some utf8 characters in there so we need them in the format \#1234; or \#x1D3D;
 sstrlen = strlen(inargv[2]);
 search_string = (char *) apr_palloc(current_p, (sizeof(char) * sstrlen * 10));
 for (c=0; c<sstrlen; c++) {
  char chr = inargv[2][c];
  char tstr[10];
  long num = 0;
  int found_utf8 = false;
  //Check the character
  if ((chr&0xF8)==0xF8) {
   //5 byte character
   if (((inargv[2][c+1]&0x80)==0x80) && ((inargv[2][c+2]&0x80)==0x80) && ((inargv[2][c+5]&0x80)==0x80) && ((inargv[2][c+4]&0x80)==0x80)) {
    //Yep where find so convert.
    num = (((chr&0x3)<<24)+((inargv[2][c+1]&0x3F)<<18)+((inargv[2][c+2]&0x3F)<<12)+((inargv[2][c+3]&0x3F)<<6)+(inargv[2][c+4]&0x3F));
    found_utf8 = true;
    c+=4;
   }
  } else if ((chr&0xF0)==0xF0) {
   //4 byte character
   if (((inargv[2][c+1]&0x80)==0x80) && ((inargv[2][c+2]&0x80)==0x80) && ((inargv[2][c+5]&0x80)==0x80)) {
    //Yep where find so convert.
    num = (((chr&0x7)<<18)+((inargv[2][c+1]&0x3F)<<12)+((inargv[2][c+2]&0x3F)<<6)+(inargv[2][c+3]&0x3F));
    found_utf8 = true;
    c+=3;
   }
  } else if ((chr&0xE0)==0xE0) {
   //3 byte character
   if (((inargv[2][c+1]&0x80)==0x80) && ((inargv[2][c+2]&0x80)==0x80)) {
    //Yep where find so convert.
    num = (((chr&0xF)<<12)+((inargv[2][c+1]&0x3F)<<6)+(inargv[2][c+2]&0x3F));
    found_utf8 = true;
    c+=2;
   }
  } else if ((chr&0xC0)==0xC0) {
   //2 byte character
   if ((inargv[2][c+1]&0x80)==0x80) {
    //Yep where find so convert.
    num = (((chr&0x1F)<<6)+(inargv[2][c+1]&0x3F));
    found_utf8 = true;
    c+=1;
   }
  }
  if (found_utf8) {
   sprintf(tstr, "\\#x%x;", (unsigned int) num);
   //Now copy over those characters
   for (d=0; d<strlen(tstr); d++) {
    search_string[sspos++] = tstr[d];
   }
  } else {
   search_string[sspos++] = chr;
  }
 }
 if (search_string==NULL) {
  search_string = apr_pstrdup(current_p, "");
 }
 search_string[sspos] = '\0';
 found = do_count_sg(search_string, this_book_ptr, current_p);
 if (found==-1) found=0;
 sprintf(this_el,"%d", found);
 tcl_return=Tcl_Alloc(strlen(this_el) + 1);
 strcpy(tcl_return, this_el);
 Tcl_SetResult(tcl_interp, tcl_return, TCL_DYNAMIC);
 apr_pool_destroy(current_p);
 return check_process_book(this_book_ptr, this_style_ptr, "countSG", __FILE__, __LINE__);
}


int get_ana_content(ClientData client_data, Tcl_Interp* tcl_interp, int argc, char **argv) {
 char *el_num_str=argv[1], *tcl_return, *spec_str;
 int spec_num;
 int got_str, got_num;
 sgml_obj_ptr this_sgml_el;
 pool  *current_p;
 process_book_ptr this_book_ptr = NULL;
 style_ptr this_style_ptr = NULL;
 create_process_book(&this_book_ptr, &this_style_ptr, tcl_interp, NULL, NULL, NULL, NULL);
 if (this_book_ptr==NULL || this_style_ptr==NULL) {
  write_log("Error processing Anastasia style TCL extension function \"content\". Critical Error!.");
  return TCL_OK;
 }
 apr_pool_create(&current_p, this_style_ptr->style_pool);
 got_str=got_num=0;
 Tcl_ResetResult(tcl_interp);
 if ((argc<2) || (argc>4)) {
  send_ana_param_error(this_style_ptr, "content", "one, two, or three", "one for the element identifier, one for the maximum number of characters to be returned, one for the .anv style file to format the content", this_book_ptr);
  apr_pool_destroy(current_p);
  return check_process_book(this_book_ptr, this_style_ptr, "content", __FILE__, __LINE__);
 }
 //fill in strings, if any
 if ((argc>2) && (!fill_in_specs(argc, argv, &got_str, &got_num, &spec_str, &spec_num, "content", this_style_ptr, this_book_ptr))) {
  apr_pool_destroy(current_p);
  return check_process_book(this_book_ptr, this_style_ptr, "content", __FILE__, __LINE__);
 }
 if (!is_valid_el(el_num_str, "content", &this_sgml_el, this_book_ptr, this_style_ptr)) {
  apr_pool_destroy(current_p);
  return check_process_book(this_book_ptr, this_style_ptr, "content", __FILE__, __LINE__);
 }
 if ((this_sgml_el->cont_type)==0) {
  apr_pool_destroy(current_p);
  return check_process_book(this_book_ptr, this_style_ptr, "content", __FILE__, __LINE__);
 }
 //ok, so we have some content.  Let's get it
 //case one: unqualified element.  Just read in the data from start to end and write it out
 //maximum read in: 1024 chars
 if (!got_str) {
  size_t getlen=this_sgml_el->end_data-this_sgml_el->start_data;
  getlen=(getlen>1024)?1024:getlen;
  if (got_num)
   getlen=(getlen>(unsigned int) spec_num)?(unsigned int) spec_num:getlen;
  tcl_return=Tcl_Alloc(getlen + 1);
  apr_off_t _where = (this_sgml_el)->start_data;
  apr_file_seek(this_book_ptr->in_sgv, APR_SET, &_where);
  apr_size_t _count = sizeof(char) * getlen;
  apr_file_read(this_book_ptr->in_sgv, tcl_return, &_count);
  if (_count != (sizeof(char) * getlen)) {
//    write_log("Error reading Anastasia .sgv file.\n");
    apr_pool_destroy(current_p);
    return check_process_book(this_book_ptr, this_style_ptr, "content", __FILE__, __LINE__);
  }
  tcl_return[getlen]='\0';
  Tcl_SetResult(tcl_interp, tcl_return, TCL_DYNAMIC);
  apr_pool_destroy(current_p);
  return check_process_book(this_book_ptr, this_style_ptr, "content", __FILE__, __LINE__);
 }
 //case two: qualified element.  We have got to find an .anv file, create an interpreter, format the 
 //content according to the .anv file.  In the meantime, we have to guard against recursion:
 //so: need to put maximum num chars here to deal with it.  Maximum default is set to 3072.
 //this can be overruled.
 else {
  //first: get the .anv file and make a new style interpreter for it (if necessary)
  //note that we have to copy universals (INCLUDING values for text hide: these could get reset) 
  style_ptr content_style;
  buffer_ptr content_fspec;
  pool *content_pool;
  int last_el;
  int getlen=3072;
  int in_search=0,orig_hide=this_book_ptr->hide;
  apr_pool_create(&content_pool, this_book_ptr->current_pool);
  if (!init_output_buffer(&content_fspec, this_book_ptr, current_p)) {
   apr_pool_destroy(current_p);
   return check_process_book(this_book_ptr, this_style_ptr, "content", __FILE__, __LINE__);
  }
  if (this_book_ptr==NULL) {
//   write_log("this_book_ptr is null");
  }
//   write_log("diong element 1b");
  if (!make_new_tcl_style(spec_str, this_book_ptr->bookname, &content_style, content_fspec, this_book_ptr, false, false)) {
   delete_output_buffer(&content_fspec, this_book_ptr);
   apr_pool_destroy(current_p);
   return check_process_book(this_book_ptr, this_style_ptr, "content", __FILE__, __LINE__);
  }
//  write_log("diong element 1c");
  in_search = this_book_ptr->in_search;
  this_book_ptr->in_search = 0;
  //Reset the hide so we actually get something printed out!
  this_book_ptr->hide = 0;
     //Put a pointer the current_books array into the interp.
    if (content_style->outputTOC)  {
   output_ana_toc(0, content_style->outputTOC, 0, content_style, this_book_ptr);
  } else {
   output_element(this_sgml_el->number, false, false, content_style, &last_el, NULL, this_book_ptr);
  }
  this_book_ptr->in_search = in_search;
  //if length is specified: use specified number.  Else, leave at default of 3072
  if (got_num) getlen=spec_num;
  getlen=(getlen<content_style->file_spec->len)?getlen:content_style->file_spec->len;
  tcl_return=Tcl_Alloc(getlen);
                if (content_style==NULL || content_style->file_spec==NULL || content_style->file_spec->ptr==NULL) {
                    write_error_output(this_style_ptr, this_book_ptr, "Warning something went wrong in this content call.");
                } else {
                    strncpy(tcl_return, content_style->file_spec->ptr, getlen-1);
                }
  tcl_return[getlen-1]='\0';
  Tcl_SetResult(tcl_interp, tcl_return, TCL_DYNAMIC);
  delete_output_buffer(&content_fspec, this_book_ptr);
  this_book_ptr->hide = orig_hide;
 }
 apr_pool_destroy(current_p);
 return check_process_book(this_book_ptr, this_style_ptr, "content", __FILE__, __LINE__);
}

int get_ana_content_type(ClientData client_data, Tcl_Interp* tcl_interp, int argc, char **argv) {
 char *el_num_str=argv[1];
 char *tcl_return;
 sgml_obj_ptr this_sgml_el;
 process_book_ptr this_book_ptr = NULL;
 style_ptr this_style_ptr = NULL;
 create_process_book(&this_book_ptr, &this_style_ptr, tcl_interp, NULL, NULL, NULL, NULL);
 if (this_book_ptr==NULL || this_style_ptr==NULL) {
  write_log("Error processing Anastasia style TCL extension function \"content_type\". Critical Error!.");
  return TCL_OK;
 }
 tcl_return=NULL;
 Tcl_ResetResult(tcl_interp);
 if (argc!=2)  {
  send_ana_param_error(this_style_ptr, "contentType", "one", "for the element identifier", this_book_ptr);
  return check_process_book(this_book_ptr, this_style_ptr, "contentType", __FILE__, __LINE__);
 }
 if (!is_valid_el(el_num_str, "contentType", &this_sgml_el, this_book_ptr, this_style_ptr))
  return check_process_book(this_book_ptr, this_style_ptr, "contentType", __FILE__, __LINE__);
 switch (this_sgml_el->cont_type) {
  case 0:
   tcl_return=Tcl_Alloc(strlen("empty") + 1);
   sprintf(tcl_return, "empty");
   break;
  case 1:
   tcl_return=Tcl_Alloc(strlen("cdata") + 1);
   sprintf(tcl_return, "cdata");
   break;
  case 2:
   tcl_return=Tcl_Alloc(strlen("rcdata") + 1);
   sprintf(tcl_return, "rcdata");
   break;
  case 3:
   tcl_return=Tcl_Alloc(strlen("mixed") + 1);
   sprintf(tcl_return, "mixed");
   break;
  case 4:
   tcl_return=Tcl_Alloc(strlen("element") + 1);
   sprintf(tcl_return, "element");
   break;
  case 5:  //this is an addition to J Clarke's types: = #PCDATA
   tcl_return=Tcl_Alloc(strlen("#PCDATA") + 1);
   sprintf(tcl_return, "#PCDATA");
   break;
  case 6:  //this is an addition to J Clarke's types: = #SDATA
   tcl_return=Tcl_Alloc(strlen("#SDATA") + 1);
   sprintf(tcl_return, "#SDATA");
   break;
  default:
   tcl_return=Tcl_Alloc(1);
                        tcl_return[0] = '\0';
   break;
 }
 Tcl_SetResult(tcl_interp, tcl_return, TCL_DYNAMIC);
 return check_process_book(this_book_ptr, this_style_ptr, "contentType", __FILE__, __LINE__);
}

//extension routine to enable puts in style subinterpreters: they can write to the 
//main interpreters error window with this
//use globals 
int ana_puts(ClientData client_data, Tcl_Interp* tcl_interp, int argc, char **argv) {
 char *err_message=argv[1];
 pool *current_pool;
 char *tcl_return;
 process_book_ptr this_book_ptr = NULL;
 style_ptr this_style_ptr = NULL;
 create_process_book(&this_book_ptr, &this_style_ptr, tcl_interp, NULL, NULL, NULL, NULL);
 if (this_book_ptr==NULL || this_style_ptr==NULL) {
  write_log("Error processing Anastasia style TCL extension function \"puts\". Critical Error!.");
  return TCL_OK;
 }
 tcl_return=NULL;
 apr_pool_create(&current_pool, this_book_ptr->current_pool);
 if (argc!=2) {
                write_error_output(this_style_ptr, this_book_ptr, apr_psprintf(current_pool, "Error processing Anastasia style TCL extension function \"puts\" in style file \"%s\".: this function requires one parameter, a string for the message to be written.", this_style_ptr->file_name));
  apr_pool_destroy(current_pool);
  return check_process_book(this_book_ptr, this_style_ptr, "puts", __FILE__, __LINE__);
 }
        write_error_output(this_style_ptr, this_book_ptr, err_message);
 apr_pool_destroy(current_pool);
 return check_process_book(this_book_ptr, this_style_ptr, "puts", __FILE__, __LINE__);
}

int get_next_chars(ClientData client_data, Tcl_Interp* tcl_interp, int argc, char **argv) {
 char *el_num_str=argv[1];
 char *offset_str=argv[2];
 char *nchars_str=argv[3];
 char *tcl_return;
 sgml_obj_ptr this_sgml_el;
 unsigned int offset;
 unsigned int nchars;
 size_t f_size;
 pool *current_pool;
 process_book_ptr this_book_ptr = NULL;
 style_ptr this_style_ptr = NULL;
 create_process_book(&this_book_ptr, &this_style_ptr, tcl_interp, NULL, NULL, NULL, NULL);
 if (this_book_ptr==NULL || this_style_ptr==NULL) {
  write_log("Error processing Anastasia style TCL extension function \"nextChars\". Critical Error!.");
  return TCL_OK;
 }
 tcl_return=NULL;
 apr_pool_create(&current_pool, this_book_ptr->current_pool);
 Tcl_ResetResult(tcl_interp);
 if ((argc<4) || (argc>5)) {
  send_ana_param_error(this_style_ptr, "nextChars", "three or four", "one for the beginning element identifier, one for the offset within the element, one for the number of text characters to get, and an optional parameter for the style file to use to process this", this_book_ptr);
  return check_process_book(this_book_ptr, this_style_ptr, "nextChars", __FILE__, __LINE__);
 }
 if (!is_valid_el(el_num_str, "nextChars", &this_sgml_el, this_book_ptr, this_style_ptr)) {
  return check_process_book(this_book_ptr, this_style_ptr, "nextChars", __FILE__, __LINE__);
 }
 //let's look at each parameter...
 if (!valid_num_param(offset_str, "second", "nextChars", (int *) &offset, this_style_ptr, this_book_ptr)) {
  return check_process_book(this_book_ptr, this_style_ptr, "nextChars", __FILE__, __LINE__);
 }
 if (!valid_num_param(nchars_str, "third", "nextChars", (int *) &nchars, this_style_ptr, this_book_ptr)) {
  return check_process_book(this_book_ptr, this_style_ptr, "nextChars", __FILE__, __LINE__);
 }
 //now, if there are only three parameters, we don't have a style file.
 //just get the characters out of the sgv file and that'll do us
 if (argc==4) {
  int start;
  int c;
  char *temp_txt= (char *) ana_malloc(&(this_book_ptr->anastasia_memory), (nchars * sizeof(char)) +1, __FILE__, __LINE__);
  if (offset==-1) {
   start=(this_sgml_el)->end_data;
  } else {
   start=(this_sgml_el)->start_data + offset;
  }
  //check that we don't try and read past the end of the file..
  apr_off_t _where = 0;
  apr_file_seek(this_book_ptr->in_sgv, SEEK_END, &_where);
  f_size=_where;
  if (f_size<start + nchars) {
   nchars=f_size-start;
  }
  _where = start;
  apr_file_seek(this_book_ptr->in_sgv, SEEK_SET, &_where);
  apr_size_t _count = sizeof(char) * nchars;
  apr_file_read(this_book_ptr->in_sgv, temp_txt, &_count);
  if (_count != (sizeof(char) * nchars)) {
//   write_log("Error reading Anastasia .sgv file.\n");
   ana_free(&(this_book_ptr->anastasia_memory), temp_txt);
   return check_process_book(this_book_ptr, this_style_ptr, "nextChars", __FILE__, __LINE__);
  }
  temp_txt[nchars]='\0';
  //so now: look back from end till we find white space
  for (c=nchars;  !isspace(temp_txt[c]) && c>0;c--);
  if (c>0) {
   temp_txt[c]='\0';
   tcl_return=Tcl_Alloc(c  + 1);
   strncpy(tcl_return, temp_txt, c + 1);
   Tcl_SetResult(tcl_interp, tcl_return, TCL_DYNAMIC);
  }
 }
  //there is a style file. Find it and use it.
 else if (argc==5)  {
  char *spec_str=argv[4];
  int orig_hide = this_book_ptr->hide, orig_search = this_book_ptr->in_search;
  struct bounds_obj bounds;
  this_book_ptr->hide = 0;
  this_book_ptr->in_search = 0;
  calc_next_chars(&bounds, offset, this_sgml_el, nchars, this_book_ptr);
  if (!format_arbitrary_span(spec_str, &tcl_return, &bounds, 2, this_book_ptr, current_pool)) {
   apr_pool_destroy(current_pool);
   this_book_ptr->hide = orig_hide;
   return check_process_book(this_book_ptr, this_style_ptr, "nextChars", __FILE__, __LINE__);
  }
  this_book_ptr->hide = orig_hide;
  this_book_ptr->in_search = orig_search;
 }
 Tcl_SetResult(tcl_interp, tcl_return, TCL_DYNAMIC);
 apr_pool_destroy(current_pool);
 return check_process_book(this_book_ptr, this_style_ptr, "nextChars", __FILE__, __LINE__);
}

int get_these_chars(ClientData client_data, Tcl_Interp* tcl_interp, int argc, char **argv) {
 char *el_num1_str=argv[1], *offset1_str=argv[2], *el_num2_str=argv[3], *offset2_str=argv[4];
 char *tcl_return;
 sgml_obj_ptr this_sgml_el1, this_sgml_el2;
 int start, end;
 int offset1, offset2;
 pool *current_pool;
 process_book_ptr this_book_ptr = NULL;
 style_ptr this_style_ptr = NULL;
 create_process_book(&this_book_ptr, &this_style_ptr, tcl_interp, NULL, NULL, NULL, NULL);
 if (this_book_ptr==NULL || this_style_ptr==NULL) {
  write_log("Error processing Anastasia style TCL extension function \"theseChars\". Critical Error!.");
  return TCL_OK;
 }
 apr_pool_create(&current_pool, this_book_ptr->current_pool);
 Tcl_ResetResult(tcl_interp);
 if ((argc<5) || (argc>6)) {
  send_ana_param_error(this_style_ptr, "theseChars", "four or five", "one for the beginning element identifier, one for the offset within the beginning element, one for the ending element identifier, one for the offset within the ending element, and an optional parameter for the style file to use to process this", this_book_ptr);
  apr_pool_destroy(current_pool);
  return check_process_book(this_book_ptr, this_style_ptr, "theseChars", __FILE__, __LINE__);
 }
 if (!is_valid_el(el_num1_str, "theseChars", &this_sgml_el1, this_book_ptr, this_style_ptr)) {
  apr_pool_destroy(current_pool);
  return check_process_book(this_book_ptr, this_style_ptr, "theseChars", __FILE__, __LINE__);
 }
 if (!is_valid_el(el_num2_str, "theseChars", &this_sgml_el2, this_book_ptr, this_style_ptr)) {
  apr_pool_destroy(current_pool);
  return check_process_book(this_book_ptr, this_style_ptr, "theseChars", __FILE__, __LINE__);
 }
 //let's look at each parameter...
 if (!valid_num_param(offset1_str, "second", "theseChars", &offset1, this_style_ptr, this_book_ptr)) {
  apr_pool_destroy(current_pool);
  return check_process_book(this_book_ptr, this_style_ptr, "theseChars", __FILE__, __LINE__);
 }
 if (!valid_num_param(offset2_str, "fourth", "theseChars", &offset2, this_style_ptr, this_book_ptr)) {
  apr_pool_destroy(current_pool);
  return check_process_book(this_book_ptr, this_style_ptr, "theseChars", __FILE__, __LINE__);
 }
 if (offset1==-1) start=(this_sgml_el1)->end_data;
   else start=(this_sgml_el1)->start_data + offset1;
 if (offset2==-1) end=(this_sgml_el2)->end_data;
   else end=(this_sgml_el2)->start_data + offset2;
  //range from start to end will be what we will read
  //check that we don't have a minus value, ie smarty has put the end before the beginning (Zen maybe?..)
  if (end<start) {
                write_error_output(this_style_ptr, this_book_ptr, apr_psprintf(this_style_ptr->style_pool, "Error in \"theseChars\" command in call from file \"%s\": the offset %i in beginning element %s comes after the offset %i in ending element %s. (-1 offset=end of the elememt)\n", this_style_ptr->file_name, offset1, el_num1_str, offset2, el_num2_str ));
  apr_pool_destroy(current_pool);
  return check_process_book(this_book_ptr, this_style_ptr, "theseChars", __FILE__, __LINE__);
 }
 //now, if there are only four parameters, we don't have a style file.
 //just get the characters out of the sgv file and that'll do us
 if (argc==5) {
  char *temp_txt;
  //Need to recalculate the exact positions
  temp_txt= (char *) ana_malloc(&(this_book_ptr->anastasia_memory), (end-start * sizeof(char)) +1, __FILE__, __LINE__);
  if (temp_txt==NULL) {
   char *this_out=apr_psprintf(this_style_ptr->style_pool, "Error in \"theseChars\" command in call from file \"%s\": Error allocating memory.", this_style_ptr->file_name);
//   write_log(this_out);
   apr_pool_destroy(current_pool);
   return check_process_book(this_book_ptr, this_style_ptr, "theseChars", __FILE__, __LINE__);
  }
  apr_off_t _where = start;
  apr_file_seek(this_book_ptr->in_sgv, APR_SET, &_where);
  apr_size_t _count = sizeof(char) * end-start;
  apr_file_read(this_book_ptr->in_sgv, temp_txt, &_count);
  if (_count != (sizeof(char) * end-start)) {
//   write_log("Error reading Anastasia .sgv file.\n");
   ana_free(&(this_book_ptr->anastasia_memory), temp_txt);
   return check_process_book(this_book_ptr, this_style_ptr, "theseChars", __FILE__, __LINE__);
  }
  temp_txt[end-start]='\0';
  tcl_return=Tcl_Alloc(end - start  + 1);
  strncpy(tcl_return, temp_txt, end - start  + 1);
 }
 //we got a style file!!!
 if (argc==6) {
  char *spec_str=argv[5];
  struct bounds_obj bounds;
  int orig_hide = this_book_ptr->hide;
  int orig_search = this_book_ptr->in_search;
  this_book_ptr->hide = 0;
  this_book_ptr->in_search = 0;
  calc_these_chars(&bounds, offset1, offset2, this_sgml_el1, this_sgml_el2, this_book_ptr);
  if (!format_arbitrary_span(spec_str, &tcl_return, &bounds, 1, this_book_ptr, current_pool)) {
   apr_pool_destroy(current_pool);
   this_book_ptr->hide = orig_hide;
   return check_process_book(this_book_ptr, this_style_ptr, "theseChars", __FILE__, __LINE__);
  }
  this_book_ptr->hide = orig_hide;
  this_book_ptr->in_search = orig_search;
 }
 Tcl_SetResult(tcl_interp, tcl_return, TCL_DYNAMIC);
 apr_pool_destroy(current_pool);
 return check_process_book(this_book_ptr, this_style_ptr, "theseChars", __FILE__, __LINE__);
}
 
int get_prev_chars(ClientData client_data, Tcl_Interp* tcl_interp, int argc, char **argv) {
 char *el_num_str=argv[1], *offset_str=argv[2], *nchars_str=argv[3];
 char *tcl_return=NULL;
 sgml_obj_ptr this_sgml_el;
 int offset, nchars;
 pool *current_pool;
 process_book_ptr this_book_ptr = NULL;
 style_ptr this_style_ptr = NULL;
 create_process_book(&this_book_ptr, &this_style_ptr, tcl_interp, NULL, NULL, NULL, NULL);
 if (this_book_ptr==NULL || this_style_ptr==NULL) {
  write_log("Error processing Anastasia style TCL extension function \"prevChars\". Critical Error!.");
  return TCL_OK;
 }
 apr_pool_create(&current_pool, this_style_ptr->style_pool);
 Tcl_ResetResult(tcl_interp);
 if ((argc<4) || (argc>5)) {
  send_ana_param_error(this_style_ptr, "prevChars", "three or four", "one for the beginning element identifier, one for the offset within the element, one for the number of text characters to get, and an optional parameter for the style file to use to process this", this_book_ptr);
  apr_pool_destroy(current_pool);
  return check_process_book(this_book_ptr, this_style_ptr, "prevChars", __FILE__, __LINE__);
 }
 if (!is_valid_el(el_num_str, "prevChars", &this_sgml_el, this_book_ptr, this_style_ptr)) {
  apr_pool_destroy(current_pool);
  return check_process_book(this_book_ptr, this_style_ptr, "prevChars", __FILE__, __LINE__);
 }
 //let's look at each parameter...
 if (!valid_num_param(offset_str, "second", "prevChars", &offset, this_style_ptr, this_book_ptr)) {
  apr_pool_destroy(current_pool);
  return check_process_book(this_book_ptr, this_style_ptr, "prevChars", __FILE__, __LINE__);
 }
 if (!valid_num_param(nchars_str, "third", "prevChars", &nchars, this_style_ptr, this_book_ptr)) {
  apr_pool_destroy(current_pool);
  return check_process_book(this_book_ptr, this_style_ptr, "prevChars", __FILE__, __LINE__);
 }
 //now, if there are only three parameters, we don't have a style file.
 //just get the characters out of the sgv file and that'll do us
 if (argc==4) {
   int start;
   int c;
   char *temp_txt;
   if (offset==-1) start=(this_sgml_el)->end_data - nchars;
   else start=(this_sgml_el)->start_data + offset -  nchars;
   if (start<0) {
    nchars=offset;  //just read up to offset then
    start=0;  //try to read before the beginning of the file .. disaster
  }
  temp_txt = (char *) ana_malloc(&(this_book_ptr->anastasia_memory), (nchars * sizeof(char)) +1, __FILE__, __LINE__);
  apr_off_t _where = start;
  apr_file_seek(this_book_ptr->in_sgv, APR_SET, &_where);
  apr_size_t _count = sizeof(char) * nchars;
  apr_file_read(this_book_ptr->in_sgv, temp_txt, &_count);
  if (_count != nchars) {
//   write_log("Error reading Anastasia .sgv file.\n");
   ana_free(&(this_book_ptr->anastasia_memory), temp_txt);
   return check_process_book(this_book_ptr, this_style_ptr, "prevChars", __FILE__, __LINE__);
  }
  temp_txt[nchars]='\0';
  //so now: read from start till we find white space
  for (c=0;  !isspace(temp_txt[c]) && c<=nchars; c++);
  if (c<nchars - 2) { //
   tcl_return=Tcl_Alloc(nchars - c);
   strncpy(tcl_return, temp_txt + c + 1, nchars - c);
  } else {
   tcl_return=Tcl_Alloc(nchars + 1);
   strncpy(tcl_return, temp_txt , nchars + 1);
  }
 }
 //there is a style file. Find it and use it.
 else if (argc==5)  {
  char *spec_str=argv[4];
  struct bounds_obj bounds;
  int orig_hide = this_book_ptr->hide;
  int orig_search = this_book_ptr->in_search;
  this_book_ptr->hide = 0;
  this_book_ptr->in_search = 0;
  calc_prev_chars(&bounds, offset, this_sgml_el, nchars, this_book_ptr);
  if (!format_arbitrary_span(spec_str, &tcl_return, &bounds, 0, this_book_ptr, current_pool)) {
   apr_pool_destroy(current_pool);
   this_book_ptr->hide = orig_hide;
   return check_process_book(this_book_ptr, this_style_ptr, "prevChars", __FILE__, __LINE__);
  }
  this_book_ptr->hide = orig_hide;
  this_book_ptr->in_search = orig_search;
 }
 Tcl_SetResult(tcl_interp, tcl_return, TCL_DYNAMIC);
 apr_pool_destroy(current_pool);
 return check_process_book(this_book_ptr, this_style_ptr, "prevChars", __FILE__, __LINE__);
}

//makes a kwic.  Uses stuff ripped out of make_arbitrary_span to set up call to make the stuff
//we need: the index to the tsBegTxt etc arrays.  We need the anv file.  We need the texts between
//before/hit and hit/after.  We need the num chars for before and for after: six parameters only
//params are: index of hit; n chars before, n chars after, text before/hit, text hit/after, name anv.file
int make_kwic(ClientData client_data, Tcl_Interp* tcl_interp, int argc, char **argv) {
 char *index_str=argv[1], *before_num_str=argv[2], *after_num_str=argv[3];
 char *before_str=argv[4], *after_str=argv[5], *spec_str=argv[6], *tcl_return;
 char tc_temp_var[50];
 const char *tc_temp;
 buffer_ptr kwic_fspec;
 style_ptr kwic_style;
 struct bounds_obj bounds;
 int orig_in_search, orig_hide, before_num, after_num, start_offset, end_offset;
 pool *current_pool;
 sgml_obj_ptr beg_sgml_el, end_sgml_el;
 process_book_ptr this_book_ptr = NULL;
 style_ptr this_style_ptr = NULL;
 create_process_book(&this_book_ptr, &this_style_ptr, tcl_interp, NULL, NULL, NULL, NULL);
 if (this_book_ptr==NULL || this_style_ptr==NULL) {
  write_log("Error processing Anastasia style TCL extension function \"makeKWIC\". Critical Error!.");
  return TCL_OK;
 }
 apr_pool_create(&current_pool, this_style_ptr->style_pool);
 if ((argc!=7)) {
  send_ana_param_error(this_style_ptr, "makeKWIC", "six", "one for the index to tsBegTxt etc, one each for the number of characters before and after the hit, one each for the text before and after the hit, one for the .anv file", this_book_ptr);
  apr_pool_destroy(current_pool);
  return check_process_book(this_book_ptr, this_style_ptr, "make_kwic", __FILE__, __LINE__);
 }
 //so let us start getting info out of the four result arrays for the hit:
 if (!valid_num_param(index_str, "first", "makeKWIC", &start_offset, this_style_ptr, this_book_ptr)) {
  apr_pool_destroy(current_pool);
  return check_process_book(this_book_ptr, this_style_ptr, "make_kwic", __FILE__, __LINE__);
 }
 sprintf(tc_temp_var, "tsBegEl(%s)", index_str);
 tc_temp = Tcl_GetVar(tcl_interp, tc_temp_var, 0);
 if (!tc_temp) {
                write_error_output(this_style_ptr, this_book_ptr, apr_psprintf(current_pool, "Error processing Anastasia style TCL extension function \"makeKWIC\" in style file \"%s\": there is no search result \"%s\".", this_style_ptr->file_name, tc_temp_var));
  apr_pool_destroy(current_pool);
  return check_process_book(this_book_ptr, this_style_ptr, "make_kwic", __FILE__, __LINE__);
 }
 if (!is_valid_el((char *) tc_temp, "makeKWIC", &beg_sgml_el, this_book_ptr, this_style_ptr)) {
  apr_pool_destroy(current_pool);
  return check_process_book(this_book_ptr, this_style_ptr, "make_kwic", __FILE__, __LINE__);
 }
 sprintf(tc_temp_var, "tsBegTxt(%s)", index_str);
 tc_temp= Tcl_GetVar(tcl_interp, tc_temp_var, 0);
 if (!tc_temp) {
                write_error_output(this_style_ptr, this_book_ptr, apr_psprintf(current_pool, "Error processing Anastasia style TCL extension function \"makeKWIC\" in style file \"%s\": there is no search result \"%s\".", this_style_ptr->file_name, tc_temp_var));
  apr_pool_destroy(current_pool);
  return check_process_book(this_book_ptr, this_style_ptr, "make_kwic", __FILE__, __LINE__);
 }
 if (!valid_num_param((char *) tc_temp, "first", "makeKWIC", &start_offset, this_style_ptr, this_book_ptr)) {
  apr_pool_destroy(current_pool);
  return check_process_book(this_book_ptr, this_style_ptr, "make_kwic", __FILE__, __LINE__);
 }
 sprintf(tc_temp_var, "tsEndEl(%s)", index_str);
 tc_temp = Tcl_GetVar(tcl_interp, tc_temp_var, 0);
 if (!tc_temp) {
                write_error_output(this_style_ptr, this_book_ptr, apr_psprintf(current_pool, "Error processing Anastasia style TCL extension function \"makeKWIC\" in style file \"%s\": there is no search result \"%s\".", this_style_ptr->file_name, tc_temp_var));
  apr_pool_destroy(current_pool);
  return check_process_book(this_book_ptr, this_style_ptr, "make_kwic", __FILE__, __LINE__);
 }
 if (!is_valid_el((char *) tc_temp, "makeKWIC", &end_sgml_el, this_book_ptr, this_style_ptr)) {
  apr_pool_destroy(current_pool);
  return check_process_book(this_book_ptr, this_style_ptr, "make_kwic", __FILE__, __LINE__);
 }
 sprintf(tc_temp_var, "tsEndTxt(%s)", index_str);
 tc_temp = Tcl_GetVar(tcl_interp, tc_temp_var, 0);
 if (!tc_temp) {
                write_error_output(this_style_ptr, this_book_ptr, apr_psprintf(current_pool, "Error processing Anastasia style TCL extension function \"makeKWIC\" in style file \"%s\": there is no search result \"%s\".",  this_style_ptr->file_name, tc_temp_var));
  apr_pool_destroy(current_pool);
  return check_process_book(this_book_ptr, this_style_ptr, "make_kwic", __FILE__, __LINE__);
 }
 if (!valid_num_param((char *) tc_temp, "first", "makeKWIC", &end_offset, this_style_ptr, this_book_ptr)) {
  apr_pool_destroy(current_pool);
  return check_process_book(this_book_ptr, this_style_ptr, "make_kwic", __FILE__, __LINE__);
 }
 //we have put in numbers for n chars before and after, yes?
 if (!valid_num_param(before_num_str, "second", "makeKWIC", &before_num, this_style_ptr, this_book_ptr)) {
  apr_pool_destroy(current_pool);
  return check_process_book(this_book_ptr, this_style_ptr, "make_kwic", __FILE__, __LINE__);
 }
 if (!valid_num_param(after_num_str, "third", "makeKWIC", &after_num, this_style_ptr, this_book_ptr)) {
  apr_pool_destroy(current_pool);
  return check_process_book(this_book_ptr, this_style_ptr, "make_kwic", __FILE__, __LINE__);
 }
 orig_hide = this_book_ptr->hide;
 this_book_ptr->hide = 0;
 orig_in_search=this_book_ptr->in_search;
 this_book_ptr->in_search=0;
 if (!init_output_buffer(&kwic_fspec, this_book_ptr, current_pool)) {
  apr_pool_destroy(current_pool);
  this_book_ptr->hide = orig_hide;
  this_book_ptr->in_search = orig_in_search;
  return check_process_book(this_book_ptr, this_style_ptr, "make_kwic", __FILE__, __LINE__);
 }
 if (!make_new_tcl_style(spec_str, this_book_ptr->bookname, &kwic_style, kwic_fspec, this_book_ptr, false, false)) {
  this_book_ptr->hide = orig_hide;
  this_book_ptr->in_search = orig_in_search;
  delete_output_buffer(&kwic_fspec, this_book_ptr);
  apr_pool_destroy(current_pool);
  return check_process_book(this_book_ptr, this_style_ptr, "make_kwic", __FILE__, __LINE__);
 }
 //make the calls here to build up the kwic
 //first: prev chars
 if (!calc_prev_chars(&bounds, start_offset, beg_sgml_el,before_num, this_book_ptr)) {
  this_book_ptr->hide = orig_hide;
  this_book_ptr->in_search = orig_in_search;
  delete_output_buffer(&kwic_fspec, this_book_ptr);
  apr_pool_destroy(current_pool);
  return check_process_book(this_book_ptr, this_style_ptr, "make_kwic", __FILE__, __LINE__);
 }
 if (!format_kwic_span(&bounds, kwic_style, this_book_ptr)) {
  this_book_ptr->hide = orig_hide;
  this_book_ptr->in_search = orig_in_search;
  delete_output_buffer(&kwic_fspec, this_book_ptr);
  apr_pool_destroy(current_pool);
  return check_process_book(this_book_ptr, this_style_ptr, "make_kwic", __FILE__, __LINE__);
 }
 //add what is between elements
 write_output(kwic_style, this_book_ptr, before_str);
 //second: the hit
 if (!calc_these_chars(&bounds, start_offset, end_offset, beg_sgml_el, end_sgml_el, this_book_ptr)) {
  this_book_ptr->hide = orig_hide;
  this_book_ptr->in_search = orig_in_search;
  delete_output_buffer(&kwic_fspec, this_book_ptr);
  apr_pool_destroy(current_pool);
  return check_process_book(this_book_ptr, this_style_ptr, "make_kwic", __FILE__, __LINE__);
 }
 if (!format_kwic_span(&bounds, kwic_style, this_book_ptr)) {
  this_book_ptr->hide = orig_hide;
  this_book_ptr->in_search = orig_in_search;
  delete_output_buffer(&kwic_fspec, this_book_ptr);
  apr_pool_destroy(current_pool);
  return check_process_book(this_book_ptr, this_style_ptr, "make_kwic", __FILE__, __LINE__);
 }
 //add what is between elements
 write_output(kwic_style, this_book_ptr, after_str);
 //third: after chars
 if (!calc_next_chars(&bounds, end_offset, end_sgml_el,after_num, this_book_ptr)) {
  this_book_ptr->hide = orig_hide;
  this_book_ptr->in_search = orig_in_search;
  delete_output_buffer(&kwic_fspec, this_book_ptr);
  apr_pool_destroy(current_pool);
  return check_process_book(this_book_ptr, this_style_ptr, "make_kwic", __FILE__, __LINE__);
 }
 if (!format_kwic_span(&bounds, kwic_style, this_book_ptr)) {
  this_book_ptr->hide = orig_hide;
  this_book_ptr->in_search = orig_in_search;
  delete_output_buffer(&kwic_fspec, this_book_ptr);
  apr_pool_destroy(current_pool);
  return check_process_book(this_book_ptr, this_style_ptr, "make_kwic", __FILE__, __LINE__);
 }
 tcl_return=Tcl_Alloc(kwic_fspec->len);
 strncpy(tcl_return, kwic_fspec->ptr,kwic_fspec->len-1);
 (tcl_return)[kwic_fspec->len-1]='\0';
 Tcl_SetResult(tcl_interp, tcl_return, TCL_DYNAMIC);
 delete_output_buffer(&kwic_fspec, this_book_ptr);
 apr_pool_destroy(current_pool);
 this_book_ptr->hide = orig_hide;
 this_book_ptr->in_search = orig_in_search;
 return check_process_book(this_book_ptr, this_style_ptr, "make_kwic", __FILE__, __LINE__);
}


//Some commands to get and set cookies via anastasia!!!
//Hmmm now with a cookie you can set the following values
//name=value
//path=/ - the location the request came from this will always be AnaServer
//Domain= - the url of the request came from... not sure if you can set this with port numbers so might just ignore
//expires=Mon, 01-Jan-2001 00:00:00 GMT .. the date the cookie will expire.. if not give it will be until the browser is closed
//Now how to let people set the expire time of the cookie... hours? days? either none or forever?
//O.k. let people specify the time if they choose.. they can input it using the commands "[clock format ..." command
//Hmm need to change this as I might have to send up writing the header information myself.. so basically I'll need to store the cookies.
int get_ana_setCookie(ClientData client_data, Tcl_Interp* tcl_interp, int argc, char **argv) {
 char *cookie, *tmp;
 pool *current_pool;
 process_book_ptr this_book_ptr = NULL;
 style_ptr this_style_ptr = NULL;
 create_process_book(&this_book_ptr, &this_style_ptr, tcl_interp, NULL, NULL, NULL, NULL);
 if (this_book_ptr==NULL || this_style_ptr==NULL) {
  write_log("Error processing Anastasia style TCL extension function \"setCookie\". Critical Error!.");
  return TCL_OK;
 }
 apr_pool_create(&current_pool, this_style_ptr->style_pool);
 //Expect 2 or 3 arguments
 if (argc<3 || argc>5) {
  send_ana_param_error(this_style_ptr, "setCookie", "two or three", "one for the cookie name, one for the cookie value and one optional one for the cookie expiration time.", this_book_ptr);
  apr_pool_destroy(current_pool);
  return check_process_book(this_book_ptr, this_style_ptr, "setCookie", __FILE__, __LINE__);
 }
 if (this_book_ptr->cookies==NULL)
 {
  return check_process_book(this_book_ptr, this_style_ptr, "setCookie", __FILE__, __LINE__);
 }
 //O.k. so add an entry to the table containing the new name=value and time.
 tmp = apr_psprintf(this_style_ptr->style_pool, "%s=%s", argv[1], argv[2]);
 if (argv[3]!=NULL) {
  cookie = apr_pstrdup(this_style_ptr->style_pool, argv[3]);
  //Now set the cookie.
  apr_table_set(this_book_ptr->cookies, tmp, cookie);
 } else {
  cookie = apr_pstrdup(this_style_ptr->style_pool, "0");
  apr_table_add(this_book_ptr->cookies, tmp, cookie);
 }
 apr_pool_destroy(current_pool);
 return check_process_book(this_book_ptr, this_style_ptr, "setCookie", __FILE__, __LINE__);
}


int get_ana_getCookie(ClientData client_data, Tcl_Interp* tcl_interp, int argc, char **argv) {
 char *tcl_return, *nv, *name, *value;
 const char *tmp;
 pool *current_pool;
 process_book_ptr this_book_ptr = NULL;
 style_ptr this_style_ptr = NULL;
 create_process_book(&this_book_ptr, &this_style_ptr, tcl_interp, NULL, NULL, NULL, NULL);
 if (this_book_ptr==NULL || this_style_ptr==NULL) {
  write_log("Error processing Anastasia style TCL extension function \"getCookie\". Critical Error!.");
  return TCL_OK;
 }
 apr_pool_create(&current_pool, this_style_ptr->style_pool);
 //Expect 1 argument.. the name of the cookie to get
 if (argc!=2) {
  send_ana_param_error(this_style_ptr, "getCookie", "one", "one for the cookie name.", this_book_ptr);
  apr_pool_destroy(current_pool);
  return check_process_book(this_book_ptr, this_style_ptr, "getCookie", __FILE__, __LINE__);
 }
 if (this_book_ptr->request_rec==NULL || this_book_ptr->request_rec->headers_in==NULL)
 {
  return check_process_book(this_book_ptr, this_style_ptr, "getCookie", __FILE__, __LINE__);
 }
 //O.k. get the Cookie from the headers
 tmp = apr_table_get(this_book_ptr->request_rec->headers_in, "Cookie");
 //Now loop through and split on ; to get the name=value pairs and then split on the = to get the name and value sepearely
 if (tmp!=NULL) {
  nv = ap_getword(current_pool, &tmp, ';');
  while (strcmp(nv, "")!=0) {
   //Now remove and trailing spaces.
   if (nv[0]==' ') {
    value = ap_getword_white_nc(current_pool, &nv);
   }
   //Now split the name=value into the seperate name and value.
   name = ap_getword_nc(current_pool, &nv, '=');
   if (strcmp(argv[1], name)==0) {
    //Found the cookie
    tcl_return=Tcl_Alloc(strlen(nv));
    strncpy(tcl_return, nv, strlen(nv));
    tcl_return[strlen(nv)]='\0';
    Tcl_SetResult(tcl_interp, tcl_return, TCL_DYNAMIC);
    break;
   }
   nv = ap_getword(current_pool, &tmp, ';');
  }
 }
 apr_pool_destroy(current_pool);
 return check_process_book(this_book_ptr, this_style_ptr, "getCookie", __FILE__, __LINE__);
}

int get_ana_getCookieNames(ClientData client_data, Tcl_Interp* tcl_interp, int argc, char **argv) {
 char *nv, *name, *value;
 const char *tmp;
 Tcl_Obj *obj;
 Tcl_Obj *listPtr;
 pool *current_pool;
 process_book_ptr this_book_ptr = NULL;
 style_ptr this_style_ptr = NULL;
 create_process_book(&this_book_ptr, &this_style_ptr, tcl_interp, NULL, NULL, NULL, NULL);
 if (this_book_ptr==NULL || this_style_ptr==NULL) {
  write_log("Error processing Anastasia style TCL extension function \"getCookie\". Critical Error!.");
  return TCL_OK;
 }
 apr_pool_create(&current_pool, this_style_ptr->style_pool);
 //Takes no arguments
 if (argc!=1) {
  send_ana_param_error(this_style_ptr, "getCookieNames", "none", ".", this_book_ptr);
  apr_pool_destroy(current_pool);
  return check_process_book(this_book_ptr, this_style_ptr, "getCookie", __FILE__, __LINE__);
 }
 //Create the lsit
 listPtr = Tcl_NewListObj(0, NULL);
 //O.k. get the Cookie from the headers
 tmp = apr_table_get(this_book_ptr->request_rec->headers_in, "Cookie");
 //Now loop through and split on ; to get the name=value pairs and then split on the = to get the name and value sepearely
 if (tmp!=NULL) {
  nv = ap_getword(current_pool, &tmp, ';');
  while (strcmp(nv, "")!=0) {
   //Now remove and trailing spaces.
   if (nv[0]==' ') {
    value = ap_getword_white_nc(current_pool, &nv);
   }
   //Now split the name=value into the seperate name and value.
   name = ap_getword_nc(current_pool, &nv, '=');
   obj = Tcl_NewObj();
   Tcl_SetStringObj(obj, name, strlen(name));
   Tcl_ListObjAppendList(tcl_interp, listPtr, obj);
   nv = ap_getword(current_pool, &tmp, ';');
  }
 }
 Tcl_SetObjResult(tcl_interp, listPtr);
 return check_process_book(this_book_ptr, this_style_ptr, "getCookieNames", __FILE__, __LINE__);
}

int set_content_type(ClientData client_data, Tcl_Interp* tcl_interp, int argc, char **argv) {
 process_book_ptr this_book_ptr = NULL;
 style_ptr this_style_ptr = NULL;
 create_process_book(&this_book_ptr, &this_style_ptr, tcl_interp, NULL, NULL, NULL, NULL);
 if (this_book_ptr==NULL || this_style_ptr==NULL) {
  write_log("Error processing Anastasia style TCL extension function \"setContentType\". Critical Error!.");
  return TCL_OK;
 }
 this_book_ptr->content_type = apr_pstrdup(this_book_ptr->current_pool, argv[1]);
 return check_process_book(this_book_ptr, this_style_ptr, "setContentType", __FILE__, __LINE__);
}

//we use this to make a vbase, .vbse, file to be read and used by vbase routines in anv files
//we have two parameters: first is anaid of witnesslist 

int make_ana_vbase(ClientData client_data, Tcl_Interp* tcl_interp, int argc, char **argv) {
 char *witlist=argv[1], *firstapp=argv[2], *lastapp=argv[3];
 char this_wit[256];
 sgml_obj_ptr witlist_el, firstapp_el, lastapp_el, thiswit_el;
 text_obj *sigil_key[MAXMSS];
 char *mss_to_exclude;
 char *this_out=0, *str=0, *vbse_file=0;
 int n_mss=0, n_apps=0, n_rdgs=0, n_wits=0;
 int currelnum, lastelnum, appelnum;
 int c=0;
 apr_file_t *vbse_in;
 pool *current_pool;
 bitstr_t *varlist=bit_alloc(MAXMSS);
 process_book_ptr this_book_ptr = NULL;
 style_ptr this_style_ptr = NULL;
 attr_ptr this_attr = 0;
 for (c=0; c<MAXMSS; c++) sigil_key[c]=NULL;
 create_process_book(&this_book_ptr, &this_style_ptr, tcl_interp, NULL, NULL, NULL, NULL);
 if (this_book_ptr==NULL || this_style_ptr==NULL) {
  write_log("Error processing Anastasia style TCL extension function \"makeVBase\". Critical Error!.");
  return TCL_OK;
 }
 mss_to_exclude=apr_psprintf(this_style_ptr->style_pool, " %s ", argv[4]);
 apr_pool_create(&current_pool, this_style_ptr->style_pool);
 if (argc < 4 || argc > 5 ) {
  send_ana_param_error(this_style_ptr, "makeVBase", "three or four", "an Anastasia identifier for the witlist,  two for the first and last app elements which contain the lem and rdg elements you wish to write into the variant database, and an optional parameter specifying witnesses you wish to exclude from the variant database.", this_book_ptr);
  apr_pool_destroy(current_pool);
  return TCL_OK;
 }
 if (!is_valid_el(witlist, "makeVBase", &witlist_el, this_book_ptr, this_style_ptr)) {
  apr_pool_destroy(current_pool);
  return TCL_OK;
 }
 if (!is_valid_el(firstapp, "makeVBase", &firstapp_el, this_book_ptr, this_style_ptr)) {
  apr_pool_destroy(current_pool);
  return TCL_OK;
 }
 if (!is_valid_el(lastapp, "makeVBase", &lastapp_el, this_book_ptr, this_style_ptr)) {
  apr_pool_destroy(current_pool);
  return TCL_OK;
 }
 lastelnum=lastapp_el->number;
 //are these elements what they are supposed to be?
 if (strcmp(witlist_el->gi->ptr,"witlist")) {
                write_error_output(this_style_ptr, this_book_ptr, apr_psprintf(this_style_ptr->style_pool, "Error processing Anastasia style TCL extension function \"makeVBase\" in style file \"%s\": the first parameter should be the anastasia identifier of a <witlist> element.  It is actually a <%s>!", this_style_ptr->file_name, witlist_el->gi->ptr));
  apr_pool_destroy(current_pool);
  return TCL_OK;
 }
    if (strcmp(firstapp_el->gi->ptr,"app")) {
                write_error_output(this_style_ptr, this_book_ptr, apr_psprintf(this_style_ptr->style_pool, "Error processing Anastasia style TCL extension function \"makeVBase\" in style file \"%s\": the second parameter should be the anastasia identifier of a <app> element.  It is actually a <%s>!", this_style_ptr->file_name, firstapp_el->gi->ptr));
  apr_pool_destroy(current_pool);
  return TCL_OK;
 }
        if (strcmp(lastapp_el->gi->ptr,"app")) {
                write_error_output(this_style_ptr, this_book_ptr, apr_psprintf(this_style_ptr->style_pool, "Error processing Anastasia style TCL extension function \"makeVBase\" in style file \"%s\": the third parameter should be the anastasia identifier of a <app> element.  It is actually a <%s>!", this_style_ptr->file_name, lastapp_el->gi->ptr));
  apr_pool_destroy(current_pool);
  return TCL_OK;
        }
        if (lastapp_el->number<=firstapp_el->number) {
                write_error_output(this_style_ptr, this_book_ptr, apr_psprintf(this_style_ptr->style_pool, "Error processing Anastasia style TCL extension function \"makeVBase\" in style file \"%s\": the third parameter (here: %s) must be the anastasia identifier of an <app> element coming after the anastasia identifier of the app element given in the second parameter (here, %s).", this_style_ptr->file_name, lastapp, firstapp));
                write_output(this_style_ptr, this_book_ptr, this_out);
  apr_pool_destroy(current_pool);
  return TCL_OK;
        }
        //all ducks in a row.  Let's roll.
        //witlist must contain a series of <wits> elements; the content of these is each a manuscript witness
        currelnum=witlist_el->child;
        while (currelnum) {
            size_t getlen;
            char wit_name[256];
            if (n_mss==MAXMSS) {
                    write_error_output(this_style_ptr, this_book_ptr, apr_psprintf(this_style_ptr->style_pool, "Error processing Anastasia style TCL extension function \"makeVBase\" in style file \"%s\": you have too many manuscripts in your witness list.  You are only permitted %d by this version of Anastasia.", this_style_ptr->file_name, MAXMSS));
                    apr_pool_destroy(current_pool);
                    return TCL_OK;
            }
            if (!get_next_sgml_obj(&thiswit_el, currelnum, this_book_ptr)) return TCL_OK;
            if (strcmp(thiswit_el->gi->ptr,"wits")) {
                    write_error_output(this_style_ptr, this_book_ptr, apr_psprintf(this_style_ptr->style_pool, "Error processing Anastasia style TCL extension function \"makeVBase\" in style file \"%s\": the children of <witlist> should all be <wits> elements.  This one is actually a <%s>!\n", this_style_ptr->file_name, thiswit_el->gi->ptr));
                    apr_pool_destroy(current_pool);
                    return TCL_OK;
            }
            //so this one is ok
                getlen=thiswit_el->end_data-thiswit_el->start_data;
                getlen=(getlen>256)?256:getlen;
  apr_off_t _where = (thiswit_el)->start_data;
  apr_file_seek(this_book_ptr->in_sgv, APR_SET, &_where);
  apr_size_t _count = sizeof(char) * getlen;
  apr_file_read(this_book_ptr->in_sgv, wit_name, &_count);
  if (_count != (sizeof(char) * getlen)) {
//    write_log("Error reading Anastasia .sgv file.\n");
    apr_pool_destroy(current_pool);
    return TCL_OK;
                }
                wit_name[getlen]='\0';
                sprintf(this_wit," %s ", wit_name);
                if (!strstr(mss_to_exclude, this_wit)) {
                    init_text_obj(&sigil_key[n_mss], this_book_ptr, current_pool, __FILE__, __LINE__);
                    if (!add_str_to_text_obj(sigil_key[n_mss], wit_name, this_book_ptr, current_pool)) {
                        apr_pool_destroy(current_pool);
                        return TCL_OK;        
                    }
                    n_mss++;
                }
                currelnum=thiswit_el->rsib;
        }
    //we got the witness list.  Now we start processing the app elements one by one
    currelnum=firstapp_el->number;
    appelnum=currelnum;
    //we now think we are going to write this out
    str = ap_make_full_path(current_pool, (this_book_ptr)->dir, (this_book_ptr)->bookname);
    vbse_file = apr_psprintf(current_pool, "%s.vbs", str);
    if (!ana_stream_open(vbse_file, &vbse_in, APR_FOPEN_WRITE|APR_FOPEN_READ|APR_FOPEN_TRUNCATE, current_pool)) return TCL_OK; 
    write_num(0, vbse_in, 0);
    write_num(sizeof(int)+7*sizeof(char)+MAXMSS/8, vbse_in, 0);
    //we have to write out the witness list!
    write_num(n_mss, vbse_in, 0);
    for (c=0; sigil_key[c]; c++) {
        write_num_str(sigil_key[c]->len, sigil_key[c]->ptr, vbse_in);
    }
    while (appelnum) {
       int c;
       char app_type[8], curr_type[8];
       int rdgelnum, refelnum;
       sgml_obj_ptr the_rdg_el, the_wit_el, the_ref_el;
       
        if (!get_next_sgml_obj(&firstapp_el, appelnum, this_book_ptr)) return TCL_OK;
        //is there an attribute type in this app
        app_type[0]=curr_type[0]='\0';
  this_attr = firstapp_el->attrs;
  while (this_attr!=0) {
   if (!strncmp("type", this_attr->name->ptr, this_attr->name->len)) {
    strncpy(app_type, this_attr->value->ptr, 8);
    app_type[7]='\0';
    strcpy(curr_type, app_type);
   }
   this_attr = this_attr->nattr;
  }
        //now, get the first lem or rdg in this: it will be the first child
        rdgelnum=firstapp_el->child;
        //possibilities..EITHER we have wit attributes on the element right off the bat OR we have rsibs which are wits
        //but for now: we are only going to deal with the the pattern 
        // <app> ...<rdg type?></rdg><wit><ref n=WITNESS></ref>....</wit> ... </app>
        while (rdgelnum) {
            if (!get_next_sgml_obj(&the_rdg_el, rdgelnum, this_book_ptr)) return TCL_OK;
            currelnum=rdgelnum;
            if (!strcmp(the_rdg_el->gi->ptr, "rdg")) {
                //is there a type attribute?
                size_t pos;
                n_rdgs++;
                strcpy(curr_type, app_type);   //put default app type in, if it exists...
    this_attr = firstapp_el->attrs;
    while (this_attr!=0) {
                    if (!strncmp("type", this_attr->name->ptr, this_attr->name->len)) {
                            strncpy(curr_type, this_attr->value->ptr, 8);
                            curr_type[7]='\0';
                    }
     this_attr = this_attr->nattr;
                }
                //now get the witelement
                for (c=0; c<MAXMSS; c++) 
                bit_clear(varlist, c);
                if (!get_next_sgml_obj(&the_wit_el, the_rdg_el->rsib, this_book_ptr)) return TCL_OK;
                //get the refs inside the wit
                refelnum= the_wit_el->child;
                while (refelnum) {
                    currelnum=refelnum;
                    n_wits++;
                    if (!get_next_sgml_obj(&the_ref_el, refelnum, this_book_ptr)) return TCL_OK;
     this_attr = the_ref_el->attrs;
     while (this_attr!=0) {
                        if (!strncmp("n", this_attr->name->ptr, this_attr->name->len)) {
                            int d;
                            int got_it=0;
                            for (d=0; sigil_key[d] && !got_it; d++) {
                                if (!strcmp(this_attr->value->ptr, sigil_key[d]->ptr)) {
                                    bit_set(varlist, d);
                                    got_it=1;
                                }
                            }
                        }
      this_attr = this_attr->nattr;
                    }
                    refelnum=the_ref_el->rsib;
                }
                //we have processed the reading and all associated witnesses.  So now we write it out
		apr_off_t _where = 0;
		apr_file_seek(vbse_in, APR_CUR, &_where);
                pos = _where;
                if (!write_num(rdgelnum, vbse_in, 0)) return TCL_OK;
		apr_size_t _count = sizeof(char) * 7;
                apr_file_write(vbse_in, &curr_type, &_count);
                if (_count != 7) return TCL_OK;
	        _count = sizeof(char) * bitstr_size(MAXMSS);
                apr_file_write(vbse_in, varlist, &_count);
               if (_count != MAXMSS/8) return TCL_OK;
                rdgelnum=the_wit_el->rsib;
            } else 
                rdgelnum=the_rdg_el->rsib;
        }
        if (appelnum==lastelnum)
            appelnum=0;
        else find_next_el(currelnum, 1, "app", &appelnum, this_book_ptr);
 //       if (!get_next_sgml_obj(&firstapp_el, firstapp, this_book_ptr)) return TCL_OK;
        currelnum=appelnum;
        n_apps++;
        if ((n_apps % 100) == 0) {
            write_error_output(this_style_ptr, this_book_ptr, apr_psprintf(this_style_ptr->style_pool, "Making variant database: %d <app>s processed; %d <rdg>s and %d witnesses (at element %d; last is %d).", n_apps, n_rdgs, n_wits, (int) currelnum, (int) lastelnum));
        }
    }
    write_error_output(this_style_ptr, this_book_ptr, apr_psprintf(this_style_ptr->style_pool, "Finished making variant database: %d <app> elements processed; %d <rdg>s and %d witnesses found .", n_apps, n_rdgs, n_wits));
    //final touch: write in the number actually written into the beginning of the file
    apr_off_t _where = 0;
    apr_file_seek(vbse_in, APR_SET, &_where);
    apr_size_t _count = sizeof(unsigned long);
    apr_file_write(vbse_in, &n_rdgs, &_count);    
    apr_file_close(vbse_in);
    return TCL_OK;
}

int search_ana_vbase(ClientData client_data, Tcl_Interp* tcl_interp, int argc, char **argv) {
        process_book_ptr this_book_ptr;
        style_ptr this_style_ptr = NULL;
        sgml_obj_ptr this_sgml_el;
        pool *current_pool;
        int  start_num, end_num, first_ret, offset, c, now_found=0, total_hits=0, tcl_index=0;
        char *first_num_str=argv[1];
        char this_el[512], cmd_el[512];  //use for write outs
 char *second_num_str=argv[2];
        search_vbase_ptr this_base;
        struct srch_cond_inf cond1,  cond2,  cond3, cond4, cond5, cond6, cond7, cond8;
        for (c=0;c<MAXMSS; c++) {
            cond1.cond_mss[c]=cond2.cond_mss[c]=cond3.cond_mss[c]=cond4.cond_mss[c]=cond5.cond_mss[c]=cond6.cond_mss[c]=cond7.cond_mss[c]=cond8.cond_mss[c]=-1;
        }
 cond1.mss[0]=cond2.mss[0]=cond3.mss[0]=cond4.mss[0]=cond5.mss[0]=cond6.mss[0]=cond7.mss[0]=cond8.mss[0]='\0';
 cond1.num_str[0]=cond2.num_str[0]=cond3.num_str[0]=cond4.num_str[0]=cond5.num_str[0]=cond6.num_str[0]=cond7.num_str[0]=cond8.num_str[0] = '\0';
 cond1.var_type[0]=cond2.var_type[0]=cond3.var_type[0]=cond4.var_type[0]=cond5.var_type[0]=cond6.var_type[0]=cond7.var_type[0]=cond8.var_type[0]='\0';
        cond1.in_out=cond2.in_out=cond3.in_out=cond4.in_out=cond5.in_out=cond6.in_out=cond7.in_out=cond8.in_out = 1;
 cond1.nom_mss=cond2.nom_mss=cond3.nom_mss=cond4.nom_mss=cond5.nom_mss=cond6.nom_mss=cond7.nom_mss=cond8.nom_mss = 0;
        cond1.active=cond2.active=cond3.active=cond4.active=cond5.active=cond6.active=cond7.active=cond8.active = 0;

 create_process_book(&this_book_ptr, &this_style_ptr, tcl_interp, NULL, NULL, NULL, NULL);
 if (this_book_ptr==NULL || this_style_ptr==NULL) {
  write_log("Error processing Anastasia style TCL extension function \"searchVBase\". Critical Error!.");
  return TCL_OK;
 }
 apr_pool_create(&current_pool, this_style_ptr->style_pool);
 if ((argc<6)) {
  send_ana_param_error(this_style_ptr, "searchVBase", "at least five", "the anastasia identifiers for the first and last <app> elements in which to search, the number of the first hit to return and the number of hits to return, and at least one search string", this_book_ptr);
  apr_pool_destroy(current_pool);
  return TCL_OK;
 }
 //so let us start getting info out of the four result arrays for the hit:
 if (!is_valid_el(first_num_str, "searchVBase", &this_sgml_el, this_book_ptr, this_style_ptr)) {
  apr_pool_destroy(current_pool);
  return TCL_OK;
 }
        start_num=this_sgml_el->number;
 if (!is_valid_el(second_num_str, "searchVBase", &this_sgml_el, this_book_ptr, this_style_ptr)) {
  apr_pool_destroy(current_pool);
  return TCL_OK;
 }
        end_num=this_sgml_el->number;
        if  (!fill_in_nums(argc, argv, (int *) &first_ret, (int *) &offset, "searchVBase", this_style_ptr, this_book_ptr))  {
  apr_pool_destroy(current_pool);
  return TCL_OK;
 }
        //if we have not already read in the vbs file, do it now!
        if (!this_book_ptr->vbase)  {
            char *str, *vbse_file;
            int c;
            apr_file_t *vbse_in;
            char *this_ptr;
            str = ap_make_full_path(current_pool, (this_book_ptr)->dir, (this_book_ptr)->bookname);
            vbse_file = apr_psprintf(current_pool, "%s.vbs", str);
            if (!ana_stream_open(vbse_file, &vbse_in, APR_FOPEN_READ|APR_FOPEN_BINARY, current_pool)) return TCL_OK; 
   //         write_log("search_ana_vbase 2231 commands.c");
            apr_size_t _count = sizeof(int);     
            apr_file_read(vbse_in, &(this_book_ptr)->n_rdgs, &_count);
  //           write_log(apr_psprintf(this_book_ptr->current_pool,"read in vbase nrdgs: %d", (this_book_ptr)->n_rdgs));
	    if (_count != sizeof(int)) return TCL_OK;
   		this_book_ptr->n_rdgs = upend(this_book_ptr->n_rdgs);
            _count = sizeof(int);
	    apr_file_read(vbse_in, &(this_book_ptr)->record_size, &_count);
	    if (_count != sizeof(int)) return TCL_OK;
   		this_book_ptr->record_size = upend(this_book_ptr->record_size);
	    apr_file_read(vbse_in, &(this_book_ptr)->n_mss, &_count);
	    if (_count != sizeof(int)) return TCL_OK;
   		this_book_ptr->n_mss = upend(this_book_ptr->n_mss);
            for (c=0; c<(this_book_ptr)->n_mss ; c++) {
                    init_text_obj((struct text_o **) &(this_book_ptr->sigil_key[c]), this_book_ptr, current_pool, __FILE__, __LINE__);
                    read_num_str(&(this_book_ptr)->sigil_key[c], vbse_in, this_book_ptr, current_pool);
             }
             //now, get in the entire data matrix
   //         if (fread(&first_id, sizeof(unsigned long), 1, vbse_in)!=1) return TCL_OK;

             (this_book_ptr)->vbase= (char *) apr_pcalloc(current_pool, (this_book_ptr)->record_size*(this_book_ptr)->n_rdgs);
   	     _count = (this_book_ptr)->record_size*(this_book_ptr)->n_rdgs;
             apr_file_read(vbse_in, (this_book_ptr)->vbase, &_count);
             if (_count != (this_book_ptr)->record_size*(this_book_ptr)->n_rdgs) {
 //                 write_log("Error reading in vbs file.\n");
                (this_book_ptr)->vbase=NULL;
  return TCL_OK;
            }
            //upend all ints
            for (c=0; (unsigned long) c<(this_book_ptr)->n_rdgs; c++) {
                this_ptr= (this_book_ptr)->vbase+(c*(this_book_ptr)->record_size);
                this_base= (search_vbase_ptr) this_ptr;
                (this_base)->rdg_id=upend((this_base)->rdg_id);
            }
        }
        //so we have searches to do.  Let's roll
    if (!check_query_cond(5, argv[5], &cond1, this_book_ptr, current_pool, this_style_ptr)) {
  apr_pool_destroy(current_pool);
  return TCL_OK;
 }
    if ((argc>6) && (!check_query_cond(6, argv[6], &cond2, this_book_ptr, current_pool, this_style_ptr))) {
  apr_pool_destroy(current_pool);
  return TCL_OK;
 }
    if ((argc>7) && (!check_query_cond(7, argv[7], &cond3, this_book_ptr, current_pool, this_style_ptr))) {
  apr_pool_destroy(current_pool);
  return TCL_OK;
 }
    if ((argc>8) && (!check_query_cond(8, argv[8], &cond4, this_book_ptr, current_pool, this_style_ptr))) {
  apr_pool_destroy(current_pool);
  return TCL_OK;
 }
    if ((argc>9) && (!check_query_cond(9, argv[9], &cond5, this_book_ptr, current_pool, this_style_ptr))) {
  apr_pool_destroy(current_pool);
  return TCL_OK;
 }
    if ((argc>10) && (!check_query_cond(10, argv[10], &cond6, this_book_ptr, current_pool, this_style_ptr))) {
  apr_pool_destroy(current_pool);
  return TCL_OK;
 }
    if ((argc>11) && (!check_query_cond(11, argv[11], &cond7, this_book_ptr, current_pool, this_style_ptr))) {
  apr_pool_destroy(current_pool);
  return TCL_OK;
 }
    if ((argc>12) && (!check_query_cond(12, argv[12], &cond8, this_book_ptr, current_pool, this_style_ptr))) {
  apr_pool_destroy(current_pool);
  return TCL_OK;
 }
    if (argc>13) {
        write_error_output(this_style_ptr, this_book_ptr, "More than eight search conditions specified for a  Anastasia VBase search.  Sorry, we cannot handle so many!");
    }
    //now, let us run the search!!!
    for (c = 0; c < (this_book_ptr)->n_rdgs; c++)
            {
                    char *this_ptr= (this_book_ptr)->vbase+(c*(this_book_ptr)->record_size);
                    this_base= (search_vbase_ptr) this_ptr;
                    if ((this_base->rdg_id>=start_num) && (this_base->rdg_id<=end_num)) {
                        if (!tst_srch_cond(this_base, &cond1, this_book_ptr)) continue;
                        if (!tst_srch_cond(this_base, &cond2, this_book_ptr)) continue;
                        if (!tst_srch_cond(this_base, &cond3, this_book_ptr)) continue;
                        if (!tst_srch_cond(this_base, &cond4, this_book_ptr)) continue;
                        if (!tst_srch_cond(this_base, &cond5, this_book_ptr)) continue;
                        if (!tst_srch_cond(this_base, &cond6, this_book_ptr)) continue;
                        if (!tst_srch_cond(this_base, &cond7, this_book_ptr)) continue;
                        if (!tst_srch_cond(this_base, &cond8, this_book_ptr)) continue;
                        total_hits++;
                        if (total_hits>=first_ret && now_found<offset) {
                            now_found++;
                            tcl_index++;
                            sprintf(this_el,"%d", (int) this_base->rdg_id);
                            sprintf(cmd_el,"vbElIds(%d)", (int) tcl_index);
                            Tcl_SetVar(tcl_interp, cmd_el, this_el, 0);
                        }
                     }
            }
    //put total hits in here:
    sprintf(this_el, "%d", total_hits);
    Tcl_SetVar(tcl_interp, "vbFound", this_el, 0);
    sprintf(this_el, "%d", now_found);
    Tcl_SetVar(tcl_interp, "vbReturned", this_el, 0);
    return TCL_OK;
}

int tst_srch_cond(search_vbase_ptr this_base, cond_ptr this_cond, process_book_ptr this_book_ptr) {
    int c;
    int these_mss=0, rdg_mss=0;
    if (this_cond->active==0) return 1;
    //how many hits among the mss?
    for (c=0; (c < (this_book_ptr)->n_mss) && (this_cond->cond_mss[c] != -1); c++)
            if (bit_test(this_base->vlist, this_cond->cond_mss[c])) these_mss++;
     if (this_cond->var_type[0]) {
  if (this_cond->var_type[0]=='!') {
   if (strstr(this_base->type, this_cond->var_type + 1))
    return(0);
  }
  else if (this_cond->var_type[0] && (!strstr(this_base->type, this_cond->var_type) ) )
   return(1);
 }
 if ((this_cond->num_cond==OnlyNom) && (these_mss!=this_cond->seek_mss)) return(0);
 if ((this_cond->num_cond==LessNom) && (these_mss>=this_cond->seek_mss)) return(0);
 if ((this_cond->num_cond==MoreNom) && (these_mss<=this_cond->seek_mss)) return(0);
 if ((this_cond->num_cond==NotNom) && (these_mss==this_cond->seek_mss)) return(0);
 if ((this_cond->num_cond==NotAny) && (these_mss!=0)) return(0);
        if (this_cond->others) return(1);
            else 
  {
                        for (c=0; (c < (this_book_ptr)->n_mss) ; c++) {
                            if (bit_test(this_base->vlist, c)) rdg_mss++;
                        }
                        if (rdg_mss == these_mss) return(1);
   else return(0);
  }
}

#if defined(ANA_MYSQL)
int ana_db_connect(ClientData client_data, Tcl_Interp* tcl_interp, int argc, char **argv) {
 pool *current_pool;
 process_book_ptr this_book_ptr = NULL;
 style_ptr this_style_ptr = NULL;
 char *host = argv[1], *username = argv[2], *password = argv[3], *db = argv[4];
 long port = 0;
 char *socket = NULL;
 char str[2048];
 MYSQL *mysql_result;
 create_process_book(&this_book_ptr, &this_style_ptr, tcl_interp, NULL, NULL, NULL, NULL);
 if (this_book_ptr==NULL || this_style_ptr==NULL) {
  write_log("Error processing Anastasia style TCL extension function \"dbConnect\". Critical Error!.");
  return TCL_ERROR;
 }
 apr_pool_create(&current_pool, this_style_ptr->style_pool);
 //Expect 1 argument.. the name of the cookie to get
 if (argc>6 && argc<3) {
  send_ana_param_error(this_style_ptr, "dbConnect", "three - six", "one for the host name, one for the username, one for the password and an option one for the database to use, optional one for the port and one for the socket", this_book_ptr);
  apr_pool_destroy(current_pool);
  check_process_book(this_book_ptr, this_style_ptr, "dbConnect", __FILE__, __LINE__);
  return TCL_ERROR;
 }
 if (argc>=6) {
     port = strtol(argv[5], NULL, 10);
     if (argc==7) { socket = argv[6]; }
 }
 //Check that a connection doesn't already exist
 if (this_book_ptr->database.host!=NULL) {
//   write_log("Closing the connection to the database");
   mysql_close(&(this_book_ptr->database));
 }
 mysql_result = mysql_real_connect(&(this_book_ptr->database), NULL, username, password, db, port, socket, 0);
 if (mysql_result==NULL) {
  sprintf(str, "Error process Anastasia function \"dbConnect\". %s - Errno %d", mysql_error(&(this_book_ptr->database)), mysql_errno(&(this_book_ptr->database)));
  write_error_output(this_style_ptr, this_book_ptr, str);
  check_process_book(this_book_ptr, this_style_ptr, "dbConnect", __FILE__, __LINE__);
  return TCL_ERROR;
 }
 apr_pool_destroy(current_pool);
 return check_process_book(this_book_ptr, this_style_ptr, "dbConnect", __FILE__, __LINE__);
}

int ana_db_disconnect(ClientData client_data, Tcl_Interp* tcl_interp, int argc, char **argv) {
 process_book_ptr this_book_ptr = NULL;
 style_ptr this_style_ptr = NULL;
 create_process_book(&this_book_ptr, &this_style_ptr, tcl_interp, NULL, NULL, NULL, NULL);
 if (this_book_ptr==NULL || this_style_ptr==NULL) {
//  write_log("Error processing Anastasia style TCL extension function \"dbDisconnect\". Critical Error!.");
  return TCL_OK;
 }
 mysql_close(&(this_book_ptr->database));
 if (this_book_ptr->database.host!=NULL) {
//  write_log("Mysql connection *not* closed");
 }
 return check_process_book(this_book_ptr, this_style_ptr, "dbDisconnect", __FILE__, __LINE__);
}

//Check to see if there is a current connection
int ana_db_checkconnect(ClientData client_data, Tcl_Interp* tcl_interp, int argc, char **argv) {
}

//Possible return variables
//dbInsertId - the last insert id if a insert query was performed
//dbNumRows  - the num of rows from the last query/insert etc.
//dbAffectedRows - the num of rows effected from the last query
//dbFieldCount - number of coloums in the return result.
//This function returns the mysql_result pointer (for use later!!)
int ana_db_query(ClientData client_data, Tcl_Interp* tcl_interp, int argc, char **argv) {
 process_book_ptr this_book_ptr = NULL;
 style_ptr this_style_ptr = NULL;
        pool *current_pool;
        char *sql_query = argv[1], str[2048], *tcl_return;
        int query_len = 0;
        long dbAffectedRows=0, dbNumRows=0, dbInsertId=0, dbFieldCount=0;
        MYSQL_RES *mysql_result;
 create_process_book(&this_book_ptr, &this_style_ptr, tcl_interp, NULL, NULL, NULL, NULL);
 if (this_book_ptr==NULL || this_style_ptr==NULL) {
  write_log("Error processing Anastasia style TCL extension function \"dbQuery\". Critical Error!.");
  return TCL_OK;
 }
 apr_pool_create(&current_pool, this_style_ptr->style_pool);
 //Expect 1 argument.. the name of the cookie to get
 if (argc!=2) {
  send_ana_param_error(this_style_ptr, "dbQuery", "one", "one for the sql query", this_book_ptr);
  apr_pool_destroy(current_pool);
  return check_process_book(this_book_ptr, this_style_ptr, "dbQuery", __FILE__, __LINE__);
 }
        query_len = strlen(sql_query);
        if (mysql_real_query(&(this_book_ptr->database), sql_query, query_len)!=0) {
            sprintf(str, "Error process Anastasia style TCL extension function \"dbQuery\". %s.", mysql_error(&(this_book_ptr->database)));
            write_error_output(this_style_ptr, this_book_ptr, str);
            sprintf(str, "%d", (int) 0);
            query_len = strlen(str);
            tcl_return = Tcl_Alloc(query_len);
            strncpy(tcl_return, str, query_len);
            (tcl_return)[query_len-1]='\0';
            Tcl_SetResult(tcl_interp, tcl_return, TCL_DYNAMIC);
            apr_pool_destroy(current_pool);
            return check_process_book(this_book_ptr, this_style_ptr, "dbQuery", __FILE__, __LINE__);
        }
        mysql_result = mysql_store_result(&(this_book_ptr->database));
        //Set up a few variable according to the results of the query
        dbInsertId = (long) mysql_insert_id(&(this_book_ptr->database));
        sprintf(str, "%lu", dbInsertId);
        Tcl_SetVar(tcl_interp, "dbInsertId", str, 0);
        dbAffectedRows = (long) mysql_affected_rows(&(this_book_ptr->database));
        sprintf(str, "%lu", dbAffectedRows);
        Tcl_SetVar(tcl_interp, "dbAffectedRows", str, 0);
        if (mysql_result!=0) {
            dbNumRows = (long) mysql_num_rows(mysql_result);
        } else {
            dbNumRows = 0;
        }
        sprintf(str, "%lu", dbNumRows);
        Tcl_SetVar(tcl_interp, "dbNumRows", str, 0);
        dbFieldCount = (long) mysql_field_count(&(this_book_ptr->database));
        sprintf(str, "%lu", dbFieldCount);
        Tcl_SetVar(tcl_interp, "dbFieldCount", str, 0);
 sprintf(str, "%lu", (ulint) mysql_result);
        query_len = strlen(str);
        query_len++;
 tcl_return = Tcl_Alloc(query_len);
 strncpy(tcl_return, str, query_len);
 (tcl_return)[query_len-1]='\0';
 Tcl_SetResult(tcl_interp, tcl_return, TCL_DYNAMIC);
        apr_pool_destroy(current_pool);
        return check_process_book(this_book_ptr, this_style_ptr, "dbQuery", __FILE__, __LINE__);
}

int ana_db_free_result(ClientData client_data, Tcl_Interp* tcl_interp, int argc, char **argv) {
 process_book_ptr this_book_ptr = NULL;
 style_ptr this_style_ptr = NULL;
        char *result_str = argv[1];
        MYSQL_RES *mysql_result;
 create_process_book(&this_book_ptr, &this_style_ptr, tcl_interp, NULL, NULL, NULL, NULL);
 if (this_book_ptr==NULL || this_style_ptr==NULL) {
  write_log("Error processing Anastasia style TCL extension function \"dbFreeResult\". Critical Error!.");
  return TCL_OK;
 }
        if (argc!=2) {
  send_ana_param_error(this_style_ptr, "dbFreeResult", "one", "one for the sql result identifier", this_book_ptr);
  return check_process_book(this_book_ptr, this_style_ptr, "dbQuery", __FILE__, __LINE__);
        }
        mysql_result = (MYSQL_RES *) atol(result_str);
        mysql_free_result(mysql_result);
        return check_process_book(this_book_ptr, this_style_ptr, "dbFreeResult", __FILE__, __LINE__);
}

//dsResults - array of 
int ana_db_get_results(ClientData client_data, Tcl_Interp* tcl_interp, int argc, char **argv) {
 process_book_ptr this_book_ptr = NULL;
 style_ptr this_style_ptr = NULL;
        char *result_str = argv[1], str[1024], str2[1024], pstr[100];
        MYSQL_RES *mysql_result;
        MYSQL_ROW mysql_row;
        Tcl_Obj *strObj, *listObj;
        int start_offset = 0, num_results = 0, result_count=1, a=0, b=0;
        unsigned int num_fields=0;
        unsigned long *strlens, longnum;
 create_process_book(&this_book_ptr, &this_style_ptr, tcl_interp, NULL, NULL, NULL, NULL);
 if (this_book_ptr==NULL || this_style_ptr==NULL) {
  write_log("Error processing Anastasia style TCL extension function \"dbGetResults\". Critical Error!.");
  return TCL_OK;
 }
        if (argc!=4) {
  send_ana_param_error(this_style_ptr, "dbGetResults", "three", "one for the sql result identifier, one for the results offset and one for the number of results to return.", this_book_ptr);
  return check_process_book(this_book_ptr, this_style_ptr, "dbGetResults", __FILE__, __LINE__);
        }
        start_offset = atoi(argv[2]);
        num_results = atoi(argv[3]);
        mysql_result = (MYSQL_RES*) atol(result_str);
        if (mysql_result==0) {
            write_error_output(this_style_ptr, this_book_ptr, "Error: Invalid mysql_result variable.");
            return check_process_book(this_book_ptr, this_style_ptr, "dbGetResults", __FILE__, __LINE__);
        }
        if (start_offset>1) {
            mysql_data_seek(mysql_result, start_offset);
        }
        num_fields = mysql_num_fields(mysql_result);
        if (num_fields==0) {
            write_log("Error processing Anastasia style TCL extension function \"dbGetResults\". Critical Error!.");
            return check_process_book(this_book_ptr, this_style_ptr, "dbGetResults", __FILE__, __LINE__);
        }
        while ((mysql_row = mysql_fetch_row(mysql_result)) && result_count<=num_results) {
            strlens = mysql_fetch_lengths(mysql_result);
            if (strlens==NULL) {
                write_log("Error processing Anastasia style TCL extension function \"dbGetResults\". Critical Error!.");
                return check_process_book(this_book_ptr, this_style_ptr, "dbGetResults", __FILE__, __LINE__);
            }
            if (num_fields==1) {
                sprintf(str, "dbResults(%d)", result_count);
    if (mysql_row[0]==NULL) {
     strcpy(str2, "");
    } else {
                 strcpy(str2, mysql_row[0]);
    }
                Tcl_SetVar(tcl_interp, str, str2, 0);
            } else {
                strObj = Tcl_NewStringObj(mysql_row[0], strlen(mysql_row[0]));
                listObj = Tcl_NewListObj(1, &strObj);
                for (a=1; (unsigned int) a<num_fields; a++) {
     if (mysql_row[a]==NULL) {
      strObj = Tcl_NewStringObj("", 0);
     } else {
                       strObj = Tcl_NewStringObj(mysql_row[a], strlen(mysql_row[a])); 
     }
                    Tcl_ListObjAppendElement(tcl_interp, listObj, strObj);
                }
                sprintf(pstr, "%d", result_count);
                Tcl_SetVar2Ex(tcl_interp, "dbResults", pstr, listObj, 0);
            }
            result_count++;
        }
  result_count--;
        sprintf(pstr, "%d", result_count);
        Tcl_SetVar(tcl_interp, "dbReturned", pstr, 0);
        num_results = (int) mysql_num_rows(mysql_result);
        sprintf(pstr, "%d", num_results);
        Tcl_SetVar(tcl_interp, "dbFound", pstr, 0);
        return check_process_book(this_book_ptr, this_style_ptr, "dbGetResults", __FILE__, __LINE__);
}
#endif

int ana_set_header(ClientData client_data, Tcl_Interp *tcl_interp, int argc, char **argv)
{
 char *name, *value;
 pool *current_pool;
 process_book_ptr this_book_ptr = NULL;
 style_ptr this_style_ptr = NULL;
 create_process_book(&this_book_ptr, &this_style_ptr, tcl_interp, NULL, NULL, NULL, NULL);
 if (this_book_ptr==NULL || this_style_ptr==NULL) {
  write_log("Error processing Anastasia style TCL extension function \"setHeader\". Critical Error!.");
  return TCL_OK;
 }
 apr_pool_create(&current_pool, this_style_ptr->style_pool);
 //Expect 1 argument.. the name of the cookie to get
 if (argc!=3) {
  send_ana_param_error(this_style_ptr, "setHeader", "two", "one for the header name, one for the header value.", this_book_ptr);
  apr_pool_destroy(current_pool);
  return check_process_book(this_book_ptr, this_style_ptr, "setHeader", __FILE__, __LINE__);
 }
 name = apr_pstrdup(this_book_ptr->current_pool, argv[1]);
 value = apr_pstrdup(this_book_ptr->current_pool, argv[2]);
 apr_table_set(this_book_ptr->headers, name, value);
 return check_process_book(this_book_ptr, this_style_ptr, "setHeader", __FILE__, __LINE__);
}

int ana_writeErrorMessage(ClientData client_data, Tcl_Interp* tcl_interp, int argc, char **argv) {
 char *err_message=argv[1];
 pool *current_pool;
 char *tcl_return;
 process_book_ptr this_book_ptr = NULL;
 style_ptr this_style_ptr = NULL;
 create_process_book(&this_book_ptr, &this_style_ptr, tcl_interp, NULL, NULL, NULL, NULL);
 if (this_book_ptr==NULL || this_style_ptr==NULL) {
  write_log("Error processing Anastasia style TCL extension function \"writeErrorMessage\". Critical Error!.");
  return TCL_OK;
 }
 tcl_return=NULL;
 apr_pool_create(&current_pool, this_book_ptr->current_pool);
 if (argc!=2) {
                write_error_output(this_style_ptr, this_book_ptr, apr_psprintf(current_pool, "Error processing Anastasia style TCL extension function \"writeErrorMessage\" in style file \"%s\".: this function requires one parameter, a string for the message to be written.", this_style_ptr->file_name));
  apr_pool_destroy(current_pool);
  return check_process_book(this_book_ptr, this_style_ptr, "puts", __FILE__, __LINE__);
 }
 write_error_output(this_style_ptr, this_book_ptr, err_message);
 apr_pool_destroy(current_pool);
 return check_process_book(this_book_ptr, this_style_ptr, "puts", __FILE__, __LINE__);
}

int ana_global_httppost(ClientData client_data, Tcl_Interp *tcl_interp, int argc, char **argv) {
 char str[2048];
 process_book_ptr this_book_ptr = NULL;
 style_ptr this_style_ptr = NULL;
// write_log("in ana_global_httppost 1");
 create_process_book(&this_book_ptr, &this_style_ptr, tcl_interp, NULL, NULL, NULL, NULL);
 // write_log("in ana_global_httppost 2");
 //write_log(apr_psprintf(anastasia_info.server_pool, "paramters are: (%s)", *argv));
 if (this_book_ptr==NULL || this_style_ptr==NULL) {
  write_log("Error processing Anastasia style TCL extension function \"writeErrorMessage\". Critical Error!.");
  return TCL_OK;
 }
 strcpy(str," if {[info exist httppost]} {");
 strcat(str, " foreach a [array names httppost] {)");
 strcat(str, "  set $a $httppost($a);");
 strcat(str, " }");
 Tcl_Eval(tcl_interp, str);
 return check_process_book(this_book_ptr, this_style_ptr, "puts", __FILE__, __LINE__);
}
