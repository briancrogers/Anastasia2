#include "sgrep.h"
#include "pmatch.h"
#include "common.h"
#include <ctype.h>

struct ACScanner {
    SgrepData *sgrep;
    struct PHRASE_NODE *phrase_list; /* Points to this scanners phrase list */
    struct ACState *root_state; /* Points to root state */
    struct ACState *s;          /* Points to current state */
    int ignore_case;
} AC_scanner; /* THE AC_scanner, since only one is needed */

SGMLScanner *new_sgml_phrase_scanner(SgrepData *this_sgrep, FileList *file_list, struct PHRASE_NODE *list);
void sgml_flush(SGMLScanner *sgmls);
int sgml_scan(SGMLScanner *scanner, const unsigned char *buf, int len, int start,int file_num);
void delete_sgml_scanner(SGMLScanner *s);

int search(SgrepData *this_sgrep,struct PHRASE_NODE *phrase_list, FileList *files, int f_file, int l_file) {
	int sgml_phrases;
	int regex_phrases;
	int ac_phrases;
	int file_phrases;
	int e=SGREP_OK;
	int previous_file=-1;

	/* If there is no phrases, there is no point to do searching */
	if (phrase_list==NULL) {
		return SGREP_OK;
	}
    if (this_sgrep->index_file==NULL) {
	struct ScanBuffer *sb=NULL;
	struct ACScanner *acs=NULL;
	SGMLScanner *sgmls=NULL;
	struct PHRASE_NODE *j=NULL;

	file_phrases=ac_phrases=sgml_phrases=regex_phrases=0;

	/* We have to create empty gc lists for phrases */
	for (j=phrase_list;j!=NULL;j=j->next) {
		assert(j->regions==NULL);
		j->regions=new_region_list(this_sgrep);
		if (j->phrase->s[0]=='@' || j->phrase->s[0]=='*') {
			list_set_sorted(j->regions,NOT_SORTED);
			j->regions->nested=1;
		}
		switch (j->phrase->s[0]) {
			case 'n':
				ac_phrases++;
				break;
			case 'r':
				regex_phrases++;
				break;
			case 'f':
				file_phrases++;
				break;
			case '#':
				/* Input independent phrases */
				break;
			default:
				sgml_phrases++;
		}	
	}
	/* Initialization */
	sb=new_scan_buffer(this_sgrep,files);
	reset_scan_buffer(sb,f_file,l_file);
	if (ac_phrases) {
		acs=init_AC_search(this_sgrep,phrase_list);
	}
	if (sgml_phrases) {
		sgmls=new_sgml_phrase_scanner(this_sgrep,files,phrase_list);
	}
	/* Main scanning loop, only if there is something to scan */
	if (acs || sgmls) while((e=next_scan_buffer(sb))>0) {
		if (ac_phrases) {
			ACsearch(acs,sb->map,sb->len,sb->region_start);
		}
		if (sgml_phrases) {
			if (previous_file!=-1 && sb->file_num!=previous_file) {
				sgml_flush(sgmls);
			}
			previous_file=sb->file_num;
			sgml_scan(sgmls,sb->map,sb->len,sb->region_start,sb->file_num);
		}
	}
	/* Clean up scanners */
	delete_scan_buffer(sb);
	if (sgmls) {
		sgml_flush(sgmls);
		delete_sgml_scanner(sgmls);
	}	
	if (acs) delete_AC_scanner(acs);
	/* Now handle the phrases, whose contents we know only after
	 * scanning or which are independent of scanning */
	/* FIXME and include-entities will break this */
	for(j=phrase_list;j!=NULL; j=j->next) {
		switch(j->phrase->s[0]) {
			case '#':
				if (strcmp(j->phrase->s,"#start")==0) {
					int start=flist_start(files,f_file);
					add_region(j->regions,start,start);
				} else if (strcmp(j->phrase->s,"#end")==0) {
					int last=flist_start(files,l_file)+
					flist_length(files,l_file)-1;
					add_region(j->regions,last,last);
				} else {
					sgrep_error(this_sgrep,"Don't know how to handle phrase %s\n", j->phrase->s);
				}
				break;
			case 'f': {
				int f;	       
				for(f=f_file;f<=l_file;f++) {
					/* Check for filename */
					if (j->phrase->s[j->phrase->length-1]=='*') {
						/* Wildcard */
						if (strncmp((char *)j->phrase->s+1,flist_name(files,f), j->phrase->length-2)==0 && flist_length(files,f)>0) {
							add_region(j->regions, flist_start(files,f), flist_start(files,f)+flist_length(files,f)-1);
						}
					} else if (strcmp((char *)j->phrase->s+1, flist_name(files,f))==0 && flist_length(files,f)>0) {
						add_region(j->regions, flist_start(files,f), flist_start(files,f)+flist_length(files,f)-1);
					}
				}
		    }
		    break;
	    }
	}
    } else {
		e=SGREP_OK;
    }
    return (e==SGREP_ERROR) ? SGREP_ERROR:SGREP_OK;
}

/*
 * Creates and initializes a new scanner buffer struct with given
 * input files
 */
struct ScanBuffer *new_scan_buffer(SgrepData *this_sgrep,FileList *files) {
    struct ScanBuffer *sc;
    sc=(struct ScanBuffer *) ana_malloc(&(this_sgrep->this_book->anastasia_memory), sizeof(struct ScanBuffer), __FILE__, __LINE__);
    sc->sgrep=this_sgrep;
    sc->file_list=files;
    sc->len=0;
    sc->file_num=0;
    sc->old_file_num=-1;
    sc->last_file=-1; /* As many files there is in list */
    sc->region_start=0;
    sc->map=NULL;
    sc->map_size=0;
    return sc;
}

struct ScanBuffer *reset_scan_buffer(struct ScanBuffer *sc, int f_file, int l_file) {
    sc->file_num=f_file;
    sc->last_file=l_file;
    sc->region_start=flist_start(sc->file_list,f_file);
    return sc;
}


void delete_scan_buffer(struct ScanBuffer *b) {
    ana_free(&(b->sgrep->this_book->anastasia_memory), b);
}

/* Phrases list points to list of phrases to be
 * matched. ifs points to names of input files, and lf is the number of
 * input files.
 */
struct ACScanner *init_AC_search(SgrepData *this_sgrep, struct PHRASE_NODE *phrase_list) {
    int i;
    struct ACScanner *sc;
    sc= (struct ACScanner *) ana_malloc(&(this_sgrep->this_book->anastasia_memory), sizeof(struct ACScanner), __FILE__, __LINE__);
    sc->sgrep=this_sgrep;
    sc->root_state=new_state(this_sgrep);
    sc->phrase_list=phrase_list;
    sc->s=sc->root_state;
    sc->ignore_case=this_sgrep->ignore_case;
    create_goto(this_sgrep,phrase_list,sc->root_state,sc->ignore_case);
    /* there isn't any fail links from root state */
    for (i=0;i<256;i++) {
	if (sc->root_state->gotos[i]==NULL) 
	    sc->root_state->gotos[i]=sc->root_state;
    }
    create_fail(this_sgrep,sc->root_state);
    return sc;
}

void delete_AC_state(SgrepData *this_sgrep,struct ACState *as) {
    int i;
    for(i=0;i<256;i++) {
	if (as->gotos[i] && as->gotos[i]!=as) {
	    delete_AC_state(this_sgrep,as->gotos[i]);
	}
	while(as->output_list) {
	    struct OutputList *ol=as->output_list;
	    as->output_list=as->output_list->next;
	    ana_free(&(this_sgrep->this_book->anastasia_memory), ol);
	}
    }
    ana_free(&(this_sgrep->this_book->anastasia_memory), as);
}
	
void delete_AC_scanner(struct ACScanner *ac) {
    /* FIXME: this leaks memory! */
    SGREPDATA(ac);
    delete_AC_state(this_sgrep,ac->root_state);
    ana_free(&(this_sgrep->this_book->anastasia_memory), ac);
}

/*
 * Fills the scanner input buffer and sets len and file_num respectively
 * if all files have been processed returns 0
 * otherwise returns number of bytes available in buffer
 * NEW: uses map_file() instead of read()
 */
int next_scan_buffer(struct ScanBuffer *sb) {
    SGREPDATA(sb);
    if (sb->map && sb->len==sb->map_size) {
		sb->file_num++;
    }
    /* Skip zero length files */
    while(sb->file_num<flist_files(sb->file_list) && flist_length(sb->file_list,sb->file_num)==0) {
		sb->file_num++;
    }
    if (sb->old_file_num!=sb->file_num && sb->map) {
		unmap_file(this_sgrep,(void *)sb->map,sb->map_size);
		sb->map=NULL;
		sb->map_size=0;
    }
    if ( (sb->last_file==-1 && sb->file_num>=flist_files(sb->file_list)) || (sb->last_file>=0 && sb->file_num>sb->last_file) ) {
		/* All files scanned */
		return 0;
    }
    if (!sb->map) {
		void *map;
		/* 12/01: here we are going to bypass map_file function in favour of putting a FILE * in map */
		/* actually we are just going to stick it in the same structure... */
		sb->map_size=map_file(this_sgrep,flist_name(sb->file_list,sb->file_num), &map);
		sb->map=(const unsigned char*)map;
    }
    if (sb->map==NULL) {
		sgrep_error(this_sgrep,"Failed to scan file '%s'\n", flist_name(sb->file_list,sb->file_num));
		return SGREP_ERROR;
    }
    sb->old_file_num=sb->file_num;
    if (sb->map_size!=flist_length(sb->file_list,sb->file_num)) {
		sgrep_error(this_sgrep,"Size of file '%s' has changed\n", flist_name(sb->file_list,sb->file_num));
    }
    sb->region_start+=sb->len;
    sb->len=sb->map_size;
    return sb->len;
}

/* 
 * The AC automate search. 
 * (A dramatically simpler version, than which it used to be four years
 *  ago when it first saw the light of the day.
 *  It seems that i've actually gained some "programming experience"
 *  in these years :) 
 */
void ACsearch(struct ACScanner *scanner, const unsigned char *buf, int len, int start) {
    struct OutputList *op;
    int ch;
    int i;
    struct ACState *s;

    s=scanner->s;
    for(i=0;i<len;i++) {
	ch=(scanner->ignore_case) ? toupper(buf[i]) : buf[i];
	while (s->gotos[ch]==NULL) {
	    assert(s->fail);
	    s=s->fail;
	}	
	s=s->gotos[ch];
	op=s->output_list;
	while(op!=NULL) {
	    scanner->sgrep->statistics.phrases++;
	    assert(op->phrase->regions!=NULL);
	    add_region( op->phrase->regions, i-(op->phrase->phrase->length-1)+start+1, i+start);
	    op=op->next;
	} while ( op!=NULL );
    }
    scanner->s=s;
}

/*
 * Gives and inits a new state to the automate.
 */
/* If root_state is NULL
 * it inits the root_state 
 */
struct ACState *new_state(SgrepData *this_sgrep) {
	int i;
	struct ACState *s;
	s=(struct ACState *)ana_malloc(&(this_sgrep->this_book->anastasia_memory), sizeof(struct ACState), __FILE__, __LINE__);
	for (i=0;i<256;i++) s->gotos[i]=NULL;
	s->output_list=NULL;
	s->next=NULL;
	s->fail=NULL;
	return s;
}

/*
 * The creation of the AC goto function using the enter function 
 * and the phrase list. 
 * The automate is spesified with root_state
 */
void create_goto(SgrepData *this_sgrep, struct PHRASE_NODE *phrase_list, struct ACState *root_state, int ignore_case) {
	struct PHRASE_NODE *pn;

	for(pn=phrase_list;pn!=NULL;pn=pn->next) {
	    if (pn->phrase->s[0]=='n') {
		/* Add only AC phrases to automate */

		enter(this_sgrep,pn,root_state,ignore_case);
	    }
	}
}

/*
 * The creation of the AC fail function and the final output function 
 * The automate to use is given with root_state
 */
void create_fail(SgrepData *this_sgrep,struct ACState *root_state) {
	int i;
	struct ACState *s,*r,*state;
	struct ACState *first=NULL;
	struct ACState *last=NULL;	
	struct OutputList *op;
	for (i=0;i<256;i++)
	{
		if ( (s=root_state->gotos[i]) !=root_state )
		{		
			if (first==NULL) first=s;
			if (last!=NULL) last->next=s;
			last=s;
			last->next=NULL;
			s->fail=root_state;
		}
	}
	while (first!=NULL)
	{
		r=first;
		first=first->next;
		for (i=0;i<256;i++) if ( r->gotos[i]!=NULL )
		{
			s=r->gotos[i];
			last->next=s;
			last=s;
			last->next=NULL;
			if (first==NULL) first=last;
			state=r->fail;
			while (state->gotos[i]==NULL) state=state->fail;
			s->fail=state->gotos[i];
			for (op=s->fail->output_list;
			     op!=NULL;
			     op=op->next) {
			    assert(op->phrase!=NULL);
			    new_output(this_sgrep,s,op->phrase);
			}
		}
	}
}

/*
 * Enters a new phrase to automate given with root_state
 */
void enter(SgrepData *this_sgrep, struct PHRASE_NODE *pn, struct ACState *root_state, int ignore_case) {
	struct ACState *state=root_state;
	size_t j;
	unsigned char pch;
	assert(pn->phrase->s[0]=='n');
	j=1;
	pch=pn->phrase->s[j];
	if (ignore_case) pch=toupper(pch);
	while ( state->gotos[pch]!=NULL && j<pn->phrase->length )
	{
		state=state->gotos[pch];
		j++;
		pch=pn->phrase->s[j];
		if (ignore_case) pch=toupper(pch);
	}
	
	while( j<pn->phrase->length )
	{
		state->gotos[pch]=new_state(this_sgrep);
		state=state->gotos[pch];
		j++;
		pch=pn->phrase->s[j];
		if (ignore_case) pch=toupper(pch);
	}
	new_output(this_sgrep,state,pn);
}

/*
 * Enters a new output link to a state 
 */
void new_output(SgrepData *this_sgrep, struct ACState *s, struct PHRASE_NODE *pn) {
	struct OutputList **op;

	op=&s->output_list;
	while (*op!=NULL) op=& (*op)->next;
 	*op= (struct OutputList *) ana_malloc(&(this_sgrep->this_book->anastasia_memory), sizeof(struct OutputList), __FILE__, __LINE__);   
	(*op)->next=NULL;
	(*op)->phrase=pn;
}

