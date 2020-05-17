/*
	System: Structured text retrieval tool sgrep.
	Module: sgrep.h
	Author: Pekka Kilpeläinen & Jani Jaakkola
	Description: Common data structures, definitions & macros for
		     all modules.
	Version history: Original version February 1995 by JJ & PK
	Copyright: University of Helsinki, Dept. of Computer Science
		   Distributed under GNU General Public Lisence
		   See file COPYING for details
*/

#ifndef WIN32
#define HAVE_CONFIG_H
#endif
#ifndef SGREP_H_INCLUDED
#define SGREP_H_INCLUDED

#ifdef HAVE_CONFIG_H
	#include "config.h"
	#include "acconfig.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#include "../ana_common.h"
#include "../ana_utils.h"
#include "../ana_browsegrove.h"
#include "sysdeps.h"

/* Environment variable for specifying options*/
#define ENV_OPTIONS "SGREPOPT"

/*
 * Get the tmp file directory from this environment variable
 */
#define ENV_TEMP "TEMP"

/*
 * For temp file name generation
 */
#define TEMP_FILE_PREFIX "sg"
#define TEMP_FILE_POSTFIX ".tmp"

/* 
 * If this is TRUE then failure to open a file is considered fatal
 */
#define OPEN_FAILURE 0

/*
 * This spesifies the default amount of RAM-memory available for indexer
 *experiment with different values...
 * (-I -m <mem> switch)
 */
#define DEFAULT_INDEXER_MEMORY (20*1024*1024) /* 20 megabytes */
/*
 * The hash table size for indexer term entries
 making it lots bigger...maybe we need to revise the whole hash function
 this value is a prime one less than 2 power of 18
 */
#define DEFAULT_HASH_TABLE_SIZE 60013

/* 
 * The default output styles
 */
#define LONG_OUTPUT ("------------- #%n %f: %l (%s,%e : %i,%j)\\n%r\\n")
#define SHORT_OUTPUT ( "%r" )


#ifdef __cplusplus
extern "C" {
#endif


/* 
 * This turns on debugging output for all modules. You really don't want
 * to that. Instead define it in the place which you are debugging.
 */
/*#define DEBUG*/

#define SGREP_ERROR -1
#define SGREP_OK 0

/* These are the constant labels */
#define LABEL_NOTKNOWN -1
#define LABEL_CONS 0
#define LABEL_CHARS 1
#define LABEL_PHRASE 2
#define LABEL_FIRST 3

#define LIST_NODE_BITS 7    /* Size of a list node in bits */

/* Sice LIST_NODE_SIZE must be power of 2. So it is calculated from
 * LIST_NODE_BITS 
 */
#define LIST_NODE_SIZE ( 1 << LIST_NODE_BITS )

/*
 * All operators. These are used in the parse tree 
 */

enum Oper { SGIN,NOT_IN,CONTAINING,NOT_CONTAINING,
	    EQUAL, NOT_EQUAL, /* PK Febr 12 '96 */
	    ORDERED,L_ORDERED,R_ORDERED,LR_ORDERED,
	    QUOTE,L_QUOTE,R_QUOTE,LR_QUOTE,
	    EXTRACTING,
	    OR,
	    PARENTING, CHILDRENING,
	    SGNEAR, NEAR_BEFORE,
	    OUTER,INNER,CONCAT,
	    JOIN,FIRST,LAST,
	    FIRST_BYTES,LAST_BYTES,
	    PHRASE,
	    INVALID };

/* 
 * Struct for non strings with length.
 */
typedef int SgrepChar;
typedef struct SgrepStringStruct {
    struct SgrepStruct *sgrep;
    size_t size;
    size_t length;
    char *s; /* This contains the sgrep internal encoding of the possibly
	      * 32-wide character */
/* This contains the same string with escape sequences
 * if string_escape() bhas been called */
    struct SgrepStringStruct *escaped;
} SgrepString;
    
/*
 * One region has a start point and a end point 
 */
typedef struct RegionStruct {
	int start;
	int end;
} Region;

/* 
 * One gc node has table for regions and a pointers to next and previous
 * nodes 
 */
typedef struct ListNodeStruct {
	Region list[LIST_NODE_SIZE];
	struct ListNodeStruct *next;
	struct ListNodeStruct *prev;
} ListNode;

/*
 * A pointer to a GC_NODE in a gc list. Used for scanning gc lists.
 */
typedef struct {
        /* list which we are scanning */
	const struct RegionListStruct *list;	
	ListNode *node;	                /* Points out the node */
	int ind;		        /* Index into a node */
} ListIterator;

/*
 * Structure for whole gc list 
 */
enum RegionListSorted { NOT_SORTED, START_SORTED, END_SORTED };
typedef struct RegionListStruct {
    struct SgrepStruct *sgrep;
    int nodes;			/* how many nodes there are */
    int length;			/* How many regions in last node */
    int chars;			/* When we have chars list, this
				   tells from how many characters
				   it is created */
    int refcount;		/* How many times this list is referenced */
    int nested;			/* This list maybe nested */
    enum RegionListSorted sorted;
    int complete;               /* The operation that produced
				 * this list has completed. This
				 * list *may not be changed* anymore */
    ListNode *first;            /* First node in the gc list
				 * NULL means we have an optimized list
				 *  (chars have been used) 
				 */
    ListNode *last;             /* Last node of start sorted list */
    ListNode *end_sorted;       /* If there is a end_point sorted 
				 *  version of this list, this points
				 * to it. Otherwise NULL. */
    struct  RegionListStruct *next; /* We may need to make lists out of gc lists */
    ListNode **start_sorted_array; /* If this region list needs to an
				    * array as well as list*/

} RegionList;

/*
 * Leaves of parse tree are always phrases. 
 */
typedef struct PHRASE_NODE {
    struct PHRASE_NODE *next;	/* Phrase nodes are kept in list for
				   the creation of AC automate */
    SgrepString *phrase;	/* The phrase string */ 
    RegionList *regions;	/* Region list containing matching regions */ 
    /* Pointer to the parse tree node containing this phrase */
    struct ParseTreeNodeStruct *parent;
} ParseTreeLeaf;

/*
 * Node of a parse tree 
 */
typedef struct ParseTreeNodeStruct {
    enum Oper oper;             /* operand */
    
    /* The usual relatives */
    struct ParseTreeNodeStruct *parent;
    struct ParseTreeNodeStruct *left;
    struct ParseTreeNodeStruct *right;

    int label_left;		/* Needed for optimizing */
    int label_right;	        /* me2 */

    int refcount;               /* Parents after common subtree elimination */
    RegionList *result;	        /* If the subtree has been evaluated, this
				 * contains value */
    
    int number;			/* Functions may have int parameters */
    ParseTreeLeaf *leaf;        /* Points to Leaf if this is */
} ParseTreeNode;

/* Opaque FileListStruct for managing lists of sgrep input files
 * defined in common.c */   
struct FileListStruct;
typedef struct FileListStruct FileList;

/* Opaque struct for managing temporary files */
typedef struct TempFileStruct TempFile;

/*
 * Struct for gathering statistical information 
 */
struct Statistics {
    int phrases;	      /* How many phrases found */

    /* Evaluation statistics */
    int operators_evaluated;  /* Total number of operators evaluated */

    int order;		      /* How many operations */
    int or_oper;              /* g++ -ansi complain about plain or... */
    int in;
    int not_in;
    int equal;		      /* PK Febr 12, '96 */
    int not_equal;	      /* PK Febr 12, '96 */
    int containing;
    int not_containing;	
    int extracting;
    int quote;
    int inner;
    int outer;
    int concat;
    int join;
    int parenting;
    int childrening;
    
    /* Statistics about region lists */
    int region_lists;	    /* Number of region lists created */
    int constant_lists;	    /* Number of constant lists */
    int region_lists_now;   /* Number of region lists in use */
    int gc_nodes;	    /* Number of gc_nodes used */
    int gc_nodes_allocated; /* Number of gc_nodes malloced */
    int longest_list;	    /* Longest gc list used */
    int output;		    /* Size of output list */
    int scans;		    /* Number of started scans */
    int scanned_files;      /* Number of scanned files */
    int scanned_bytes;      /* Scanned bytes total */
    int sorts_by_start;	    /* Number of sorts by start points */
    int sorts_by_end;	    /* Number of sorts by end points */
#ifdef OPTIMIZE_SORTS
    int sorts_optimized;	  /* How many sorts we could optimize away */
#endif
    int remove_duplicates;	  /* Number of remove_duplicates operations */
    /* Statistics about the query and it's optimization */
    int parse_tree_size;	  /* Parse tree size */
    int optimized_phrases;        /* How many times we had same phrase */
    int optimized_nodes;	  /* How many parse tree nodes optimized */

    int input_size;		  /* Size of given input in bytes */
    
    /* The memory debugging information is only available if sgrep was
     * compiled with memory-debugging enabled */
#if MEMORY_DEBUG
    int memory_blocks;        /* How many memory blocks allocated now */
    size_t memory_allocated;  /* How much memory allocated now */
    size_t peak_memory_usage; /* Memory usage at worst */
    int reallocs;	      /* how many times memory has been reallocated */
#endif
};

/* 
 * Some handy macros
 */

/*
 * Macro for finding out gc list size 
 */
#define LIST_SIZE(LIST)	(((LIST)->nodes-1)*LIST_NODE_SIZE+(LIST)->length)
/*
 * Macto for last node in list
 */
#define LAST_NODE(LIST) ((LIST)->last->list[(LIST)->length-1])

/* 
 * These are for speeding up list scanning and creation.
 * get_region, add_region and prev_region are the most used
 * functions.
 */

/* Macro for indexing into gc list. */
#define LIST_RNUM(INDS,IND)	( (INDS)[ (IND)>>LIST_NODE_BITS ]-> \
				list[ (IND)& ((1<<LIST_NODE_BITS)-1) ] )

#define add_region(L,S,E) ADD_REGION_MACRO((L),(S),(E))
#define get_region(handle,reg) GET_REGION_MACRO((handle),(reg))
#define prev_region(handle,reg) PREV_REGION_MACRO((handle),(reg))
#define region_at(list,ind,region) REGION_AT_MACRO((list),(ind),(region))
#define region_lvalue_at(second_list, second_i) REGION_LVALUE_AT_MACRO((second_list), (second_i))

#define REGION_AT_MACRO(list,ind,region) \
     do { (*region)=LIST_RNUM((list)->start_sorted_array,(ind)); } while(0)

#define REGION_LVALUE_AT_MACRO(list,ind) \
     (LIST_RNUM((list)->start_sorted_array,(ind)))

#define ADD_REGION_MACRO(L,S,E)	do { \
    if ( (L)->length==LIST_NODE_SIZE ) insert_list_node(L); \
    (L)->last->list[(L)->length].start=(S); \
    (L)->last->list[(L)->length].end=(E); \
    (L)->length++; \
} while (0)

#define GET_REGION_MACRO(handle,reg)	\
do { \
	if ( (handle)->node==NULL || (handle)->node->next== NULL ) \
	{ \
		if ((handle)->ind==(handle)->list->length) \
		{ \
			(reg)->start=-1; \
			(reg)->end=-1; \
			break; \
		} \
	 	if ((handle)->list->last==NULL) /* chars list */ \
		{ \
			(reg)->start=(handle)->ind; \
			(reg)->end=(handle)->ind+(handle)->list->chars; \
			(handle)->ind++; \
			break; \
		} \
	} \
	if ( (handle)->ind==LIST_NODE_SIZE ) \
	{ \
		(handle)->node=(handle)->node->next; \
		(handle)->ind=0; \
	} \
	*(reg)=(handle)->node->list[(handle)->ind++]; \
} while(0)

#define PREV_REGION_MACRO(handle,reg) \
do { \
	if ( (handle)->node==NULL || (handle)->node->prev==NULL) \
	{ \
		if ((handle)->ind==0) \
		{ \
			(reg)->start=-1; \
			(reg)->end=-1; \
			break; \
		} \
		if ((handle)->list->first==NULL) \
		{ \
			(handle)->ind--; \
			(reg)->start=(handle)->ind; \
			(reg)->end=(reg)->start+(handle)->list->chars; \
			break; \
		} \
	} \
	if ( (handle)->ind==0 ) \
	{ \
		(handle)->node=(handle)->node->prev; \
		(handle)->ind=LIST_NODE_SIZE; \
	} \
	*(reg)=(handle)->node->list[--(handle)->ind]; \
} while(0)


/* Backward compatibility hack */
#define stats (this_sgrep->statistics)

typedef enum { SGML_SCANNER, XML_SCANNER, TEXT_SCANNER } ScannerType;

/* The default is to GUESS
 * For XML sgrep guesses UTF8, all others 8-BIT.
 * For XML the encoding declaration is honored, if there is one.
 */
enum Encoding {ENCODING_GUESS, ENCODING_8BIT, ENCODING_UTF8, ENCODING_UTF16};

/*
 * Sgrep has no global variables (outside of main.c), since there exists
 * also a library version of sgrep, which can actually create multiple
 * sgrep instances in different threads
 */
struct MemoryBlockStruct; /* Opaque. See common.c */
typedef struct SgrepStruct {
    char *index_file;                 /* If we have an index_file, this is it's name */
    int recurse_dirs;                 /* Should we recurse into subdirectories (-R) */

    struct Statistics statistics;     /* here we gather statistical					                 information */
    int do_concat;    /* Shall we do concat operation on result list (-d) */
    /* Current IndexReader instance */
    struct IndexReaderStruct *index_reader;
    void (*progress_callback)(void *data,
			    int files_processed, int total_files,
			    int bytes_processed, int total_bytes);   
    void *progress_data;
    apr_file_t *progress_stream; /* stream to write progress reports */
    int progress_output;   /* Should we write progress output */

    SgrepString *error;
    apr_file_t *error_stream;
    char *word_chars;

    char *output_style;		/* String containing the output style
				   default is DEFAULT_OUTPUT */
    int open_failure;		/* So if file that can't be opened
				   is considered to be fatal
				   defaults to OPEN_FAILURE (above)*/
    int print_newline;		/* Shall we print newline at the end of output */
    int print_all;		/* If sgrep is used as a filter */
    int stream_mode; 	        /* Input files considered a stream (-S) */
    
    /* Pmatch.c stuff */
    ScannerType scanner_type;
    int ignore_case;                  /* Ignore case distinctions in phrases */

    /* Default encoding */
    enum Encoding default_encoding;
    /* SGML-stuff */
    int sgml_debug;              /* Enables SGML-scanner debugging */
    int include_system_entities; /* Should scanner include system entities */
	int no_output;				//Moved from global variables
	int display_count;			//Moved from global variable
	int option_space;			//Moved from global variable
	/* The historical remain, chars list */
    RegionList *chars_list; 
    /* Points to list of temporary files */
    TempFile *first_temp_file;
    /* The temp file to which stdin is read */
    TempFile *stdin_temp_file;
	process_book_ptr this_book;
	pool *current_pool;
} SgrepData;

#define MAX_FILE_LIST_FILES 64


/*
 * global function prototypes 
 */

/* Memory management. Beware: I've used some mighty ugly macro magic when
 * debugging sgrep memory usage. Don't do this at home */
#define SGREPDATA(STRUCT) SgrepData *this_sgrep=((STRUCT)->sgrep)
#define sgrep_new(TYPE) ((TYPE *)sgrep_malloc(sizeof(TYPE)))

/*put apache memory defines here... we force failure here*/
#define sgrep_strdup(X) ap_pstrdup(pool, (X));
#define sgrep_malloc(X) ap_pcalloc(pool, (X));
#define sgrep_calloc(X,Y) ap_pcalloc(pool, X, Y);
#define sgrep_realloc(X,Y) ana_ap_realloc(pool,(X),(Y))
#define sgrep_free(X);

/* Interface to index modules */
struct IndexReaderStruct;
typedef struct IndexReaderStruct IndexReader;
struct IndexWriterStruct;
struct IndexEntryListStruct;
typedef struct IndexEntryListStruct IndexEntryList;
struct IndexEntryStruct;
typedef struct IndexEntryStruct IndexEntry;

/*
 * Indexer Options
 */
enum IndexModes {IM_NONE,IM_CREATE,IM_TERMS,IM_DONE};
typedef struct {
    struct SgrepStruct *sgrep;
    enum IndexModes index_mode;
    int index_stats;     /* Display index statistics? */
    int stop_word_limit;
    const char *input_stop_word_file;
    const char *output_stop_word_file;
    int hash_table_size;
    int available_memory;
    FileList *file_list_files;
    FileList *file_list;
    const char *file_name;
} IndexOptions;

/* Interface to SGML scanner module */
struct SGMLScannerStruct;
typedef struct SGMLScannerStruct SGMLScanner;

#define string_clear(S) ((S)->length=0)
#define string_truncate(S,LEN) ((S)->length=(LEN))
#define string_to_char(S) ((S)->s[(S)->length]=0,(const char *)(S)->s)
#define string_len(STR) ((STR)->length)

/* #define string_push(STR,CHAR) real_string_push((STR),(CHAR)) */
#define string_push(STR,CHAR) do { \
if ((STR)->length<(STR)->size && (CHAR)<255) { \
        (STR)->s[(STR)->length]=(CHAR); \
        (STR)->length++; \
    } else { real_string_push((STR),(CHAR)); } \
} while (0)


/* Interface to output module */
typedef struct DisplayerStruct Displayer;

/*
 * Temporary file handling. These functions might seem to be overkill,
 * but i wanted to have portable and reliable temp file handling..
 * The ANSI tmpfile functions we're not enough */
struct TempFileStruct {  
    SgrepData *sgrep;
    char *file_name;
    FILE *stream;
    TempFile *next;
    TempFile *prev;
};
#ifdef WIN32
//this causes redefinition errors in other environments
typedef struct TempFileStruct TempFile;
#endif

#ifdef __cplusplus
}
#endif

#endif /* SGREP_H_INCLUDED */


TempFile *create_temp_file(SgrepData *this_sgrep);
FILE *temp_file_stream(TempFile *temp_file);
TempFile *create_named_temp_file(SgrepData *this_sgrep);
int delete_temp_file(TempFile *temp_file);
int unmap_file(SgrepData *this_sgrep,void *map, size_t size);
size_t map_file(SgrepData *this_sgrep,const char *filename,void **map);

