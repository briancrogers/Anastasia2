//#define MEM_TEST 1

#if !defined(MEM_TEST)
    #include "apache1/httpd.h"
    #include "apache1/http_config.h"
    #include "apache1/http_core.h"
    #include "apache1/http_log.h"
    #include "apache1/http_protocol.h"
    #include "apache1/http_main.h"
#endif

#define ANAHASHSIZE 60013
#define ANASIZEHASH 51

typedef apr_pool_t pool;
typedef apr_table_t table;

typedef struct ana_hash_item *ana_hash_item_ptr;

struct ana_hash_item {
	void *ptr;
	long size;
	int num;
	char file[200];
	int line;
        int used;
	ana_hash_item_ptr hprevious;
	ana_hash_item_ptr hnext;
        ana_hash_item_ptr lprevious;
        ana_hash_item_ptr lnext;
};

struct ana_memory {
	ana_hash_item_ptr used_list[ANAHASHSIZE];
	ana_hash_item_ptr size_list[ANASIZEHASH];
	ana_hash_item_ptr first_block;
	void *memory_start;
	long mem_free;
	long mem_used;
	long mem_allocated;
	long number_of_blocks;
	long number_of_blocks_used;
	int memory_error;
	void *mem_pool;
	void *this_book;
};

int init_ana_memory(struct ana_memory *my_mem, void *mem_pool, long size);
int free_ana_memory(struct ana_memory *my_mem);
void *ana_malloc(struct ana_memory *my_mem, long size, char *file, int line);
int ana_free(struct ana_memory *my_mem, void *this_mem);
void *ana_realloc(struct ana_memory *my_mem, void **this_mem, long oldsize, long size, char *file, int line);
void *ana_reclaim(struct ana_memory *my_mem, ana_hash_item_ptr input_hash_ptr);
int check_mem(struct ana_memory *my_mem);
int deep_check(struct ana_memory *my_mem);
