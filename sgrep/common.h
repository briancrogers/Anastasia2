
typedef struct {
    int start;	/* Start index of a file */
    int length;	/* Length of a file */
    char *name;	/* Name of the file, NULL if stdin */
} OneFile;

struct FileListStruct {
    SgrepData *sgrep;
    int total_size;   /* Total length of all files in bytes */
    int num_files;    /* How many files */
    int allocated;    /* How many OneFile entries allocated */
    OneFile *files;   /* Since this list must be binary searchable, files
		       * are kept in array instead of linked list */
    int last_errno;   /* Remember the last error in add() */
    int progress_limit; /* When to show progress */
};
#ifdef WIN32
//this causes redefinition errors in other environments
typedef struct FileListStruct FileList;
#endif

enum SortTypes {SORT_BY_START,SORT_BY_END };

void delete_region_list(RegionList *l);
#define free_gclist(LIST) delete_region_list(LIST);
int sgrep_error(SgrepData *this_sgrep,char *format, ...);
int flist_files(const FileList *list);
RegionList *new_region_list(SgrepData *this_sgrep);
void delete_list_node(SgrepData *this_sgrep, ListNode *this_node);
void to_chars(RegionList *c,int chars, int end);
void string_cat(SgrepString *s, const char *str);
SgrepString *new_string(SgrepData *this_sgrep,size_t size);
void delete_string(SgrepString *s);
ParseTreeNode *parse_and_optimize(SgrepData *this_sgrep,const char *query, struct PHRASE_NODE **phrases);
void delete_flist(FileList *list);
void start_region_search(RegionList *l, ListIterator *handle);
FileList *new_flist(SgrepData *this_sgrep);
void flist_add_known(FileList *ifs, const char *name, int length);
void flist_ready(FileList *ifs);
void init_region_list(RegionList *l);
ListNode *get_start_sorted_list(RegionList *s);
ListNode *new_list_node(SgrepData *this_sgrep);
ListNode *copy_list_nodes(SgrepData *this_sgrep,const ListNode *n, ListNode **return_last);
ListNode **create_node_array(const RegionList *s, ListNode *n);
void gc_qsort(ListNode **inds,int s,int e, enum SortTypes st);
int flist_start(const FileList *list, int n);
int flist_total(const FileList *list);
void insert_list_node(RegionList *l);
void start_end_sorted_search(RegionList *l, ListIterator *handle);
void list_set_sorted(RegionList *l, enum RegionListSorted sorted);
enum RegionListSorted list_get_sorted(const RegionList *l);
void list_require_start_sorted_array(RegionList *l, struct SgrepStruct *this_sgrep);
void remove_duplicates(RegionList *s);
void start_region_search_from(RegionList *l, int index, ListIterator *handle);
ListNode *get_end_sorted_list(RegionList *s);
void string_cat_escaped(SgrepString *escaped, const char *str);
void real_string_push(SgrepString *s, SgrepChar ch);
SgrepString *expand_backslashes(SgrepData *this_sgrep,const char *s);
SgrepString *init_string(SgrepData *this_sgrep, size_t size, const char *src);
void push_front(SgrepString *s,const char *str);
int expand_backslash_escape(SgrepData *this_sgrep, const unsigned char *list, int *i);
void string_toupper(SgrepString *s, int from);
void string_tolower(SgrepString *s, int from);
const char *string_escaped(SgrepString *str);
int flist_exists(FileList *list, const char *name);
const char *flist_name(const FileList *list, int n);
int flist_length(const FileList *list, int n);

