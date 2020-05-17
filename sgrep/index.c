#include "sgrep.h"
#include "index.h"
#include "common.h"
#include <apr_strings.h>


static int put_int(int i,FILE* stream);
int fwrite_postings(IndexWriter *writer, IndexBuffer *tmp, FILE *stream);
static IndexBuffer *new_writer_index_buffer(IndexWriter *writer);

/*based on existing new_index_reader: set up so we just stick in existing pointer to mapped file, for Mac */
/*not used when building an index */
/* revised for low memory use: entries etc go into memory; rest is read of disc */
/* this will allow consideralbe simplification of this */
IndexReader *pr_new_index_reader(SgrepData *this_sgrep) {
    IndexReader *imap;
    const unsigned char *ptr;
    imap= (struct IndexReaderStruct *) ana_malloc(&(this_sgrep->this_book->anastasia_memory), sizeof(IndexReader), __FILE__, __LINE__);
	if (imap==NULL) {
		goto error;
	}
    imap->sgrep=this_sgrep;
    imap->filename=this_sgrep->index_file;
    imap->size=this_sgrep->this_book->map_len;
    imap->map=this_sgrep->this_book->map_idx;
    if (imap->size==0) goto error;
    if (imap->size<=1024) {
		sgrep_error(this_sgrep,"Too short index file '%s'",this_sgrep->index_file);
		goto error;
    }
    ptr=(const unsigned char *)imap->map;
    if (strncmp((const char *)ptr,INDEX_VERSION_MAGIC, strlen(INDEX_VERSION_MAGIC))!=0) {
		sgrep_error(this_sgrep,"File '%s' is not an sgrep index.\n",this_sgrep->index_file);
		goto error;
    }
	ptr+=512;
	imap->len=get_int(ptr,0);
	imap->array=((const unsigned char*)imap->map)+get_int(ptr,1);
	imap->entries=((const char *)imap->map)+get_int(ptr,2);
    return imap;

error:
	ana_free(&(this_sgrep->this_book->anastasia_memory), imap);
	return NULL;
}

FileList *index_file_list(IndexReader *imap, unsigned char *flist_ptr) {
	SGREPDATA(imap);
	int files;
	int i;
	int l;
	int size;
	const char *name;
	FileList *file_list;
	file_list=new_flist(this_sgrep);
	files=get_int(flist_ptr,0);
	for(i=0;i<files;i++) {
		flist_ptr+=4;
		l=get_int(flist_ptr,0);
		flist_ptr+=4;
		name=(const char *)flist_ptr;
		flist_ptr+=l+1;
		size=get_int(flist_ptr,0);
		flist_add_known(file_list,name,size);
	}
	flist_ready(file_list);
	return file_list;
}

void delete_index_reader(IndexReader *reader) {
	ana_free(&(reader->sgrep->this_book->anastasia_memory), reader);
}

/*
 * This lookup version is faster with one term, but uses less memory
 * in every case
 */
RegionList *index_lookup(IndexReader *map,const char *term) {
	int hits;
	struct LookupStruct *ls;
	RegionList *l;
	SGREPDATA(map);

	/* Initialize LookupStruct */
	ls = (struct LookupStruct *) ana_malloc(&(this_sgrep->this_book->anastasia_memory), sizeof(struct LookupStruct), __FILE__, __LINE__);
        if (ls==NULL) {
            return NULL;
        }
	ls->sgrep=this_sgrep;
	ls->map=map;
	ls->stop_words=0;

	if (this_sgrep->progress_output) {
		SgrepString *s=new_string(this_sgrep,max_term_len);
		string_cat_escaped(s,term);
		delete_string(s);
	}
	if (term[strlen(term)-1]=='*') {
		char *tmp=NULL;
		tmp=apr_pstrdup(this_sgrep->current_pool, term);
		tmp[strlen(tmp)-1]=0;
		ls->begin=ls->end=tmp;
		l=index_lookup_sorting(map,term,ls,&hits);
		if (!l) return(NULL);
		/* Clean up */
		tmp = NULL;
		ls->begin=NULL;
		ls->end=NULL;
	} else {
		l=new_region_list(this_sgrep);
		if (term[0]=='@') {
			l->nested=1;
		} else {
			l->nested=0;
		}
		ls->data.reader=l;
		ls->begin=term;
		ls->end=NULL;
		ls->callback=read_unsorted_postings;
		/* Do the lookup */	
		hits=do_recursive_lookup(ls,0,map->len,"", this_sgrep);
	}
	/* Do the sorting if necessary */
	if (hits>1 && l->sorted!=START_SORTED) {
		remove_duplicates(l);
	} else {
		list_set_sorted(l,START_SORTED);
    }
	/* All done */
	return l;
}

RegionList *index_lookup_sorting(IndexReader *map, const char *term, struct LookupStruct *ls, int *return_hits) {    
    struct SortingReaderStruct *reader=&ls->data.sorting_reader;
    Region *current;
    int len;
    RegionList *result;
    int i;
    SGREPDATA(map);    

    /* Initialize our private part of LookupStruct */
    ls->callback=read_and_sort_postings;
    reader->max=0;
    reader->regions_merged=0;
    reader->lists_merged=0;
    reader->one.start=
	reader->one.end=INT_MAX;
    memset(&reader->sizes,0,sizeof(reader->sizes));
    memset(&reader->regions,0,sizeof(reader->regions));
    reader->saved_size=LIST_NODE_SIZE;
    reader->saved_array=(Region *) ana_malloc(&(this_sgrep->this_book->anastasia_memory), sizeof(Region)*reader->saved_size, __FILE__, __LINE__);
	/*could fail.  say so and abort*/
	if (!reader->saved_array) {
		write_log("Unable to allocate sufficient memory for internal searching procedure (\"index_lookup_sorting\": this will lead to unreliable search results.  This could be because you are carrying out a wild-card search, such as \'stag(\"*\")\' or \'word(\"*\")\'.  Try rephrasing the search query\n");            
		return(NULL);
	}
    reader->dots=0;
    
    /* Do the lookup */
    *return_hits=do_recursive_lookup(ls,0,map->len,"", this_sgrep);

    ana_free(&(this_sgrep->this_book->anastasia_memory), reader->saved_array);

    /* Check for the one region "array" */
    if (reader->one.start!=INT_MAX) {
		current= (struct RegionStruct *) ana_malloc(&(this_sgrep->this_book->anastasia_memory), sizeof(Region), __FILE__, __LINE__);
		current[0]=reader->one;
		len=1;
    } else {
		current=NULL;
		len=0;
    }

    /* Do the final merge */
    for(i=0;i<=reader->max;i++) {
		if (reader->sizes[i]) {
		    if (current) {
				Region *new_current;
				reader->lists_merged++;
				reader->regions_merged+=len+reader->sizes[i];
				new_current=merge_regions(this_sgrep, len,current, reader->sizes[i],reader->regions[i], &len);
				ana_free(&(this_sgrep->this_book->anastasia_memory), current);
				ana_free(&(this_sgrep->this_book->anastasia_memory), reader->regions[i]);
				current=new_current;
				while(reader->dots<reader->regions_merged) {
					reader->dots+=DOT_REGIONS;
				}
		    } else {
				current=reader->regions[i];
				len=reader->sizes[i];
		    }
		}
    }
    /* FIXME: This loop could be avoided. */
    result=new_region_list(this_sgrep);
    result->nested=1;
    reader->lists_merged++;
    reader->regions_merged+=len;
    /* sgrep_progress(sgrep,"\nCreating region list.."); */
    for(i=0;i<len;i++) {
		add_region(result,current[i].start,current[i].end);
    }
    if (current) ana_free(&(this_sgrep->this_book->anastasia_memory), current);
    return result;
}

void read_unsorted_postings(const char *entry, const unsigned char *regions, struct LookupStruct *ls) {
    Region r;
    RegionList *list;
    IndexBuffer *map_buffer;
    int size;
    SGREPDATA(ls);
    list=ls->data.reader;
    size=LIST_SIZE(list);
    map_buffer=new_map_buffer(this_sgrep,entry,regions);
    if (get_region_index(map_buffer,&r)) {
		add_region(list,r.start,r.end);
		while(get_region_index(map_buffer,&r)) {
		    add_region(list,r.start,r.end);
		}
    } else {
		ls->stop_words++;
    }
    delete_map_buffer(this_sgrep,map_buffer);
    return;
}

int do_recursive_lookup(struct LookupStruct *ls, int s,int e, const char *pstr, struct SgrepStruct *this_sgrep) {
    const char *str;
    /* Yes, i'd really like to use C++ variable lenght arrays here... */
    char npstr[max_term_len+1];
    int middle=(e-s)/2;
    int rc,lc, result;

    /* Rebuild current entry */
	IndexFileReadPtr index_read, clone_index;
	clone_index= (struct IndexFileRead *) get_hash_lilo_obj(ls->sgrep->this_book->index_objects, s+middle, NULL, ls->sgrep->this_book, 1);
        if (clone_index==NULL) {
            return 0;
        }
	//Clone the index read
	clone_index_obj(clone_index, &index_read, this_sgrep->this_book, this_sgrep->current_pool);
	str=(const char *) index_read->the_str;
	if (str[0]>0) {
		assert(pstr!=NULL);
		strncpy(npstr,pstr,str[0]);
    }
    strncpy(npstr+str[0],str+1,max_term_len-str[0]);
    if (ls->end) {
		int r=0;
		/* Look up a region */
		lc=strncmp(ls->begin,npstr,strlen(ls->begin));
		rc=strncmp(npstr,ls->end,strlen(ls->end));
		if (lc<=0 && middle>0) {
		    r+=do_recursive_lookup(ls, s, s+middle, npstr, this_sgrep);
		}
		if (lc<=0 && rc<=0) {
			/* Found */
			r++;
			if ((!index_read->all_read) && (!read_all_postings(index_read, ls->sgrep->this_book, this_sgrep))) {
				dispose_clone_index_obj(index_read, ls->sgrep->this_book);
				return 0;
			}
			str=(const char *) index_read->the_str;
			ls->callback(npstr,(const unsigned char *)str+2+strlen(str+1),ls);
		}
		if (rc<=0 && s+middle<e-1) {
		    r+=do_recursive_lookup(ls, s+middle+1, e, npstr, this_sgrep);
		}
		dispose_clone_index_obj(index_read, ls->sgrep->this_book);
		return r;
    }
    
	/* Lookup exact */
	lc=strcmp(ls->begin,npstr);
	if (lc<0 && middle>0) {
	    result = do_recursive_lookup(ls, s, s+middle, npstr, this_sgrep);
		dispose_clone_index_obj(index_read, ls->sgrep->this_book);
		return result;
	} else if (lc>0 && s+middle<e-1) {
	    result = do_recursive_lookup(ls, s+middle+1, e, npstr, this_sgrep);
		dispose_clone_index_obj(index_read, ls->sgrep->this_book);
		return result;
	} else if (lc==0) {
	    /* Found */
		if ((!index_read->all_read) && (!read_all_postings(index_read, ls->sgrep->this_book, this_sgrep))) {
			dispose_clone_index_obj(index_read, ls->sgrep->this_book);
			return 0;
		}
		str=(const char *)index_read->the_str;
		ls->callback(npstr,(const unsigned char *)(str+2+strlen(str+1)),ls);
		dispose_clone_index_obj(index_read, ls->sgrep->this_book);
		return 1;
	}
	/* Not found */
	dispose_clone_index_obj(index_read, ls->sgrep->this_book);
	return 0;
}

void read_and_sort_postings(const char *entry, const unsigned char *regions, struct LookupStruct *ls) {
    IndexBuffer *map_buffer;
    struct SortingReaderStruct *read=&ls->data.sorting_reader;
    int i;
    Region first,tmp;
    Region *array;
    int size,length;
    SGREPDATA(ls);
    /* Initialize */
    map_buffer=new_map_buffer(this_sgrep,entry,regions);
    array=read->saved_array;
    size=read->saved_size;
    length=0;
    first=read->one;
#define ARRAY_PUSH(ARRAY,VALUE,SIZE,LENGTH) do { \
  if ((LENGTH)==(SIZE)) { \
    (SIZE)+=(SIZE)/2; \
    ( ARRAY)=(Region *) ana_realloc(&(this_sgrep->this_book->anastasia_memory), (void **) &ARRAY, sizeof(Region)*LENGTH, sizeof(Region)*(SIZE), __FILE__, __LINE__); \
  }  \
  if ((ARRAY)==NULL) return; \
  (ARRAY)[(LENGTH)++]=(VALUE); } while(0)
    /* Read */
    while(get_region_index(map_buffer,&tmp)) {
		if (first.start<=tmp.start) {
		    if (first.start<tmp.start || first.end<tmp.end) {
				/* First is before. Add it */
				ARRAY_PUSH(array,first,size,length);
				if (array==NULL) return;
				first.start=INT_MAX;
				read->one.start=INT_MAX;
		    } else {
				assert(first.start==tmp.start);
				if (first.end==tmp.end) {
				    /* Same region, skip first */
				    first.start=INT_MAX;
				    read->one.start=INT_MAX;
				}
		    }
		}
		ARRAY_PUSH(array, tmp,size,length);
		if (array==NULL) return;
    }
    delete_map_buffer(this_sgrep,map_buffer);
    /* Empty entry */
    if (length==0) {
		ls->stop_words++;
		return;
    }
    /* first may sometimes be also last :) */
    if (first.start!=INT_MAX) {
		ARRAY_PUSH(array,first, size, length);
		if (array==NULL) return;
		read->one.start=INT_MAX;
    }
    /* If size==1, we just save the new list */
    if (length==1) {
		read->one=tmp;
		return;
    }
    read->saved_array=array;
    read->saved_size=size;
    /* Found first having right size */
    for(i=0; (1<<i)<length ; i++)
    ;   //yes we need the semi colon 
	    while(read->sizes[i]>0) {
			/* We need to merge */
			Region *merged=NULL;
			int merged_length;
			read->lists_merged++;
			read->regions_merged+=length+read->sizes[i];
			merged=merge_regions(this_sgrep, length,array, read->sizes[i],read->regions[i], &merged_length);
			/*this could fail.  So clean up and abort, leaving existing regions where they are*/
			if (!merged) {
				read->lists_merged--;
				read->regions_merged-=(length+read->sizes[i]);
				return;
			}
			/* Free the old array(s) */
			if (array!=read->saved_array) {
			    ana_free(&(this_sgrep->this_book->anastasia_memory), array);
			    array=NULL;
			}
			ana_free(&(this_sgrep->this_book->anastasia_memory), read->regions[i]);
			read->regions[i]=NULL;
			read->sizes[i]=0;
			/* We have new array */
			array=merged;
			length=merged_length;
			if ( (1<<i)<length) i++;	
		}
    if (array==read->saved_array) {
		/* Did not need to merge, but need to copy */
		size_t asize=length*sizeof(Region);
		Region *new_array=(Region *) ana_malloc(&(this_sgrep->this_book->anastasia_memory), asize, __FILE__, __LINE__);
		/*again: sgrep does not check for successful allocation of memory.  But we should.  abort again*/
		if  (!new_array) {  
			write_log("Unable to allocate sufficient memory for internal searching procedure (\"read_and_sort_postings\": this will lead to unreliable search results.  This could be because you are carrying out a wild-card search, such as \'stag(\"*\")\' or \'word(\"*\")\'.  Try rephrasing the search query\n");
			read->lists_merged--;
			read->regions_merged-=(length+read->sizes[i]);
			return;
		}
		memcpy(new_array,array,asize);	
		array=new_array;
	}
    /* Save the new array */
    read->regions[i]=array;
    read->sizes[i]=length;
    if (i>read->max) read->max=i;
    while(read->dots<read->regions_merged) {
		read->dots+=DOT_REGIONS;
	}
}

Region *merge_regions(SgrepData *this_sgrep, const int length1, const Region *array1, const int length2, const Region *array2, int *return_length) {
    Region region1,region2;
    Region eor={INT_MAX,INT_MAX};
    int ind1=0;
    int ind2=0;
    int m=0;
    Region *merged;
    /* Initialize */
	merged=(Region *) ana_malloc(&(this_sgrep->this_book->anastasia_memory), (length1+length2)*sizeof(Region), __FILE__, __LINE__);
    /*maybe we tried to do too much and just did not have the memory to cope..abort*/
    if (!merged) {
		if (this_sgrep->this_book->request_rec!=NULL) {
			ap_rputs("<font color=red>A serious error occurred during a search process.</font>", this_sgrep->this_book->request_rec);
		}
        write_log("Unable to allocate sufficient memory for internal searching procedure (\"merge_regions\": this will lead to unreliable search results.  This could be because you are carrying out a wild-card search, such as \'stag(\"*\")\' or \'word(\"*\")\'.  Try rephrasing the search query\n");
        *return_length=0;
    	return(NULL);
    }
    region1=array1[0];
    region2=array2[0];
    /* Do the job */
    while(ind1<length1 || ind2<length2) {
		if (region1.start<region2.start) {
		    /* Region in list 1 is first */
		    merged[m]=region1;
		    region1= (++ind1 < length1) ? array1[ind1] : eor;
		} else if (region1.start>region2.start) {
		    /* Region in list 2 is first */
		    merged[m]=region2;
		    region2= (++ind2 < length2) ? array2[ind2] : eor;
		} else if (region1.end<region2.end) {
		    /* Same start point, region in list 1 is first */
		    merged[m]=region1;
			region1= (++ind1 < length1) ? array1[ind1] : eor;
		} else if (region1.end==region2.end) {
		    /* Same region in both lists */
		    merged[m]=region1;
		    region1= (++ind1 < length1) ? array1[ind1] : eor;
			region2= (++ind2 < length2) ? array2[ind2] : eor;
		} else {
		    /* Same start point, region in list 2 is first */
		    merged[m]=region2;
		    region2= (++ind2 < length2) ? array2[ind2] : eor;
		}
		m++;
	}
    assert( m >= ((length1>length2) ? length1:length2));
    *return_length=m;
    return merged;
}

IndexBuffer *new_map_buffer(SgrepData *this_sgrep, const char *entry, const unsigned char *buf) {
    IndexBuffer *n;
    n=(struct IndexBufferStruct *) ana_malloc(&(this_sgrep->this_book->anastasia_memory), sizeof(IndexBuffer), __FILE__, __LINE__);
    n->list.map.buf=buf;
    n->list.map.ind=0;
    n->block_used=SHRT_MIN;
    n->last_index=0;
    n->last_len=strlen(entry)-1;
    n->str=(char *) apr_pstrdup(this_sgrep->current_pool, entry);
    n->saved_bytes=-1;
    return n;
}

void delete_map_buffer(SgrepData *this_sgrep,IndexBuffer *map_buffer) {
    assert(map_buffer->block_used==SHRT_MIN);
    map_buffer->block_used=0;
    ana_free(&(this_sgrep->this_book->anastasia_memory), map_buffer->str);
    ana_free(&(this_sgrep->this_book->anastasia_memory), map_buffer);
}

int get_region_index(IndexBuffer *buf, Region *region) {
    int saved_index;
    int s,e;
    saved_index=buf->last_index;
    assert(saved_index!=INT_MAX);
    s=get_entry(buf);
    if (s==INT_MAX) {
		buf->last_index=INT_MAX;
		return 0;
    }
    if (buf->last_len>0) {
		/* We are in compression hack state */
		if (s==saved_index) {
		    /* The zero tag: either switch to normal state or this is
		     * escaped zero tag */
		    s=get_entry(buf);
		    if (s==saved_index && s!=0) {
				/* It was an escaped zero tag */
				region->start=s;
				region->end=s+buf->last_len-1;
				return 1;
		    }
		    /* Switch to normal state. Need to read also end index */
		    e=get_entry(buf);
		    assert(e!=INT_MAX);
		    buf->last_len=0-(e-s+1);
		    region->start=s;
		    region->end=e;
		    return 1;
		}
		/* Ending point was compressed out */
		region->start=s;
		region->end=s+buf->last_len-1;
		return 1;
	}
    /* Normal state. Read also end point */
    e=get_entry(buf);
    assert(e!=INT_MAX);
    if (e-s+1==-buf->last_len) {
		/* Same length twice. Switch to CH state */
		buf->last_len=e-s+1;
    } else {
		/* Different length. Just save the length */
		buf->last_len=0-(e-s+1);
    }
    region->start=s;
    region->end=e;
    return 1;
}

int read_all_postings(IndexFileReadPtr the_read,process_book_ptr this_book_process, SgrepData *this_sgrep) {
	unsigned char i=0;
	size_t offset;
	apr_file_t *this_file=this_book_process->in_idx;
 	size_t read_length;
	offset=2+strlen((const char *)the_read->the_str+1);
  	apr_off_t _where = this_book_process->lomem_idx->entries_offset+the_read->place+offset;
	apr_file_seek(this_file, APR_SET, &_where);
	//now we read every compressed int
	while (i!=END_OF_POSTINGS_TAG) {
		unsigned char ch;
   		apr_size_t _count = 1;
		apr_file_read(this_file, &ch, &_count);
		i = ch;
		if (i==NEGATIVE_NUMBER_TAG) {
			_count = 1;
			apr_file_read(this_file, &ch, &_count);
			i=ch;
		}
		if (i!=END_OF_POSTINGS_TAG) {
  			apr_off_t _where = 0;
			if (i<127) {}
			else if ((i&(128+64))==128) _where = 1;  //16 bits
			else if ((i&(128+64+32))==128+64) _where = 2;  //24 bits
			else if ((i&(128+64+32+16))==128+64+32) _where = 3;  //32 bits
			else if(i==0xf0) _where = 4;  //40 bits
			else {
				write_log("Error reading Anastasia index object: apparently, corrupted index file.\n");
				return 0;
			}
			if (_where) apr_file_seek(this_file, APR_CUR, &_where);
		}
	}
	//so: where are we?
	_where = 0;
	apr_file_seek(this_file, APR_CUR, &_where);
	read_length=_where-this_book_process->lomem_idx->entries_offset-the_read->place;
	if (read_length<=256) {
		the_read->all_read=1;
		return 1;
	}
	else {
		ana_free(&(this_book_process->anastasia_memory), the_read->the_str);
		the_read->the_str = (unsigned char *) 						ana_malloc(&(this_book_process->anastasia_memory), read_length, __FILE__, __LINE__);
		if (!the_read->the_str) {
			write_log("Unable to allocate memory for search results.\n");
			return 0;
		}
  		apr_off_t _where = this_book_process->lomem_idx->entries_offset+the_read->place;
		apr_file_seek(this_file, APR_SET, &_where);
   		apr_size_t _count = sizeof(char) * read_length;
		apr_file_read(this_file, (void *)the_read->the_str, &_count);
		if (_count != sizeof(char) * read_length) {
			//Didn't read the number of characters required.. means we probably read past the end of the array
			if (APR_EOF == apr_file_eof(this_file)) {
				write_log("Error: read_all_posting read past the end of a file.\n");
			} else {
				write_log("Error: in read_all_posting function.\n");
			}
			return 0;
		}
		//add more to current mem use in this hashlilo
		the_read->allocated=read_length;
		the_read->all_read=1;
		return 1;
	}
}

unsigned int get_entry(IndexBuffer *buf) {
    int r=get_integer(buf);
    if (r==INT_MAX) return r;
    buf->last_index+=r;
    assert(buf->last_index>=0);
    return buf->last_index;
}

int get_integer(IndexBuffer *buf) {
    unsigned char i;
    int r;
    int negative=0;

    i=get_byte(buf);
    if (i==NEGATIVE_NUMBER_TAG) {
	negative=1;
	i=get_byte(buf);
    }
    if (i==END_OF_POSTINGS_TAG) {
	/* Found end of index */
	return INT_MAX;
    }
    else if (i<127) r=i; /* 8 bits starting with 0 */
    else if ((i&(128+64))==128) {
	/* 16 bits starting with 10 */
	r=((i&63)<<8)|get_byte(buf);
    }
    else if ((i&(128+64+32))==128+64) {
	/* 24 bits starting with 110 */
	r=(i&31)<<16|(get_byte(buf)<<8);
	r=r|get_byte(buf);
    }
    else if ((i&(128+64+32+16))==128+64+32) {
	/* 32 bits starting with 1110 */
	r=(i&15)<<24|(get_byte(buf)<<16);
	r|=get_byte(buf)<<8;
	r=r|get_byte(buf);      
    } else if(i==0xf0) {
	/* 40 bits starting with 0xf0 */
	r=get_byte(buf)<<24;
	r|=get_byte(buf)<<16;
	r|=get_byte(buf)<<8;
	r|=get_byte(buf);
    } else {
	assert(0 && "Corrupted index file");
	abort();
    }
    return (negative)?-r:r;
}

unsigned char get_byte(IndexBuffer *buf) {
    assert(buf->block_used==SHRT_MIN);
    return buf->list.map.buf[buf->list.map.ind++];
}

int add_region_to_index(IndexWriter *writer, const char *str, int start, int end) {
    IndexBuffer *ib;
    int len;
    SGREPDATA(writer);

    
    if (end<start) {
	sgrep_error(this_sgrep,"BUG: ignoring zero sized region\n");
	return SGREP_OK;
    }
    ib=find_index_buffer(writer,str);
    writer->postings++;
    
    /* Check for stopword */
    if (ib->last_index==-1) return SGREP_OK;
    len=end-start+1;
    /* FIXME: the start!=0 condition should be removed, but removing
     * it needs change in index file format! (In other words: a design
     * bug. Sorry about that. */
    if (ib->last_len==len && start!=0) {
	/* This is the compression hack: skip the end point in entries
	 * having same length */
	if (start==ib->last_index) {
	    /* Bad luck: we have to add zero entry tag. So we duplicate
	     *  it */
	    add_entry(writer,ib,start, this_sgrep);
	    add_entry(writer,ib,start, this_sgrep);
	} else {
	    add_entry(writer,ib,start, this_sgrep);
	}
   } else if (len==-ib->last_len) {
	/* Previous was same lenght as this. Switch to compression hack
	 * state */
	ib->last_len=len;

	add_entry(writer,ib,start, this_sgrep);
	add_entry(writer,ib,end, this_sgrep);
    } else {	
	/* Lengths did not match. If we we're in compression hack
	 * state, need to add tag to switch state */
	if (ib->last_len>0) {
	    /* Add the zero entry tag to switch state */
	    add_entry(writer, ib, ib->last_index, this_sgrep);
	}
	/* Normal entry */
	ib->last_len=-len;
	add_entry(writer,ib,start, this_sgrep);
	add_entry(writer,ib,end, this_sgrep);
    }
    if (writer->failed) {
	return SGREP_ERROR;
    } else {
	return SGREP_OK;
    }
}

IndexBuffer *find_index_buffer(IndexWriter *writer, const char *str) {
	IndexBuffer **n;
    int h;
    SgrepData *sgrep=writer->sgrep;

    h=hash_function(writer->hash_size,str);

    n=&writer->htable[h];
    while(*n!=NULL) {
	/* There already is entries with same hash value */
	if (strcmp(str,(*n)->str)!=0) {
	    /* No match */
	    n=&(*n)->next;
	} else {
	    /* Found existing entry */
	    return *n;
	}
    }
    writer->terms++;
    *n=new_writer_index_buffer(writer);
	(*n)->str=(char *)apr_pstrdup(sgrep->current_pool, str);
    (*n)->last_len=strlen(str)-1;
    writer->total_string_bytes+=strlen(str)+1;
    return *n;
}

void add_entry(IndexWriter *writer,IndexBuffer *buf, int index, struct SgrepStruct *this_sgrep) {
    assert(index>=0);
    index-=buf->last_index;
    buf->last_index+=index;
    add_integer(writer,buf,index, this_sgrep);
}

unsigned int hash_function(int size,const char *str) {
    int i;
    unsigned int r=0;
    for(i=0;str[i] && i<6;i++) {
	r=r*256+((unsigned char *)str)[i];
    }
    return r%size;
}

static IndexBuffer *new_writer_index_buffer(IndexWriter *writer) {
    struct SgrepStruct *sgrep=writer->sgrep;
    if (writer->free_index_buffers==NULL ||
	writer->first_free_index_buffer==INDEX_BUFFER_ARRAY_SIZE) {
		struct IndexBufferArray *b;
		b=(struct IndexBufferArray *)ana_malloc(&(sgrep->this_book->anastasia_memory), sizeof(struct IndexBufferArray), __FILE__, __LINE__);
		memset(b, 1, sizeof(struct IndexBufferArray));
		b->next=writer->free_index_buffers;
		writer->first_free_index_buffer=0;
		writer->free_index_buffers=b;
    }
    return &writer->free_index_buffers->bufs[writer->first_free_index_buffer++];    
}

void add_integer(IndexWriter *writer, IndexBuffer *buf, int num, struct SgrepStruct *this_sgrep) {
    if (num<0) {
	/* Negative number: Add the NEGATIVE_NUMBER tag */
	add_byte(writer,buf,NEGATIVE_NUMBER_TAG, this_sgrep);
	/* Add the number as positive integer */
	num=-num;
    }
    if (num<127) {
	/* Eight bits with being 0 */
	/* 01111111 is reserved for end of buffer. Zero is OK */
	add_byte(writer,buf,((unsigned char) num), this_sgrep);
	writer->entry_lengths[0]++;
    } else if (num<(1<<14)) {
	/* 16 bits with first being 10  */
	add_byte(writer,buf,((unsigned char) (num>>8)|128), this_sgrep);
	add_byte(writer,buf,((unsigned char) num&255), this_sgrep);
	writer->entry_lengths[1]++;
    } else if (num<(1<<21)) {
	/* 24 bits with first being 110 */
	add_byte(writer,buf,(unsigned char) (num>>16)|(128+64), this_sgrep);
	add_byte(writer,buf,(unsigned char) (num>>8)&255, this_sgrep);
	add_byte(writer,buf,(unsigned char) num&255, this_sgrep);	 
	writer->entry_lengths[2]++;
    } else if (num<(1<<28)) {
	/* 32 bits with first being 1110 */
	add_byte(writer,buf,(unsigned char) (num>>24)|(128+64+32), this_sgrep);
	add_byte(writer,buf,(unsigned char) (num>>16)&255, this_sgrep);
	add_byte(writer,buf,(unsigned char) (num>>8)&255, this_sgrep);
	add_byte(writer,buf,(unsigned char) num&255, this_sgrep);
	writer->entry_lengths[3]++;
    } else if (num<=0x0fffffff) {
	/* First byte 11110000*/
	add_byte(writer,buf,(unsigned char) 0xf0, this_sgrep);
	add_byte(writer,buf,(unsigned char) (num>>24)&255, this_sgrep);
	add_byte(writer,buf,(unsigned char) (num>>16)&255, this_sgrep);
	add_byte(writer,buf,(unsigned char) (num>>8)&255, this_sgrep);
	add_byte(writer,buf,(unsigned char) num&255, this_sgrep);
    } else {
	/* More than 32 bits. Shouldn't happen with ints. */
	sgrep_error(writer->sgrep,"Index value %u is too big!\n",num);
    }
}

void add_byte(IndexWriter *writer,IndexBuffer *buf, unsigned char byte, struct SgrepStruct *this_sgrep) {
    int used;
    writer->total_postings_bytes++;
    if (buf->block_used>=0) {
	/* Internal block */
	if (buf->block_used<INTERNAL_INDEX_BLOCK_SIZE) {
	    buf->list.internal.ibuf[buf->block_used++]=byte;
	    return;
	}
	/* Does not fit into internal block anymore. Make it external */
	assert(writer->spool_used<writer->spool_size);
	writer->spool[writer->spool_used].next=INT_MIN;
	memcpy(writer->spool[writer->spool_used].buf,
	       buf->list.internal.ibuf,INTERNAL_INDEX_BLOCK_SIZE);
	buf->list.external.first=writer->spool_used;
	buf->list.external.current=writer->spool_used;
	buf->list.external.bytes=buf->block_used;
	buf->block_used=-INTERNAL_INDEX_BLOCK_SIZE;
	writer->spool_used++;
    }
    /* External block */    
    used=-buf->block_used;
    if (used==EXTERNAL_INDEX_BLOCK_SIZE) {
	new_block(writer,buf,byte);
    } else {
	writer->spool[buf->list.external.current].buf[used]=byte;
	buf->block_used--;
	buf->list.external.bytes++;
    }
    assert(writer->spool_used<=writer->spool_size);
    if (writer->spool_used==writer->spool_size) index_spool_overflow(writer, this_sgrep);
}

void new_block(IndexWriter *writer,IndexBuffer *buf, unsigned char byte) {
    assert(writer->spool_used<writer->spool_size);
    assert(writer->spool[buf->list.external.current].next==INT_MIN);

    writer->spool[buf->list.external.current].next=writer->spool_used;
    buf->list.external.current=writer->spool_used;
    writer->spool[writer->spool_used].buf[0]=byte;
    writer->spool[writer->spool_used].next=INT_MIN;
    buf->list.external.bytes++;
    buf->block_used=-1;
    writer->spool_used++;
}

void index_spool_overflow(IndexWriter *writer, struct SgrepStruct *this_sgrep) {
    int i,j;
    IndexBuffer *l;
    IndexBuffer **term_array;
    int esize;
    TempFile *temp_file;
    FILE *load_file;

    term_array=(IndexBuffer **) ana_malloc(&(this_sgrep->this_book->anastasia_memory), sizeof(IndexBuffer *)*writer->terms, __FILE__, __LINE__);
    if (writer->htable) {
        /* Make an array of the hash table */
        j=0;
        for(i=0;i<writer->hash_size;i++) {
            for(l=writer->htable[i];l;l=l->next) {
                term_array[j++]=l;
            }
        }
        qsort(term_array,writer->terms,
              sizeof(IndexBuffer *),index_buffer_compare);
    } else {
        /* Make an array of the sorted buffers */
        j=0;
        for(l=writer->sorted_buffers;l;l=l->next) {
            assert(j<writer->terms);
            term_array[j++]=l;
        }
        assert(j==writer->terms);
    }
    temp_file=create_temp_file(this_sgrep);
    if (!temp_file) {
        sgrep_error(this_sgrep,"Can't write memory load\n");
        writer->failed=1;
        writer->spool_used=0;
        ana_free(&(this_sgrep->this_book->anastasia_memory), term_array);
        return;
    }
    load_file=temp_file_stream(temp_file);
    for(i=0;i<writer->terms;i++) {
        /*this could slow things dwn hugely: have to read EVERY term to check whether we have external blocks*/
        /*could speed this up by putting block_used in the stub..but would cost precious bytes..*/
        /*actually, coz putting it in the stub uses no extra bytes, we do this*/
        if (term_array[i]->block_used<0) {
            /* Only write external buffers. First the entry string. */
            fputs(term_array[i]->str,load_file);
            fputc(0,load_file);
            /* Then the postings */
            put_int(term_array[i]->list.external.bytes,load_file);
            esize=fwrite_postings(writer,term_array[i],load_file);
            term_array[i]->saved_bytes+=esize;
            assert(esize==term_array[i]->list.external.bytes);
            /* Now empty the entry */
            term_array[i]->block_used=0;
        }
    }
    ana_free(&(this_sgrep->this_book->anastasia_memory), term_array);
    fflush(load_file);
    if (ferror(load_file)) {
        sgrep_error(this_sgrep,"Failed to write memory load: %s\n",strerror(errno));
        delete_temp_file(temp_file);
        writer->failed=1;
    } else {
        writer->memory_load_files[writer->memory_loads++]=temp_file;
    }
    writer->spool_used=0;
}


int index_buffer_compare(const void *first, const void *next) {
    return strcmp(
	(*(const IndexBuffer **)first)->str,
	(*(const IndexBuffer **)next)->str
	);
}

static int put_int(int i,FILE* stream) {
	struct {
		unsigned a1 : 1;
		unsigned a2 : 1;
		unsigned a3 : 1;
		unsigned a4 : 1;
		unsigned a5 : 1;
		unsigned a6 : 1;
		unsigned a7 : 1;
		unsigned a8 : 1;
		unsigned a9 : 1;
		unsigned a10 : 1;
		unsigned a11 : 1;
		unsigned a12 : 1;
		unsigned a13 : 1;
		unsigned a14 : 1;
		unsigned a15 : 1;
		unsigned a16 : 1;
		unsigned a17 : 1;
		unsigned a18 : 1;
		unsigned a19 : 1;
		unsigned a20 : 1;
		unsigned a21 : 1;
		unsigned a22 : 1;
		unsigned a23 : 1;
		unsigned a24 : 1;
		unsigned a25 : 1;
		unsigned a26 : 1;
		unsigned a27 : 1;
		unsigned a28 : 1;
		unsigned a29 : 1;
		unsigned a30 : 1;
		unsigned a31 : 1;
		unsigned a32 : 1;
	} flags;
	int c, d, e, f;
	memcpy(&flags, &i, 4);
	memcpy(&flags, &i, 4);
    c=i>>24;
	d=i>>16;
	e=i>>8;
	f=i&255;
	putc(c,stream);
	putc(d,stream);
	putc(e,stream);
	putc(f,stream);
  	return 4;
}

int fwrite_postings(IndexWriter *writer, IndexBuffer *tmp, FILE *stream) {
    int bytes=0;
    /* Now the regions */
    if (tmp->block_used==0) {
        /* This is possible, when this buffer was written out 
 *          * in some previous memory load, and there has been no new
 *                   * entries in this buffer since or when this is a stop word
 *                            * buffer and therefore contains no entries */
        return 0;
    } else if (tmp->block_used>0) {
        bytes+=tmp->block_used;
        fwrite(tmp->list.internal.ibuf,tmp->block_used,1,stream);
    } else {
        int esize;
        struct IndexBlock *ind=&writer->spool[tmp->list.external.first];
        esize=tmp->list.external.bytes;
        bytes=esize;
        while(ind->next!=INT_MIN) {
            esize-=EXTERNAL_INDEX_BLOCK_SIZE;
            fwrite(ind->buf,EXTERNAL_INDEX_BLOCK_SIZE,1,stream);
            ind=&writer->spool[ind->next];
        }
        assert(esize<=EXTERNAL_INDEX_BLOCK_SIZE);
        fwrite(ind->buf,esize,1,stream);
    }
    return bytes;
}

