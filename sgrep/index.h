#define INDEX_VERSION_MAGIC ("sgrep-index v0")
#define max_term_len 256
#define INTERNAL_INDEX_BLOCK_SIZE 12
#define NEGATIVE_NUMBER_TAG ((unsigned char)255)
#define END_OF_POSTINGS_TAG ((unsigned char)127)
#define DOT_REGIONS (1<<17) /* 65536*2 */
#define MAX_MEMORY_LOADS 256
#define INDEX_BUFFER_ARRAY_SIZE 1024
#define MAX_INDEX_SIZE (INT_MAX)
#define EXTERNAL_INDEX_BLOCK_SIZE 32

struct IndexReaderStruct {
    SgrepData *sgrep;
    const char *filename;
    void *map;
    size_t size;
    int len;    
    const unsigned char *array;
    const void *entries;    
};

struct IndexBlock {
    int next;
    unsigned char buf[EXTERNAL_INDEX_BLOCK_SIZE];
};

/*
 * Used for reading index postings
 */
struct SortingReaderStruct {
    Region *regions[32];
    int sizes[32];
    int lists_merged;
    int regions_merged;
    int max;

    Region one;
    Region *saved_array;
    int saved_size;
    int dots;
};

/*
 * Looking up something in index requires one of these
 */
struct LookupStruct {
    SgrepData *sgrep;
    const char *begin;
    const char *end;
    IndexReader *map;
    void (*callback)(const char *str, const unsigned char *regions, 
		     struct LookupStruct *data);    
    int stop_words;
    union {
	/* This one is for looking up only entries */
	struct IndexEntryListStruct *entry_list;
	/* This one is for creating possibly unsorted region list from all
	 * postings */
	RegionList *reader;
	/* This is used for sorting postings while reading them */
	struct SortingReaderStruct sorting_reader;
	/* This is for dumping postings to a file stream */
	FILE *stream;
    } data;
};

struct ExternalIndexBuffer {
    int first;
    int current;
    int bytes;
};

struct IndexBufferStruct {
    char *str;
    struct IndexBufferStruct *next;
    union {
	/* This struct is used when building index and all entries of a 
	 * term fit inside IndexBuffer */
	struct {
	    unsigned char ibuf[INTERNAL_INDEX_BLOCK_SIZE];
	} internal;
	/* This struct is used when building index and entries of a term
	 * do not fit inside IndexBuffer */
	struct ExternalIndexBuffer external;
	/* This is used, when reading index from a file */
	struct {
	    const unsigned char *buf;
	    int ind;
	} map;
    } list;
    /* Last index added to this buffer. This will be zero when the buffer
     * is created, INT_MAX when the buffer has been scanner and -1
     * if this buffer corresponds to stop word (and therefore is not used )
     */
    int last_index;
    int saved_bytes; /* How many bytes of this entry have been saved to a
		      * memory load file */
    /* block_used will >= 0 for internal buffer, <0 for external buffers
     * and SHORT_MIN  when reading */
    short block_used;
    short last_len;
    unsigned char lcp;
};
typedef struct IndexBufferStruct IndexBuffer;

struct IndexBufferArray {
    IndexBuffer bufs[INDEX_BUFFER_ARRAY_SIZE];
    struct IndexBufferArray *next;
};
typedef struct IndexWriterStruct {
    struct SgrepStruct *sgrep;
    
    const IndexOptions *options;

    /* FileList of the indexed files */
    FileList *file_list;

    /* To speed up index buffer allocation and to reduce memory usage of
     * the index buffers, they are allocated in chunks. */
     /*altered for new style indexbuffer stubs*/
	struct IndexBufferArray *free_index_buffers;
    int first_free_index_buffer;

    /* Points to hash table of IndexBuffers when scanning indexed files */
    int hash_size; /* Size of the hash table */
    IndexBuffer **htable;
    /* Points to list of sorted IndexBuffers when writing index file */
    IndexBuffer *sorted_buffers;
  
    /* Size, usage and pointer to postings spool (the one in main memory) */
    int spool_size;
    int spool_used;
    struct IndexBlock *spool;
	/* Array of memory load files */
    TempFile *memory_load_files[MAX_MEMORY_LOADS];
    int memory_loads;
    
    /* The stream to which index is written */
    FILE *stream;

    
    /* Statistics */
    int terms;
    int postings;
    int total_postings_bytes;
    int total_string_bytes;
    int strings_lcps_compressed;
    int entry_lengths[8];
    int flist_start;
    int flist_size;
    int total_index_file_size;

    int failed;
} IndexWriter;

IndexReader *pr_new_index_reader(SgrepData *this_sgrep);
FileList *index_file_list(IndexReader *imap, unsigned char *flist_ptr);
void delete_index_reader(IndexReader *reader);
RegionList *index_lookup(IndexReader *map,const char *term);
RegionList *index_lookup_sorting(IndexReader *map, const char *term, struct LookupStruct *ls, int *return_hits);
void read_unsorted_postings(const char *entry, const unsigned char *regions, struct LookupStruct *ls);
int do_recursive_lookup(struct LookupStruct *ls, int s,int e, const char *pstr, struct SgrepStruct *this_sgrep);
void read_and_sort_postings(const char *entry, const unsigned char *regions, struct LookupStruct *ls);
Region *merge_regions(SgrepData *this_sgrep, const int length1, const Region *array1, const int length2, const Region *array2, int *return_length);
IndexBuffer *new_map_buffer(SgrepData *this_sgrep, const char *entry, const unsigned char *buf);
void delete_map_buffer(SgrepData *this_sgrep,IndexBuffer *map_buffer);
int get_region_index(IndexBuffer *buf, Region *region);
int read_all_postings(IndexFileReadPtr the_read,process_book_ptr this_book_process, SgrepData *this_sgrep);
unsigned int get_entry(IndexBuffer *buf);
int get_integer(IndexBuffer *buf);
unsigned char get_byte(IndexBuffer *buf);
int add_region_to_index(IndexWriter *writer, const char *str, int start, int end);
IndexBuffer *find_index_buffer(IndexWriter *writer, const char *str);
void add_entry(IndexWriter *writer,IndexBuffer *buf, int index, struct SgrepStruct *this_sgrep);
unsigned int hash_function(int size,const char *str);
void add_integer(IndexWriter *writer, IndexBuffer *buf, int num, struct SgrepStruct *this_sgrep);
void add_byte(IndexWriter *writer,IndexBuffer *buf, unsigned char byte, struct SgrepStruct *this_sgrep);
void new_block(IndexWriter *writer,IndexBuffer *buf, unsigned char byte);
void index_spool_overflow(IndexWriter *writer, struct SgrepStruct *this_sgrep);
int index_buffer_compare(const void *first, const void *next);
