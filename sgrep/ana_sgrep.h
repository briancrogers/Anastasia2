// SGREP Header for external function calls

int set_up_sgrep_index(process_book_ptr this_book_ptr);
int do_count_sg(char *search_str, process_book_ptr this_book_ptr, pool *current_pool);
int do_find_sg_el(char *search_str, int start, int *num, int *nresults, process_book_ptr this_book, pool *current_pool);
int do_find_sg_text(char *search_str, int start, int *num, int *nresults, process_book_ptr this_book_ptr, pool *current_pool);
