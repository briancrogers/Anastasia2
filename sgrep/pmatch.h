/* Scanbuffer is for reading files */
struct ScanBuffer {
    SgrepData *sgrep;
    FileList *file_list;
    int len;
    int file_num;
    int old_file_num;
    int last_file;
    int region_start;
    const unsigned char *map;
    int map_size;
};

/* OutputList, ACState and ACScanner are for Aho-Corasick automate */
struct OutputList {
	struct PHRASE_NODE *phrase;
	struct OutputList *next;
};

struct ACState {
    struct ACState *gotos[256];
    struct ACState *fail;
    struct ACState *next; /* queue needed when creating fail function */
    struct OutputList *output_list;
#ifdef DEBUG
    int state_num;
#endif
};


int search(SgrepData *this_sgrep,struct PHRASE_NODE *phrase_list, FileList *files, int f_file, int l_file);
struct ScanBuffer *new_scan_buffer(SgrepData *this_sgrep,FileList *files);
struct ScanBuffer *reset_scan_buffer(struct ScanBuffer *sc, int f_file, int l_file);
void delete_scan_buffer(struct ScanBuffer *b);
struct ACScanner *init_AC_search(SgrepData *this_sgrep, struct PHRASE_NODE *phrase_list);
void delete_AC_state(SgrepData *this_sgrep,struct ACState *as);
void delete_AC_scanner(struct ACScanner *ac);
int next_scan_buffer(struct ScanBuffer *sb);
void ACsearch(struct ACScanner *scanner, const unsigned char *buf, int len, int start);
struct ACState *new_state(SgrepData *this_sgrep);
void create_goto(SgrepData *this_sgrep, struct PHRASE_NODE *phrase_list, struct ACState *root_state, int ignore_case);
void create_fail(SgrepData *this_sgrep,struct ACState *root_state);
void enter(SgrepData *this_sgrep, struct PHRASE_NODE *pn, struct ACState *root_state, int ignore_case);
void new_output(SgrepData *this_sgrep, struct ACState *s, struct PHRASE_NODE *pn);
