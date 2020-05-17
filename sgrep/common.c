#include "sgrep.h"
#include "common.h"
#include "optimize.h"
#include <apr_strings.h>
#include <ctype.h>

ParseTreeNode *parse_string(SgrepData *this_sgrep,const char *str, struct PHRASE_NODE **phrase_list);


int sgrep_error(SgrepData *this_sgrep,char *format, ...) {
	char tmpstr[2048];
	int l;
	va_list ap;

	if (!this_sgrep) return 0;
	va_start(ap,format);
#if HAVE_VSNPRINTF
	l=vsnprintf(tmpstr,sizeof(tmpstr),format,ap);
#else
	l=vsprintf(tmpstr,format,ap);
#endif
	va_end(ap);
	write_log(tmpstr);
	return 1;
}


int flist_files(const FileList *list) {
	if (list==NULL) {
		return 0;
	} else {
	    return list->num_files;
	}
}

/* 
 * Create's and initializes new gc list.
 * Returns pointer to new list
 */
RegionList *new_region_list(SgrepData *this_sgrep) {
	RegionList*l;
	l=(struct RegionListStruct *) ana_malloc(&(this_sgrep->this_book->anastasia_memory), sizeof(RegionList), __FILE__, __LINE__);  
	if (l==NULL) {
		return NULL;
	}
	l->sgrep=this_sgrep;
	init_region_list(l);
	if (l==NULL) {
		return NULL;
	} else {
		stats.region_lists++; 
		stats.region_lists_now++;
		return l;
	}
}

/*
 * Delete region list
 */
void delete_region_list(RegionList *l) {
	SGREPDATA(l);
	stats.region_lists--;
	stats.region_lists_now--;
	delete_list_node(this_sgrep, l->first);
	ana_free(&(this_sgrep->this_book->anastasia_memory), l);
}

/*
 * Turns a gc list to a optimized chars list.
 * in chars list we only tell the length of every region (c->chars)
 * chars list 'contains' every possible region of that size
 * (0,0) (1,1) (2,2) or (1,2) (2,3) (3,4)
 */
void to_chars(RegionList *c,int chars, int end) {
	assert(c->length==0 && c->last==c->first);
	c->chars=chars-1;
	if (c->first!=NULL) {
		ana_free(&(c->sgrep->this_book->anastasia_memory), c->first);
		c->first=NULL;
		c->last=NULL;
	}
	if (end==0) end=c->length+chars-2;
	c->length=end-chars+2;
	if (c->length<=0) {
		/* The gc list became empty, we reinit it to empty list */
		init_region_list(c);
	}
}

void string_cat(SgrepString *s, const char *str) {
	int l;
	SGREPDATA(s);
	l=strlen(str);
	if (s->length+l+1>=s->size) {
		s->size=s->length+l+1;
		//Hmm need to reallocte some memory
		s->s=(char *) ana_realloc(&(this_sgrep->this_book->anastasia_memory), (void **) &(s->s), s->length, s->size, __FILE__, __LINE__);	
	}
	memcpy(s->s+s->length,str,l);
	s->length+=l;
	s->s[s->length]=0;
}

/*
 * Functions for handling non null terminated strings 
 * non null terminating strings won't work in version 2.0
 * (maybe never)
 */
SgrepString *new_string(SgrepData *this_sgrep,size_t size) {
	SgrepString *s;
	s=(struct SgrepStringStruct *) ana_malloc(&(this_sgrep->this_book->anastasia_memory), sizeof(SgrepString), __FILE__, __LINE__);
	if (s==NULL) {
		return NULL;
	}
	s->sgrep=this_sgrep;
	s->s=(char *)ana_malloc(&(this_sgrep->this_book->anastasia_memory), size+1, __FILE__, __LINE__);   
	if (s->s==NULL) {
		return NULL;
	}
	s->size=size;
	s->length=0;
	s->s[0]=0;
	s->escaped=NULL;
	return s;
}

void delete_string(SgrepString *s) {
	ana_free(&(s->sgrep->this_book->anastasia_memory), s->s);
	if (s->escaped) {
		delete_string(s->escaped);
	}
	ana_free(&(s->sgrep->this_book->anastasia_memory), s);
}

/*
 * Moved to here from main.c: FIXME: maybe own module for these.
 */

ParseTreeNode *parse_and_optimize(SgrepData *this_sgrep,const char *query, struct PHRASE_NODE **phrases) {
	ParseTreeNode *root;
	/*
	 * Optimize the operator tree 
	 */
	root=parse_string(this_sgrep,query,phrases);
	if (root==NULL) {
		/* Parse error (probably) */
		return NULL;
	}
	optimize_tree(this_sgrep,&root,phrases);
	if (this_sgrep->do_concat) {
		/* If we do concat on result list, we add a concat operation to
		 * parse tree */
		ParseTreeNode *concat=(struct ParseTreeNodeStruct *) ana_malloc(&(this_sgrep->this_book->anastasia_memory), sizeof(ParseTreeNode), __FILE__, __LINE__);
		concat->oper=CONCAT;
		concat->left=root;
		concat->right=NULL;
		concat->leaf=NULL;
		concat->parent=NULL;
		concat->refcount=1;
		concat->result=NULL;
		root=concat;
	};
	return root;
}

void delete_flist(FileList *list) {
	int i;
	for(i=0;i<flist_files(list);i++) {
		if (list->files[i].name!=NULL) {
			ana_free(&(list->sgrep->this_book->anastasia_memory), list->files[i].name);
			list->files[i].name=NULL;
		}
	}
	if (list!=NULL) {
		ana_free(&(list->sgrep->this_book->anastasia_memory), list->files);
		list->files=NULL;
		ana_free(&(list->sgrep->this_book->anastasia_memory), list);
	}
}

/*
 * Starts a search for regions in a gc list.
 * Inits ListIterator searching handle and returns it
 */
void start_region_search(RegionList *l, ListIterator *handle) {
	SGREPDATA(l);
	assert(l->last==NULL || l->last->next==NULL);
	assert(l->last!=NULL && l->length<=LIST_NODE_SIZE);
	assert(l->length>=0);
	l->complete=1;
	if (l->sorted!=START_SORTED) {
		get_start_sorted_list(l);
	}
	handle->list=l;
	handle->ind=0;
	handle->node=l->first;
	stats.scans++;
}

/*
 * FileList handling routines 
 */
FileList *new_flist(SgrepData *this_sgrep) {
	FileList *ifs;
	ifs= (struct FileListStruct *) ana_malloc(&(this_sgrep->this_book->anastasia_memory), sizeof(FileList), __FILE__, __LINE__);
	ifs->progress_limit=100;
	ifs->sgrep=this_sgrep;
	ifs->allocated=256; /* arbitrary */
	ifs->files=(OneFile *) ana_malloc(&(this_sgrep->this_book->anastasia_memory), sizeof(OneFile) * ifs->allocated, __FILE__, __LINE__);
	ifs->num_files=0;
	ifs->total_size=0;
	ifs->last_errno=0;
	return ifs;
}

/*
 * Adds a file to filelist *
 */
void flist_add_known(FileList *ifs, const char *name, int length) {
	SGREPDATA(ifs);
	if (ifs->num_files>=ifs->allocated) {
		ifs->files=(OneFile *) ana_realloc(&(this_sgrep->this_book->anastasia_memory), (void **) &(ifs->files), sizeof(OneFile)*ifs->allocated, sizeof(OneFile)*ifs->allocated*2, __FILE__, __LINE__);
		ifs->allocated*=2;
	}
	ifs->files[ifs->num_files].start=ifs->total_size;
	ifs->files[ifs->num_files].length=length;
	ifs->files[ifs->num_files].name=(name) ? (char *) apr_pstrdup(this_sgrep->current_pool, name):NULL;
	ifs->total_size+=length;
	ifs->num_files++;
}

void flist_ready(FileList *ifs) {
	SGREPDATA(ifs);
	if (ifs->num_files==0) {
		ifs->allocated=1;
	} else {
		ifs->allocated=ifs->num_files;
	}
	ifs->files=(OneFile *) ana_realloc(&(this_sgrep->this_book->anastasia_memory), (void **) &(ifs->files),(ifs->num_files-1)*sizeof(OneFile), ifs->allocated*sizeof(OneFile), __FILE__, __LINE__);
	ifs->progress_limit=0;
}

/*
 * initializes a gc list 
 */
void init_region_list(RegionList *l) {
	l->first=new_list_node(l->sgrep);
	l->last=l->first;
	l->last->next=NULL;
	l->last->prev=NULL;
	l->length=0;
	l->nodes=1;
	l->chars=0;
	l->complete=0;
	l->end_sorted=NULL;
	l->nested=0;
	l->sorted=START_SORTED;
	l->start_sorted_array=NULL;
}

/*
 * Creates a copy of GC_LIST s which is sorted by it's start points.
 * - if the given gc_list is sorted by end points, it is saved.
 * - otherwise newly created list will be replace old list.
 * - If list already had start sorted version it is returned.
 * - if list length is 0 or 1 same list is returned
 */
ListNode *get_start_sorted_list(RegionList *s) {
	int size;
	ListNode **inds;
	SGREPDATA(s);

	assert(s);
	s->complete=1;
	if (s->sorted==START_SORTED) {
		return s->first;
	}
	size=LIST_SIZE(s);
	if (size<2) {
		/* Only one or zero regions */	
		s->sorted=START_SORTED;
		return s->first;
	}
	/* Create new copy, only if we need to save end sorted version */
	if (s->sorted==END_SORTED) {
		assert(s->first==s->end_sorted);
		s->first=copy_list_nodes(this_sgrep,s->first,NULL);
	}
	s->sorted=START_SORTED;
	/* Sort the copy */
	inds=create_node_array(s,s->first); 
	gc_qsort(inds,0,size-1,SORT_BY_START);
	ana_free(&(this_sgrep->this_book->anastasia_memory), inds);
	stats.sorts_by_start++;
	return s->first;
}

/*
 * Allocates a new GC_NODE
 */
ListNode *new_list_node(SgrepData *this_sgrep) {
	ListNode *n;
	stats.gc_nodes++;
	stats.gc_nodes_allocated++;
	n= (struct ListNodeStruct *) ana_malloc(&(this_sgrep->this_book->anastasia_memory), sizeof(ListNode), __FILE__, __LINE__);
	if (n==NULL) {
		return NULL;
	}
	n->prev=NULL;
	n->next=NULL;
	return n;
}


void delete_list_node(SgrepData *this_sgrep, ListNode *this_node) {
	ListNode *temp_node, *delete_node;
	temp_node = this_node;
	while (temp_node!=NULL) {
		stats.gc_nodes--;
		stats.gc_nodes_allocated--;
		delete_node = temp_node;
		temp_node = temp_node->next;
		delete_node->next = NULL;
		delete_node->prev = NULL;
		ana_free(&(this_sgrep->this_book->anastasia_memory), delete_node);
	}
}

/*
 * Copies a list of ListNodes. Returns a pointer to first a node.
 * if last is not NULL, returns also a pointer to last node
 */
ListNode *copy_list_nodes(SgrepData *this_sgrep,const ListNode *n, ListNode **return_last) {
	ListNode *first=NULL;
	ListNode *last=NULL;

	last=new_list_node(this_sgrep);
	memcpy(last,n,sizeof(ListNode));
	first=last;
	first->prev=NULL;
	n=n->next;
	while(n) {
		last->next=new_list_node(this_sgrep);
		memcpy(last->next,n,sizeof(ListNode));
		last->next->prev=last;
		last=last->next;
		n=n->next;
	}
	last->next=NULL;
	if (return_last) {
		*return_last=last;
	}
	return first;
}

/*
 * creates an index table to nodes of a gc list 
 * index table is needed for referencing regions by their number in gc list.
 */
ListNode **create_node_array(const RegionList *s, ListNode *n) {
	int i;
	ListNode **inds;
	SGREPDATA(s);

	inds=(ListNode **) ana_malloc(&(this_sgrep->this_book->anastasia_memory), sizeof(ListNode *) * s->nodes, __FILE__, __LINE__);
	inds[0]=n;
	for(i=1;i<s->nodes;i++)
		inds[i]=inds[i-1]->next;
	return inds;
}

/*
 * Recursive qsort for gc_list. Needs gc node index table created by
 * create_node_array 
 * A faster way to do this would be nice
 */
void gc_qsort(ListNode **inds,int s,int e, enum SortTypes st) {
	Region creg,sreg;
	int i,m,last;	
	int r;
	if (s>=e) return;
	m=(s+e)/2;
	creg=LIST_RNUM(inds,m);
	LIST_RNUM(inds,m)=LIST_RNUM(inds,s);
	LIST_RNUM(inds,s)=creg;
	last=s;
	for(i=s+1;i<=e;i++) {
		if (st==SORT_BY_START) {
			r=LIST_RNUM(inds,i).start < creg.start || 
				(LIST_RNUM(inds,i).start==creg.start && LIST_RNUM(inds,i).end<creg.end );
		} else {
			/* SORT_BY_END */
			r=LIST_RNUM(inds,i).end < creg.end || 
				(LIST_RNUM(inds,i).end==creg.end && LIST_RNUM(inds,i).start<creg.start );
		}	    
		if ( r ) {
			last++;
			sreg=LIST_RNUM(inds,i);
			LIST_RNUM(inds,i)=LIST_RNUM(inds,last);
			LIST_RNUM(inds,last)=sreg;
		}
	}
	sreg=LIST_RNUM(inds,s);
    LIST_RNUM(inds,s)=LIST_RNUM(inds,last);
	LIST_RNUM(inds,last)=sreg;
	gc_qsort(inds,s,last-1,st);
	gc_qsort(inds,last+1,e,st);	
}

int flist_start(const FileList *list, int n) {
	if (n<0 || n>=list->num_files) return SGREP_ERROR;
	return list->files[n].start;
}

int flist_total(const FileList *list) {
	return list->total_size;
}

/*
 * Inserts new ListNode to RegionList
 */
void insert_list_node(RegionList *l) {
	ListNode *new_node;
	assert(l->length==LIST_NODE_SIZE);
	new_node=new_list_node(l->sgrep);
	if (new_node==NULL) {
		return;
	}
	l->last->next=new_node;
	new_node->prev=l->last;
	l->last=new_node;
	l->length=0;
	l->nodes++;
}

void start_end_sorted_search(RegionList *l, ListIterator *handle) {
    SGREPDATA(l);
    assert(l->last==NULL || l->last->next==NULL);
    assert(l->last!=NULL && l->length<=LIST_NODE_SIZE);
    assert(l->length>=0);

    l->complete=1;
    if (l->sorted==START_SORTED && !l->nested) {
		start_region_search(l,handle);
		return;
    }
    handle->list=l;
    handle->ind=0;
    handle->node=get_end_sorted_list(l);
    stats.scans++;
}

void list_set_sorted(RegionList *l, enum RegionListSorted sorted) {
    assert(!l->complete);
    l->sorted=sorted;
}

enum RegionListSorted list_get_sorted(const RegionList *l) {
    return l->sorted;
}

void list_require_start_sorted_array(RegionList *l, struct SgrepStruct *this_sgrep) {
    l->complete=1;
    /* FIXME: */ assert(!l->chars);
    if (l->start_sorted_array) return;
    if (l->sorted!=START_SORTED) {
		get_start_sorted_list(l);
    }
    assert(l->sorted==START_SORTED && l->first);
    l->start_sorted_array=create_node_array(l,l->first);
}

/*
 * Removes duplica regions from a gc list 
 */
void remove_duplicates(RegionList *s) {
	ListIterator r,s_handle;
	ListNode *t;
	Region p1,p2;
	SGREPDATA(s);
	/* We know only how to remove remove_duplicates from start sorted
	 * lists */
	assert(s);
	start_region_search(s,&r);
	assert(s->sorted==START_SORTED);
	stats.remove_duplicates++;

	start_region_search(s,&s_handle);	
	get_region(&s_handle,&p1);
	while ( p1.start!=-1 ) {
		get_region(&s_handle,&p2);
		if ( p1.start!=p2.start || p1.end!=p2.end ) {
		/* Regions p1 and p2 are different */
			if ( r.ind==LIST_NODE_SIZE ) {
				r.node=r.node->next;
				assert(r.node!=NULL);
				r.ind=0;
			}
			r.node->list[r.ind++]=p1;
			p1=p2;
		}
	}
	s->length=r.ind;
	s->last=r.node;
/* free gc blocks which are not needed any more */
	r.node=r.node->next;
	while (r.node!=NULL) {
		t=r.node;
		r.node=r.node->next;
		ana_free(&(this_sgrep->this_book->anastasia_memory), t);
	}
	s->last->next=NULL;
}

/*
 * Starts a search for regions from given index (starting from 0) in
 * region list.
 * Inits ListIterator searching handle and returns it
 */
void start_region_search_from(RegionList *l, int index, ListIterator *handle) {
    SGREPDATA(l);
    assert(l->last==NULL || l->last->next==NULL);
    assert(l->last!=NULL && l->length<=LIST_NODE_SIZE);
    assert(l->length>=0);

    l->complete=1;
    if (l->sorted!=START_SORTED) {
		get_start_sorted_list(l);
    }
    handle->list=l;
    handle->ind=0;
    handle->node=l->first;
    while(index>=LIST_NODE_SIZE && handle->node->next) {
		handle->node=handle->node->next;
		index-=LIST_NODE_SIZE;
    }
    handle->ind= (index < l->length) ? index : l->length;
    stats.scans++;
}

ListNode *get_end_sorted_list(RegionList *s) {
    int size;
    ListNode **inds;
    SGREPDATA(s);
    assert(s);
    s->complete=1;
    if (s->sorted==END_SORTED) {
		return s->first;
    }
    if (s->sorted==START_SORTED && (!s->nested)) {
		/* Start sorted non nested list is also end sorted */
		return s->first;
    }    
    size=LIST_SIZE(s);
    if (size<2) {
		/* Only one or zero regions */	
		return s->first;
    }
    if (s->end_sorted) {
		/* Already had end sorted version */
		return s->end_sorted;
    }
    /* Create new copy, only if we need to save start sorted version */
    if (s->sorted==NOT_SORTED) {
		s->sorted=END_SORTED;
		s->end_sorted=s->first;
    } else {
		s->end_sorted=copy_list_nodes(this_sgrep,s->first,NULL);
    }
    /* Sort the copy */
    inds=create_node_array(s,s->end_sorted); 
    gc_qsort(inds,0,size-1,SORT_BY_END);
    ana_free(&(this_sgrep->this_book->anastasia_memory), inds);
    
    stats.sorts_by_end++;
    return s->end_sorted;
}

void string_cat_escaped(SgrepString *escaped, const char *str) {
    int i;
    int c;

    for(i=0;str[i];i++) {
	c=(unsigned char)str[i];
	switch(c) {
	case '\n':
	    string_cat(escaped,"\\n");
	    break;
	case '\r':
	    string_cat(escaped,"\\r");
	    break;
	case '\b':
	    string_cat(escaped,"\\b");
	    break;
	case '"':
	    string_cat(escaped,"\\\"");
	    break;
	case 255: {
	    /* Encoded character */
	    int ch=0;
	    char buf[40];
	    int x=0;
	    i++;
	    c=(unsigned char)str[i];
	    while(c && c!=32) {
		ch|=(c-33) << x;
		x+=6;
		i++;
		c=(unsigned char)str[i];
	    }
	    if (c==0) {
		sgrep_error(escaped->sgrep,
			    "Could not decode internal encoded character!\n");
	    } else {
		sprintf(buf,"\\#x%x;",ch);
		string_cat(escaped,buf);
	    }
	    break;
	}
	default:
	    if (c<32) {
		char buf[30];
		sprintf(buf,"\\#x%x;",c);
		string_cat(escaped,buf);
	    } else {
		string_push(escaped,c);
	    }
	    break;
	}
    }
}

void real_string_push(SgrepString *s, SgrepChar ch) {
    SGREPDATA(s);

    if (s->length+1>=s->size) {	
		s->size=(s->size<16) ? 32 : s->size+s->size/2;	
		s->s=(char *) ana_realloc(&(this_sgrep->this_book->anastasia_memory), (void **) &(s->s), s->length, s->size, __FILE__, __LINE__);	
    }
    if (ch>254) {
	((unsigned char *)s->s)[s->length++]=255;
	while(ch>0) {
	    string_push(s,(ch%64)+33);
	    ch=ch/64;
	}
	string_push(s,32);
    } else {
	s->s[s->length++]=ch;
    }
}

/*
 * Create new SgrepString expanding all backslash escapes
 */
SgrepString *expand_backslashes(SgrepData *this_sgrep,const char *s) {
    int i=0;
    SgrepString *r;
    const unsigned char *str=(const unsigned char *)s;

    r=new_string(this_sgrep,strlen(s));
    while(str[i]) {
	if (str[i]=='\\') {
	    int ch;
	    i++;
	    ch=expand_backslash_escape(this_sgrep,str,&i);
	    if (ch>=0) string_push(r,ch);
	} else {
	    string_push(r,str[i]);
	    i++;
	}
    }
    return r;
}

SgrepString *init_string(SgrepData *this_sgrep, size_t size, const char *src) {
	SgrepString *s;
	
	s=new_string(this_sgrep,size);
	memcpy(s->s,src,size);
	s->s[size]=0;
	s->length=size;
	return s;
}

void push_front(SgrepString *s,const char *str) {
    char *tmp;
    int l;
    SGREPDATA(s);

    l=strlen(str);
    tmp=(char *)ana_malloc(&(this_sgrep->this_book->anastasia_memory), s->length+l+1, __FILE__, __LINE__);
    memcpy(tmp,str,l);
    memcpy(tmp+l,s->s,s->length);
    ana_free(&(this_sgrep->this_book->anastasia_memory), s->s);
    s->s=tmp;
    s->length+=l;
    s->s[s->length]=0;
    s->size=s->length+1;
}

/*
 * Expand one backslash escape
 */
int expand_backslash_escape(SgrepData *this_sgrep, const unsigned char *list, int *i) {
    int reference=-1;
    int ch;

    if (list[*i]==0) {
	sgrep_error(this_sgrep,"Backslash at end of string\n");
	return -1;
    }

    ch=list[*i];
    (*i)++;
    switch(ch) {
    case 't': return '\t';
    case 'n': return '\n';
    case 'r': return '\r';
    case 'f': return '\f';
    case 'b': return '\b'; 
    case '\\': return '\\';
    case '\"': return ch='\"';
    case '\n': return '\n';       
    case '#': break;
    default:
	if (isprint(ch)) {
	    sgrep_error(this_sgrep,"Unknown backslash escape '%c'\n",ch);
	} else {
	    sgrep_error(this_sgrep,"Unknown blackslash escape #%d\n",ch);
	}
	return -1;
    }

    if (list[*i]==0) {
	sgrep_error(this_sgrep,"Character reference at end of string\n");
	return -1;
    }
    
    if (list[*i]=='x') {
	/* Hexadecimal character reference */
	reference=0;
	(*i)++;
	while( (list[*i]>='0' && list[*i]<='9') ||
	       ( toupper(list[*i])>='A' && toupper(list[*i])<='F')) {
	    if (list[*i]>='0' && list[*i]<='9') {
		reference=reference*16+list[*i]-'0';
	    } else {
		reference=reference*16+toupper(list[*i])-'A'+10;
	    }
	    (*i)++;
	}
	/* Eat the ';' if there is one */
	if (list[*i]==';') (*i)++;
    } else if (list[*i]>='0' && list[*i]<='9') {
	/* Decimal character reference */
	reference=list[*i]-'0';
	(*i)++;
	while (list[*i]>='0' && list[*i]<='9') {
	    reference=reference*10+(list[*i]-'0');
	    (*i)++;
	}
	/* Eat the ';' if there is one */
	if (list[*i]==';') (*i)++;
    } else if (list[*i]<32) {
	sgrep_error(this_sgrep,"Invalid character #%d in character list character reference\n",
		    list[*i]);
	return -1;
    } else {
	sgrep_error(this_sgrep,"Invalid character '%c' in character list character reference\n",
		    list[*i]);
	return -1;
    }

    if (reference>=0xfffe || reference==0) {
	sgrep_error(this_sgrep,"Character #%d in character list is not an unicode character\n",
		    reference);
	reference=-1;
    }

    return reference;
}

void string_toupper(SgrepString *s, int from) {
    unsigned int i=from;
    while(i<s->length) {
	if ((unsigned char)s->s[i]==255) {
	    /* Just ignore chars >255 */
	    i++;
	    while(i<s->length && s->s[i]!=32) i++;
	} else {
	    s->s[i]=toupper(s->s[i]);
	}
	i++;
    }
}

void string_tolower(SgrepString *s, int from) {
    unsigned int i=from;
    while(i<s->length) {
	if ((unsigned char)s->s[i]==255) {
	    /* Just ignore chars >255 */
	    i++;
	    while(i<s->length && s->s[i]!=32) i++;
	} else {
	    s->s[i]=tolower(s->s[i]);
	}
	i++;
    }
}

const char *string_escaped(SgrepString *str) {    
    if (!str->escaped) {
	str->escaped=new_string(str->sgrep,str->length+8);
    } else {
	string_clear(str->escaped);
    }
    string_cat_escaped(str->escaped,string_to_char(str));
    return string_to_char(str->escaped);
}

int flist_exists(FileList *list, const char *name) {
    int i;
    for(i=0;i<flist_files(list);i++) {
	if (strcmp(name,flist_name(list,i))==0) return 1;
    }
    return 0;
}

const char *flist_name(const FileList *list, int n) {
    if (n<0 || n>=list->num_files) return NULL;
    return list->files[n].name;
}

int flist_length(const FileList *list, int n) {
    if (n<0 || n>=list->num_files) return SGREP_ERROR;
    return list->files[n].length;
}

