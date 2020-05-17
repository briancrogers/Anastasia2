#include "sgrep.h"
#include "common.h"
#include <apr_strings.h>
#include <sys/types.h>
#include <unistd.h>


TempFile *create_temp_file(SgrepData *this_sgrep) {
#if HAVE_UNIX
    TempFile *temp_file=create_named_temp_file(this_sgrep);
    if (!temp_file) return NULL;
    if (remove(temp_file->file_name)==0) {
	ana_free(&(this_sgrep->this_book->anastasia_memory), temp_file->file_name);
	temp_file->file_name=NULL;
    } else {
	sgrep_error(this_sgrep,"Failed to unlink tempfile '%s':%s\n",
		    temp_file->file_name,strerror(errno));
    }
    return temp_file;
#else
    return create_named_temp_file(this_sgrep);
#endif
}

FILE *temp_file_stream(TempFile *temp_file) {
    assert(temp_file);
    return temp_file->stream;
}

TempFile *create_named_temp_file(SgrepData *this_sgrep) {

#if HAVE_UNIX || HAVE_WIN32
    const char *prefix;
    SgrepString *file_name;
    char tmp[50];
    int fd;
    static int i=0;
    int j;
#endif

    /* The things which WIN32 and unix have in common  */
    TempFile *temp_file;
    temp_file= (struct TempFileStruct *) ana_malloc(&(this_sgrep->this_book->anastasia_memory), sizeof(TempFile), __FILE__, __LINE__);
    temp_file->sgrep=this_sgrep;
    temp_file->stream=NULL;
    temp_file->prev=NULL;
    
//#if HAVE_UNIX
#ifndef O_BINARY
#define O_BINARY 0
#endif
#if HAVE_WIN32 || HAVE_UNIX
    file_name=new_string(this_sgrep,1024);
    prefix=getenv(ENV_TEMP);
    if (!prefix) {
	prefix=DEFAULT_TEMP_DIR;
    }
    for(j=0;temp_file->stream==NULL && j<1000;j++) {
	i++;
	string_clear(file_name);
	string_cat(file_name,prefix);
	string_cat(file_name,"/");
	string_cat(file_name,TEMP_FILE_PREFIX);
	sprintf(tmp,"%d-%d",getpid(),i);
	string_cat(file_name,tmp);
	string_cat(file_name,TEMP_FILE_POSTFIX);
	fd=open(string_to_char(file_name),O_RDWR|O_BINARY|O_CREAT|O_EXCL,0600);
	if (fd>=0) {
	    temp_file->stream=fdopen(fd,"wb+");
	}
    }
    if (temp_file->stream==NULL) {
	sgrep_error(this_sgrep,"Failed to create temp file '%s': %s\n",
		    string_to_char(file_name),strerror(errno));
	ana_free(&(this_sgrep->this_book->anastasia_memory), temp_file);
	delete_string(file_name);
	return NULL;
    }
    temp_file->file_name=apr_pstrdup(this_sgrep->current_pool, string_to_char(file_name));
    delete_string(file_name);
    /* sgrep_error(sgrep,"tempfile: %s(%d)\n",temp_file->file_name,fd); */
//#elif HAVE_WIN32 || HAVE_MAC
#elif HAVE_MAC
	temp_file->file_name=(char *) ana_malloc(this_sgrep->this_book, 100);
//	temp_file->file_name=(char *)sgrep_malloc(L_tmpnam);
	tmpnam(temp_file->file_name);
	temp_file->stream=fopen(temp_file->file_name,"wb+");
	if (temp_file->stream==NULL) {
		sgrep_error(sgrep,"Failed to create temp file '%s': %s\n",
		    temp_file->file_name,strerror(errno));		
		ana_free(&(this_sgrep->this_book->anastasia_memory), temp_file->file_name);
		ana_free(&(this_sgrep->this_book->anastasia_memory), temp_file);
		return NULL;
	}
	/* fprintf(stderr,"tempfile: %s\n",temp_file->file_name); */
#elif HAVE_MAC
	/*just return NULL for the moment; we will make this later...*/
	return NULL;
#else
#error "needs create_temp_file for target"
#endif

	temp_file->next=this_sgrep->first_temp_file;
    if (temp_file->next) temp_file->next->prev=temp_file;
    this_sgrep->first_temp_file=temp_file;
	return temp_file;
}

int delete_temp_file(TempFile *temp_file) {
    SGREPDATA(temp_file);
    fclose(temp_file->stream);
    if (temp_file->file_name) {
	if (remove(temp_file->file_name)) {
	    sgrep_error(this_sgrep,"Failed to remove temp file '%s':%s\n",
			temp_file->file_name,strerror(errno));
	}
	ana_free(&(this_sgrep->this_book->anastasia_memory), temp_file->file_name);
	temp_file->file_name=NULL;
    }
    if (this_sgrep->first_temp_file==temp_file) {
	this_sgrep->first_temp_file=temp_file->next;
    }
    if (temp_file->next) {
	temp_file->next->prev=temp_file->prev;
    }
    if (temp_file->prev) {
	temp_file->prev->next=temp_file->next;
    }
    ana_free(&(this_sgrep->this_book->anastasia_memory), temp_file);
    return SGREP_OK;
}

int unmap_file(SgrepData *this_sgrep,void *map, size_t size) {
    return SGREP_OK;
}

size_t map_file(SgrepData *this_sgrep,const char *filename,void **map) {
   int fd;
    int len;
    *map=NULL;
   fd=open(filename,O_RDONLY);
    if (fd<0) {
	sgrep_error(this_sgrep,"Failed to open file '%s':%s\n",
		filename,strerror(errno));
	return 0;
    }
    len=lseek(fd,0,SEEK_END);
    if (len<0) {
	sgrep_error(this_sgrep,"lseek '%s':%s",filename,strerror(errno));
	close(fd);
	return 0;
    }
    return len;
}

