/*
        System: Structured text retrieval tool sgrep.
	Module: main.c
	Author: Pekka Kilpeläinen & Jani Jaakkola
	Description: Command line parsing. All work is done elsewhere
		     	pattern matching, evaluation and output
	Version history: Original version February 1995 by JJ & PK
	Copyright: University of Helsinki, Dept. of Computer Science
		   Distributed under GNU General Public Lisence
		   See file COPYING for details
*/
/* changed to fit into anastasia grove maker and reader 0500 */

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <limits.h>

/*apache stuff goes here*/
#include "sgrep.h"
#include "index.h"
#include "pmatch.h"
#include "eval.h"
#include "common.h"
#include <apr_strings.h>

#define INDEX_VERSION_MAGIC ("sgrep-index v0")

void free_parse_tree(SgrepData *this_sgrep, ParseTreeNode *root);

/*
 * struct for list of expression strings ( or files ) to be executed 
 */
enum ExpressionType { E_FILE,E_TEXT };
struct Expression {
    enum ExpressionType type; /* If this is a file, or command line */
    char *expr; 	      /* Pointer to either filename or expression */
    struct Expression *next;
};

void add_command(char *);
SgrepString *read_expressions(SgrepData *this_sgrep, struct Expression *expression_list);
int run_stream(FileList *files, ParseTreeNode *, struct PHRASE_NODE *p_list, int *, int, int *, void **, int, struct SgrepStruct *this_sgrep, process_book_ptr );
void create_constant_lists(struct SgrepStruct *this_sgrep);
void delete_constant_lists(struct SgrepStruct *this_sgrep);
int search_main(int argc, char *argv[]);
FileList *lo_mem_file_list(FILE *, unsigned char *);
static int pr_display_gc_list(RegionList *l, int num_return, int is_el, int start, void **result_array, struct SgrepStruct *this_sgrep, process_book_ptr);

/*
 * Global variables used inside main.c . These are mainly used for storing
 * information about given options
 */

#define CALC_TIME(X) /* Nothing */
#define times(X) /* Nothing */

/*
 * Runs sgrep in stream mode
 */
/*num_return comes back with number actually found and put in sgElsIds array; nresults comes back with total founds */
int run_stream(FileList *files, ParseTreeNode *root, struct PHRASE_NODE *p_list, int *n_results, int start, int *num_return, void **result_array, int is_el, struct SgrepStruct *this_sgrep, process_book_ptr this_book_ptr) {
	RegionList *result;
	/* Pattern matching on input files */	
	/*in case we find nothing: don't give a false positive */
	*n_results=0;
//	write_log("in run_stream main.c 70");
	if (search(this_sgrep,p_list,files,0,flist_files(files)-1)==SGREP_ERROR) {
	    return SGREP_ERROR;
	}
	times(&tps.acsearch);
	
	/* Evaluate the expression */
	result=eval(this_sgrep,files,root);
	if (result==NULL) return SGREP_ERROR;

	if (stats.region_lists_now>stats.constant_lists+1) {
	    sgrep_error(this_sgrep,"Query leaked %d gc lists\n",
			stats.region_lists_now-stats.constant_lists+1);
	}
	times(&tps.eval);
//	write_log("in run_stream main.c 85");

	/* Outputting result */
	stats.output=LIST_SIZE(result);
	/* Should we show the count of matching regions */
	if (this_sgrep->display_count )
	{
	/*	printf("%d\n",LIST_SIZE(result)); */
		*n_results=LIST_SIZE(result);
	}
	/* We show result list only if there wasn't -c option, and there was
	   something to output */
//		write_log("in run_stream main.c 96");
  	if ( !this_sgrep->display_count && !this_sgrep->no_output && (
			stats.output>0 || this_sgrep->print_all ))  {
			/* PR: we have altered this quite a lot to give us exactly the format we want */
			*n_results=LIST_SIZE(result);;
//			write_log(apr_psprintf(anastasia_info.server_pool, "problematic number? %d", *n_results));
			if (is_el) {
				/*how many do we send back? */
				if (start<=*n_results) {
					*num_return=(*n_results-start)<*num_return?*n_results-start + 1:*num_return;
					*num_return=pr_display_gc_list(result, *num_return, is_el, start,  (void **) result_array, this_sgrep, this_book_ptr);
				}
			}
			else {}  /*actually don't need this! */
	}
//	*n_results=0;
	free_gclist(result);
	times(&tps.output);
	return SGREP_OK;
}

/*
 * Creates and initializes the constant lists, start end and chars.
 * They may need to be modified later, because when scanning each
 * file separately end point keeps changing
 */
void create_constant_lists(struct SgrepStruct *this_sgrep) {
	/* Chars list is optimized and created in a special way */
	this_sgrep->chars_list=new_region_list(this_sgrep);
	if (this_sgrep->chars_list!=NULL) {
		to_chars(this_sgrep->chars_list,1,1);
		stats.constant_lists+=1;
	}
}

void delete_constant_lists(struct SgrepStruct *this_sgrep) {
	free_gclist(this_sgrep->chars_list);
	this_sgrep->chars_list=NULL;
	stats.constant_lists-=1;
}

/*
 * Catenates expression file to string
 */
int read_expression_file(SgrepString *str, const char *fname) {
    FILE *stream;
    char buf[1024];
    int bytes;
    SGREPDATA(str);

    /* First add a newline, if there isn't one already */
    if (str->length>0 && str->s[str->length-1]!='\n') {
        string_cat(str,"\n");
    }

    if (fname[0]=='-' && fname[1]==0) {
        /* Expression is coming from stdin */
        stream=stdin;
        string_cat(str,"#line 1 \"-\"\n");
    } else {
        stream=fopen(fname,"r");
        if (stream==NULL) {
            sgrep_error(this_sgrep,"Expression file '%s' : %s\n",
                        fname,strerror(errno));
            return SGREP_ERROR;
        }
        string_cat(str,"#line 1 \"");
        string_cat(str,fname);
        string_cat(str,"\"\n");
    }

    do {
        bytes=fread(buf,1,sizeof(buf)-1,stream);
        buf[bytes]=0;
        string_cat(str,buf);
    } while (!feof(stream) && !ferror(stream));

    if (ferror(stream)) {
        sgrep_error(this_sgrep,"Reading file '%s' : %s\n",
                    fname,strerror(errno));
        if (stream!=stdin) fclose(stream);
        return SGREP_ERROR;
    }
    if (stream!=stdin) fclose(stream);
    return SGREP_OK;
}


/*
 * Reads the expression commands to com_file_buf 
 */
SgrepString *read_expressions(SgrepData *this_sgrep,
			      struct Expression *expression_list) {
    SgrepString *return_string;

    if (expression_list==NULL) {
	FILE *test_stream=NULL;
	/* fix to get around a problem in fopen */
	return_string=new_string(this_sgrep,8192);

	/* Check for USER_SGREPRC */
	if (getenv("HOME")) {
	    SgrepString *sgreprc=new_string(this_sgrep,1024);
	    string_cat(sgreprc,getenv("HOME"));
	    string_cat(sgreprc,"/");
	    string_cat(sgreprc,USER_SGREPRC);
	    test_stream=fopen(sgreprc->s,"r");
	    if (test_stream) {
		/* found USER_SGREPRC */
		if (read_expression_file(return_string,string_to_char(sgreprc))
		    ==SGREP_ERROR) {
		    delete_string(return_string);
		    return_string=NULL;
		}
	    }
	    delete_string(sgreprc);
	}
	
	/* Check for SYSTEM_SGREPRC */
	if (!test_stream) {
	/* ANDREW WHY DOES THIS CRASH THE SYSTEM */
	#ifdef WIN32
	    /* test_stream=fopen(SYSTEM_SGREPRC,"r"); */
	 #endif
	 #ifdef _MAC
	 	test_stream=fopen(SYSTEM_SGREPRC,"r");
	 #endif
	    if (test_stream) {
		if (read_expression_file(return_string,SYSTEM_SGREPRC)
		    ==SGREP_ERROR) {
		    delete_string(return_string);
		    return_string=NULL;
		}
	    }
	}
	
	if (test_stream) fclose(test_stream);
	return return_string;
    }

    /* Shameless use of recursion */
    return_string=read_expressions(this_sgrep,expression_list->next);

    if (return_string!=NULL)  {
	switch(expression_list->type){
	case E_FILE:
	    if (read_expression_file(return_string,expression_list->expr)
		==SGREP_ERROR) {
		delete_string(return_string);
		return_string=NULL;
	    }
	    break;
	case E_TEXT:
	    /* First add a newline, if there isn't one already */	
	    if (return_string->length>0 &&
		return_string->s[return_string->length-1]!='\n') {
		string_cat(return_string,"\n");
	    }	    
	    string_cat(return_string,"#line 1 \"\"\n");
	    string_cat(return_string,expression_list->expr);
	    break;
	}
    }
	ana_free(&(this_sgrep->this_book->anastasia_memory), expression_list->expr);
    ana_free(&(this_sgrep->this_book->anastasia_memory), expression_list);
    return return_string;
   
}

int set_up_sgrep_index(process_book_ptr this_book_ptr) {
	int file_list_start;
	const unsigned char *ptr;
  	apr_off_t _where = 0;
	apr_file_seek(this_book_ptr->in_idx, APR_END, &_where);
	this_book_ptr->map_len=_where;
	this_book_ptr->map_idx= ana_malloc(&(this_book_ptr->anastasia_memory), 1024, __FILE__, __LINE__);
	if (!this_book_ptr->map_idx) {
		write_log("Failed to allocate memory for index file; searching will fail\n");
		this_book_ptr->map_idx=NULL;
		return false; 
	}
	_where = 0;
	apr_file_seek(this_book_ptr->in_idx, APR_SET, &_where);
   	apr_size_t _count = sizeof(char) * 1024;
	apr_file_read(this_book_ptr->in_idx, this_book_ptr->map_idx, &_count);
	if (_count != (sizeof(char) * 1024)) {
		char message[512];
		sprintf(message, "Failed to read index file '%s'; seaching will fail\n", this_book_ptr->fs_idx);
		write_log(message);
		this_book_ptr->map_idx=NULL;
		return false;  
	}
	ptr = (const unsigned char *) this_book_ptr->map_idx;
	ptr += 512;
	this_book_ptr->lomem_idx= (struct lomem_inf *) ana_malloc(&(this_book_ptr->anastasia_memory), sizeof(struct lomem_inf), __FILE__, __LINE__);
	this_book_ptr->lomem_idx->len = get_int(ptr,0); 
	this_book_ptr->lomem_idx->array_offset = get_int(ptr,1);
	this_book_ptr->lomem_idx->entries_offset = get_int(ptr,2); 
	this_book_ptr->lomem_idx->file_list = ana_malloc(&(this_book_ptr->anastasia_memory), 256, __FILE__, __LINE__);
	file_list_start = get_int(((const unsigned char *)this_book_ptr->map_idx)+512,3);
  	_where = file_list_start;
	apr_file_seek(this_book_ptr->in_idx, APR_SET, &_where);
   	_count = sizeof(char) * 256;
	apr_file_read(this_book_ptr->in_idx, (void *)this_book_ptr->lomem_idx->file_list, &_count);
	return true;

}

int do_count_sg(char *search_str, process_book_ptr this_book_ptr, pool *current_pool) {
  	ParseTreeNode *root;
    struct PHRASE_NODE *p_list;
    FileList *input_files=NULL;
    int n_results;
    struct SgrepStruct sgrep_instance, *this_sgrep;
	SgrepString *expression;
    /* First initialize the sgrep instance */
    memset(&sgrep_instance,0,sizeof(sgrep_instance));
	this_sgrep=&sgrep_instance;	
    this_sgrep->do_concat = 0;
    this_sgrep->output_style = SHORT_OUTPUT; /* default is short */
    this_sgrep->open_failure = OPEN_FAILURE;
    this_sgrep->print_newline = 1;
    this_sgrep->stdin_temp_file = NULL;	/* not read yet */
    this_sgrep->print_all = 0;
    this_sgrep->chars_list = NULL;
    this_sgrep->scanner_type = SGML_SCANNER;
    this_sgrep->current_pool = current_pool;
	this_sgrep->index_file = this_book_ptr->fs_idx;
	this_sgrep->stream_mode = 1;
	this_sgrep->this_book = this_book_ptr;
	this_sgrep->word_chars = apr_psprintf(current_pool, "0-9a-zA-Z[]%c-%c", 128, 255);
	/*here is the pre-opened index file*/
	this_sgrep->index_reader=pr_new_index_reader(this_sgrep);
	/*make sure we are set for this */
	this_sgrep->no_output=1;
	this_sgrep->display_count=1;

    /* 
     * Creating constant lists. They might be needed in the parse() step
     */
    create_constant_lists(this_sgrep);
	
	//Create the expression
//	expression=read_expressions(this_sgrep,NULL);
	expression = new_string(this_sgrep,8192);
	string_cat(expression,"#line 1 \"\"\n");
    string_cat(expression, search_str);

	/* 
	 * Invoking parser 
	 */
	if ((root=parse_and_optimize(this_sgrep,expression->s,&p_list)) == NULL) {
	    sgrep_error(this_sgrep,"No query to evaluate. Bailing out.\n");
	    return -1;
	}	

    /* Check for file list in index */
    if (this_sgrep->index_reader) {
		input_files=(struct FileListStruct *) index_file_list(this_sgrep->index_reader, (unsigned char *) this_book_ptr->lomem_idx->file_list);
    }
    
    /*
     * Evaluation style depends on sgrep->stream_mode 
     */
	run_stream(input_files,root,p_list, &n_results, 0, NULL, NULL, 0, this_sgrep, this_book_ptr);
    free_parse_tree(this_sgrep,root);
    delete_constant_lists(this_sgrep);

    /* Free stuff:  */
	delete_string(expression);
    delete_flist(input_files);
    if (this_sgrep->index_reader) {
		delete_index_reader(this_sgrep->index_reader);
    }

    if (stats.output==0) {
		return n_results; /* Empty result list */
    }
    /* non empty result list */
    return n_results;
}


/*returns number actually found in put in array*/
int pr_display_gc_list(RegionList *l, int num_return, int is_el, int start, void **result_array, struct SgrepStruct *this_sgrep, process_book_ptr this_book_ptr) {
    ListIterator lp;
    Region r,p;
    int c=0;
    int region=0;  /* does what displayer->region would do */
    int el_sought, found_el;
    int found_pos;
    this_sgrep->print_all=1;
    start_region_search(l,&lp);
    get_region(&lp,&r);
    int *placeInt=(int *) ana_malloc(&(this_book_ptr->anastasia_memory), sizeof(int), __FILE__, __LINE__);
 	int *tempInt=placeInt;
    bounds_ptr placePtr = (bounds_ptr) ana_malloc(&(this_book_ptr->anastasia_memory), sizeof(struct bounds_obj), __FILE__, __LINE__); 
    bounds_ptr temp=placePtr;
    if (r.start>0 && this_sgrep->print_all)
    {
	/* There is no text before first region*/
    }
    if (r.start==-1 && this_sgrep->print_all) {
	/* There was no regions, but we are in filter mode: irrelevant */
    }
    while (r.start!=-1) {
		/* We don't do the output_style */
		p=r;
		get_region(&lp,&r);
		region++; 
		if (this_sgrep->print_all) {
	//		void *place;
	//		int *place=[];
		    if ((region>=start) && (region<start+num_return)) {
		    	/* There is text between two regions : we don't showregion */
			    /*here is where we pull out what we want: is_el==1, we are looking for an element*/
			    if (is_el==1)  {
			    	/*we must be getting back sgrepids.  pull them out, make them numbers, put them in result_array */
					//O.k. the anaid should come right after the element start so we shouldn't really need to check to see if its greater than 512
					//What we could do is search through the sgml_obj's to find the element number..
					//p.end = (this_sgml_obj->sgrep_start - 1);  //Obviously.. :)
			    	if (p.start<p.end) {
						get_el_sgp_rel_pos(&el_sought, p.start, this_sgrep->this_book);
//						write_log(apr_psprintf(this_book_ptr->current_pool,"tricky memoy pointer stuff 1 .. found element is: %d", el_sought));
						/*got an element number: add them to the results array*/
						//place: the pointer to the new array holding the results. Make it big enough to add this extra element
						if (c>0) {
							placeInt=(int *) ana_malloc(&(this_book_ptr->anastasia_memory), (c+1)*sizeof(int), __FILE__, __LINE__);
							for (int j=0; j<c; j++){   
                			   placeInt[j]=tempInt[j];
           					}
           					ana_free(&(this_book_ptr->anastasia_memory), tempInt);
						}
						placeInt[c]=el_sought;
//						write_log(apr_psprintf(this_book_ptr->current_pool,"tricky memoy pointer stuff 2 .. found element is: %d", placeInt[c]));
//						if (c>0) write_log(apr_psprintf(this_book_ptr->current_pool,"tricky memoy pointer stuff 2 .. fprevios ound element is: %d", placeInt[c-1]));
						tempInt=placeInt;
						*result_array=placeInt;
						c++;
				    } else {
				    	return 0;
				    }    			
				 }
				else if (is_el==2) { /*this is a word search */
					/*redone now I understand pointer arithmetic and memory etc */
					if (p.start<=p.end) {
						if (c>0) {
							placePtr= (bounds_ptr) ana_malloc(&(this_book_ptr->anastasia_memory), (c+1)*sizeof(struct bounds_obj), __FILE__, __LINE__);
							for (int j=0; j<c; j++){
								placePtr[j].start_el=temp[j].start_el;
								placePtr[j].end_el=temp[j].end_el;
								placePtr[j].start_off=temp[j].start_off;
								placePtr[j].end_off=temp[j].end_off;
							 }
							 ana_free(&(this_book_ptr->anastasia_memory), temp);
						}
						get_sgp_rel_pos(&found_el, &found_pos, p.start, this_sgrep->this_book);
						(placePtr)[c].start_el = found_el;
						(placePtr)[c].start_off = found_pos;
						get_sgp_rel_pos(&found_el, &found_pos, p.end, this_sgrep->this_book);
						(placePtr)[c].end_el = found_el;
						(placePtr)[c].end_off = found_pos + 1;
						temp=placePtr;
//					    write_log(apr_psprintf(this_book_ptr->current_pool, "tricky memory pointer stuff 2 .. found element is: %d, offsetis %d", placePtr[c].start_el,  placePtr[c].start_off));
						*result_array=placePtr;
						c++;
					} else {
			    		return 0;
					}
				}
			}
			if (region>=start+num_return) return c;
	 	}
 	/* There is text after last region : not for us! */
 	}
    return c;
}

/*num comes back with number actually found and put in sgElsIds array; nresults comes back with total found, which is also put in result of this */
int do_find_sg_el(char *search_str, int start, int *num, int *nresults, process_book_ptr this_book, pool *current_pool) {
	ParseTreeNode *root;
	struct PHRASE_NODE *p_list;
	FileList *input_files=NULL;
	struct SgrepStruct sgrep_instance;
	struct SgrepStruct *this_sgrep;
	SgrepString *expression;
	*nresults=0;
	/* First initialize the sgrep instance */
//	write_log(apr_psprintf(current_pool, "in do_find_sg_el main 458 %s", search_str ));
	memset(&sgrep_instance,0,sizeof(sgrep_instance));
	this_sgrep=&sgrep_instance;	
	this_sgrep->do_concat=1;
	this_sgrep->output_style=SHORT_OUTPUT; /* default is short */
	this_sgrep->open_failure=OPEN_FAILURE;
	this_sgrep->print_newline=1;
	this_sgrep->stdin_temp_file=NULL;	/* not read yet */
	this_sgrep->print_all=1;
	this_sgrep->chars_list=NULL;
	this_sgrep->scanner_type=SGML_SCANNER;
	this_sgrep->current_pool = current_pool;
	this_sgrep->index_file = this_book->fs_idx;
	this_sgrep->stream_mode = 1;
	this_sgrep->this_book = this_book;
	this_sgrep->word_chars = apr_psprintf(current_pool, "0-9a-zA-Z[]%c-%c", 128, 255);
	/*here is the pre-opened index file*/
	this_sgrep->index_reader=pr_new_index_reader(this_sgrep);
	this_sgrep->no_output=0;
	this_sgrep->display_count=0;

    /* 
     * Creating constant lists. They might be needed in the parse() step
     */
    create_constant_lists(this_sgrep);

	/*
	 * Shall we get expression from command line 
	 */
//	expression=read_expressions(this_sgrep,NULL);
	expression = new_string(this_sgrep,1024);
	string_cat(expression,"#line 1 \"\"\n");
    string_cat(expression, search_str);
	
	/* 
	 * Invoking parser 
	 */
	if ((root=parse_and_optimize(this_sgrep,expression->s,&p_list)) == NULL) {
		sgrep_error(this_sgrep,"No query to evaluate. Bailing out.\n");
		delete_string(expression);
		delete_region_list(this_sgrep->chars_list);
	    delete_flist(input_files);
		return 0;
	}
	input_files=index_file_list(this_sgrep->index_reader, (unsigned char *) this_book->lomem_idx->file_list);

    /*
     * Evaluation style depends on sgrep->stream_mode 
     */
    run_stream(input_files,root,p_list, nresults, start, num,  (void **) &this_book->search_els_found, 1, this_sgrep, this_book);
//	write_log(apr_psprintf(current_pool, "in do_find_sg_el main 515 %s", search_str ));
    free_parse_tree(this_sgrep,root);
    /* Free stuff:  */
	delete_string(expression);
	delete_region_list(this_sgrep->chars_list);
    delete_flist(input_files);
	delete_index_reader(this_sgrep->index_reader);
//	write_log(apr_psprintf(current_pool, "in do_find_sg_el main 524 %s", search_str ));

    if (stats.output==0) {
	return *nresults; /* Empty result list */
    }
    /* non empty result list */
    return *nresults;
}

/*this book is actually a ana_book_ptr .. but we dont want to complicate matters so we will just export it to ANastasia which understands such things*/
int do_find_sg_text(char *search_str, int start, int *num, int *nresults, process_book_ptr this_book_ptr, pool *current_pool) { 	
	ParseTreeNode *root;
	struct PHRASE_NODE *p_list;
	FileList *input_files=NULL;
	struct SgrepStruct sgrep_instance;
	struct SgrepStruct *this_sgrep;
	SgrepString *expression;
	*nresults=0;
	/* First initialize the sgrep instance */
	memset(&sgrep_instance,0,sizeof(sgrep_instance));
	this_sgrep=&sgrep_instance;	
	this_sgrep->do_concat=1;
	this_sgrep->output_style=SHORT_OUTPUT; /* default is short */
	this_sgrep->open_failure=OPEN_FAILURE;
	this_sgrep->print_newline=1;
	this_sgrep->stdin_temp_file=NULL;	/* not read yet */
	this_sgrep->print_all=1;
	this_sgrep->chars_list=NULL;
	this_sgrep->scanner_type=SGML_SCANNER;
	this_sgrep->current_pool = current_pool;
	this_sgrep->index_file = this_book_ptr->fs_idx;
	this_sgrep->stream_mode = 1;
	this_sgrep->this_book = this_book_ptr;
	this_sgrep->word_chars = apr_psprintf(current_pool, "0-9a-zA-Z[]%c-%c", 128, 255);
	/*here is the pre-opened index file*/
	this_sgrep->index_reader=pr_new_index_reader(this_sgrep);
	this_sgrep->no_output=0;
	this_sgrep->display_count=0;

    /* 
     * Creating constant lists. They might be needed in the parse() step
     */
    create_constant_lists(this_sgrep);
	
	/*
	 * Shall we get expression from command line 
	 */
//	expression=read_expressions(this_sgrep,NULL);
	expression = new_string(this_sgrep,8192);
	if (expression==NULL) {
		return 0;
	}
	string_cat(expression,"#line 1 \"\"\n");
    string_cat(expression, search_str);	
		
	/* 
	 * Invoking parser 
	 */
	if ((root=parse_and_optimize(this_sgrep,expression->s,&p_list)) == NULL) {
	    sgrep_error(this_sgrep,"No query to evaluate. Bailing out.\n");
		delete_region_list(this_sgrep->chars_list);
		delete_string(expression);
		delete_index_reader(this_sgrep->index_reader);
	    return 0;
    }
	delete_string(expression);
	input_files=index_file_list(this_sgrep->index_reader, (unsigned char *) this_book_ptr->lomem_idx->file_list);

	run_stream(input_files,root,p_list, nresults, start, num, (void **) &this_book_ptr->search_texts_found, 2, this_sgrep, this_book_ptr);
//	write_log("back here successfully");
    free_parse_tree(this_sgrep,root);

    /* Free stuff:  */
	delete_region_list(this_sgrep->chars_list);
    delete_flist(input_files);
	delete_index_reader(this_sgrep->index_reader);
    if (stats.output==0) {
	return *nresults; /* Empty result list */
    }
    /* non empty result list */
    return *nresults;
}
