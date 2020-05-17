//Apache includes
#include "httpd.h"
#include "http_config.h"
#include "http_core.h"
#include "http_log.h"
#include "http_protocol.h"
#include "http_main.h"
//Tcl includes
#include "tcl.h"
#include "ana_config.h"
#include "ana_memory.h"
#if defined(WIN32) || defined(LINUX)
#include "bitstuff.h"
#else
#include <bitstring.h>
#endif

#if defined(WIN32)
#include <winsock2.h>
#endif
#if defined(ANA_MYSQL)
	#include <mysql/mysql.h>
#endif

//Our external.. the one and only global variable (or should be)
extern struct anastasia_information anastasia_info;


//Some typedefs
//Right we need two type, for integer data held in memory
typedef unsigned long int ulint;
//And one pointers to memory
typedef unsigned long mint;

#if !defined(false)
	#define false 0
#endif

#if !defined(true)
	#define true 1
#endif

//Some structures and theres associative typedefs

struct text_o {
	char *ptr;
	int len;
	int allocated;
	pool *text_o_pool;
	pool *parent_pool;
};
typedef struct text_o text_obj;
typedef struct text_o *text_ptr;

struct buffer_o {
	char *ptr;
	int len;
	pool *buffer_obj_pool;
	pool *parent_pool;
};
typedef struct buffer_o buffer_obj;
typedef struct buffer_o *buffer_ptr;

typedef struct attr_obj *attr_ptr;

struct attr_obj {
	text_ptr name;
	text_ptr value;
	attr_ptr nattr;
	attr_ptr pattr;
};

struct bounds_obj {
	int start_el;
	int start_off;
	int end_el;
	int end_off;
};

struct search_inf {
	int nelements_found;  /*Number returned in last elements search. so we don't go out of scope */
        int ntexts_found; /*number texts found in last sgtext search */
	int ntexts_done;  /*how many we have dealt with in search_texts_found*/
	int nelements_done;  /*how many we have dealt with in search_els_found*/
	int start_el; /*here we put info for the NEXT fragment of text we should deal with*/
	int start_off;
	int end_el;
	int end_off;
} ;

typedef struct bounds_obj *bounds_ptr;

typedef struct hash_lilo_item_inf *hash_lilo_ptr;
struct hash_lilo_item_inf 
{
	pool *hash_pool;
	void *comp_str;
	int comp_int;
	int allocated;
	hash_lilo_ptr previous_lilo; /*what is before in the lilo*/
	hash_lilo_ptr next_lilo;
	hash_lilo_ptr previous_hash; /*what is before in the hash*/
	hash_lilo_ptr next_hash;
	int check;
	void *the_data;  /*all the data associated with this object.*/
};

typedef struct process_book_inf *process_book_ptr;

struct hash_lilo_obj_inf 
{
	hash_lilo_ptr first_lilo;
	hash_lilo_ptr last_lilo;
		/*when we want a new one: this does the call.  Notice that you have to tell 
		it how much memory there is left; if it fails because of lack of capacity,
		then it returns with success set to true but returns a null ptr.  Success=o is 
		reserved for disasters in the system */
	void *(*callback_init)(void *mydata, int *allocated, void *sought, process_book_ptr this_book_ptr); 
	void *(*callback_delete)(void *mydata, int *reclaimed, void *sought, process_book_ptr this_book_ptr); 
	int capacity; /* hoow much memory do I want to be used by this?*/
	pool *object_pool;
	int current;
	int htable_size; 
	hash_lilo_ptr *htable;
	int number;
};
typedef struct hash_lilo_obj_inf *hash_lilo_obj_ptr;

struct sgml_obj {
	int number;
	int ancestor;
	int lsib;
	int rsib;
	int child;
	int sgrep_start;
	int sgrep_end;
	int cont_type;
	text_ptr gi;
	int n_attrs;
	attr_ptr attrs;
	text_ptr path;
	text_ptr pcdata;  /*dunno whether we will use this (useful in toc contexts?) */
	text_ptr bookname;
	int start_data;
	int end_data;
	hash_lilo_ptr hashptr;
	pool *object_pool;	//A pool unique to this sgml object
	//Need to be able to check to see if we dealing with a clone
	int is_clone;
};
typedef struct sgml_obj *sgml_obj_ptr;

struct lomem_inf {
	int len;    
	int array_offset;
	int entries_offset;
	void *file_list;
};
typedef struct lomem_inf *lomem_index;

/* for each style we load: make one of these */
struct style_obj {
	char *file_name;
	char *book_name;  /*book this belongs to  */
	//O.k. now the main style will only use the file_spec for the begin function after that it will just right straight to the rec,
	//all other styles will have a file_spec which they can write too.
	buffer_ptr file_spec;
	request_rec *rec;
	Tcl_Interp* tcl_interp;
	int outputSGML;
	int defoutputSGML;
	int outputMax;
	char *startSGML;
	char *endSGML;
	char *closeSGML;
	char element_name[200];
	int outputTOC;
	int outputSiblings;
	int outputRight;
	pool *style_pool;
	int globalHttppost;
	int timeout;
};
typedef struct style_obj *style_ptr;

//used by vbase mechanisms -- this has to be multiple of 8
struct search_vbase_inf {
	int rdg_id;   /*name of the book*/
	char type[7];
	bitstr_t vlist[bitstr_size(MAXMSS)]; 
};
typedef struct search_vbase_inf *search_vbase_ptr;

//modelled on the collate original, upgraded for this life we lead
struct srch_cond_inf
{
        int cond_mss[MAXMSS + 1]; /* use this to hold mss, referred to by number, which are in our search request */
        int num_cond;
        int seek_mss;
        int nom_mss;
        char mss[256];
        char num_str[32];
        char var_type[32];
        int others;
        int in_out;
        int active;
};
typedef struct srch_cond_inf *cond_ptr;
//end section for variant database

//this is for the local copy of the book instance kept in each style interpreter process instance
//17/11/2002 - Updated.. before the tcl style where still held in the original anastasia_book_inf
//but this could cause problems in a threaded inviroment and doesn't seem to have much point in a
//process inviroment.
struct process_book_inf {
	pool *current_pool;
	char *bookname;   /*name of the book*/
	char *dir;  /*directory to get us where are ngv sgv etc files*/
						/*if blank: must be in the same directory as the application*/
						/*must be set in AnaRead.cfg: book name is direc*/
	apr_file_t *in_ngv; /*ngv file stream*/
	apr_file_t *in_sgv; /*sgv file stream*/
	apr_file_t *in_rgv; /*rgv file stream*/
	apr_file_t *in_idx; /*sgp file stream for reading index */
        apr_file_t *in_output; /*output file for when in standalone mode */
	void *map_idx;  /*sgrep index for this book*/
	long map_len;  /*sgrep length of index for this book*/
	lomem_index lomem_idx; /*defined in anacommon.h */
	char *fs_ngv ;
	char *fs_sgv;
	char *fs_rgv;
	char *fs_idx;
	char *fs_output;
	hash_lilo_obj_ptr sgml_objects;
	hash_lilo_obj_ptr index_objects;
	//Store the request record
	request_rec *request_rec;
	char *http_post_args;
	//Search information;
	struct search_inf current_search;
	int in_search;
	void *search_els_found;
	void *search_texts_found;
	int current_element_no;
	Tcl_Interp *book_interp;
	style_ptr styles[NUMSTYLES];
	//Have a memory pool for this pool
	struct ana_memory anastasia_memory;
	size_t no_elements;
	int rgv_size;
	time_t starttime;
	char *vbase;
	int n_rdgs;
	int n_mss;
	int record_size;
	text_obj sigil_key[MAXMSS];
#if defined(ANA_MYSQL)
	MYSQL database;
#endif
	//0 not hide
	//1 hide children
	//2 hide content
	int hide;
	table *cookies;
	table *headers;
	char *content_type;
};

//A structure to hold global information about anastasia
struct anastasia_information {
	pool *server_pool;
	server_rec *server_rec;
	char *path;
	int big_endian;
	int platform;
	int standalone;
	time_t ana_start_time;
	int access_control;
	int ac_default_deny;
	int ac_log;
	char ac_host[512];
	char ac_username[512];
	char ac_password[512];
	char ac_database[512];
	char anastasia_root[512];
	char anastasia_url[512];  //added by Troy?
};

struct IndexFileRead  {
	pool *index_pool;
	unsigned char *the_str;
	int all_read;
	int place;
	int allocated;
};
typedef struct IndexFileRead *IndexFileReadPtr;

struct global_book_inf {
	char book[128];   /*name of the book*/
	int nhits;
	int max_sgml_objs;
	int max_sgml_mem;
	int max_index_objs;
	int max_index_mem;
	int nerrors;
};
typedef struct global_book_inf *global_book_ptr;
typedef struct global_book_inf  global_book_arr[NUMBOOKS];
typedef global_book_arr *global_book_arr_ptr;

struct lock_inf {
	int lock;
	time_t time;
};


struct ana_mem_info {
	char file[1024];
	int lineno;
	int count;
	int size;
};
typedef struct ana_mem_info *ana_mem_info_ptr;

