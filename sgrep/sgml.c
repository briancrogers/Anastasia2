#include "sgrep.h"
#include "sgml.h"
#include "index.h"
#include "common.h"
#include <apr_strings.h>
#include <ctype.h>

SGMLScanner *new_sgml_scanner_common(SgrepData *this_sgrep, FileList *file_list) {
    SGMLScanner *scanner;
    scanner= (struct SGMLScannerStruct *) ana_malloc(&(this_sgrep->this_book->anastasia_memory), sizeof(SGMLScanner), __FILE__, __LINE__);
    scanner->sgrep=this_sgrep;
    scanner->file_list=file_list;
    scanner->file_num=-1;
    scanner->state_stack_ptr=0;

    scanner->maintain_element_stack=1;
    scanner->top=NULL;
    scanner->element_list=NULL;

    scanner->word_chars=new_character_list(this_sgrep);
    switch(this_sgrep->scanner_type) {
    case SGML_SCANNER:
	scanner->name_start_chars=new_character_list(this_sgrep);
	character_list_add(scanner->name_start_chars,SGML_NameStartChars);
	scanner->name_chars=new_character_list(this_sgrep);
	character_list_add(scanner->name_chars,SGML_NameChars);
	break;
    case XML_SCANNER:
	/* NameStart characters */
	scanner->name_start_chars=new_character_list(this_sgrep);
	character_list_add(scanner->name_start_chars,XML_BaseChar);
	character_list_add(scanner->name_start_chars,XML_Ideographic);
	character_list_add(scanner->name_start_chars,SGML_NameStartChars);
	/* Name characters */
	scanner->name_chars=new_character_list(this_sgrep);
	character_list_add(scanner->name_chars,XML_BaseChar);
	character_list_add(scanner->name_chars,XML_Ideographic);
	character_list_add(scanner->name_chars,SGML_NameChars);	
	break;
    case TEXT_SCANNER:
	scanner->name_start_chars=NULL;
	scanner->name_chars=NULL;
	break;
    }
    if (this_sgrep->word_chars) {
	character_list_add(scanner->word_chars,this_sgrep->word_chars);
    } else {
        character_list_add(scanner->word_chars,XML_BaseChar);
	character_list_add(scanner->word_chars,XML_Ideographic);
    }
    scanner->parse_errors=0;

    scanner->type=this_sgrep->scanner_type;
    scanner->ignore_case=this_sgrep->ignore_case;
    scanner->include_system_entities=this_sgrep->include_system_entities;

    scanner->state=SGML_PCDATA;

    scanner->gi=new_string(this_sgrep,MAX_TERM_SIZE);
    scanner->word=new_string(this_sgrep,MAX_TERM_SIZE);
    TERM_PUSH(scanner->word,'w');
    scanner->name2=new_string(this_sgrep,MAX_TERM_SIZE);
    scanner->comment_word=new_string(this_sgrep,MAX_TERM_SIZE);
    scanner->name=new_string(this_sgrep,MAX_TERM_SIZE);
    scanner->literal=new_string(this_sgrep,MAX_TERM_SIZE);
    string_cat(scanner->literal,"xxx");
    scanner->aname=new_string(this_sgrep,MAX_TERM_SIZE);
    TERM_PUSH(scanner->aname,'a');
    scanner->aval=new_string(this_sgrep,MAX_TERM_SIZE);
    TERM_PUSH(scanner->aval,'v');
    scanner->pi=new_string(this_sgrep,MAX_TERM_SIZE);
    TERM_PUSH(scanner->pi,'?');
    scanner->failed=0;

    reset_encoder(scanner,&scanner->encoder);
    return scanner;
}

void sgml_add_entry_to_gclist(SGMLScanner *state, const char *phrase,int start, int end, struct SgrepStruct *this_sgrep) {
    struct PHRASE_NODE *n;
    for(n=state->phrase_list;n!=NULL;n=n->next) {
	if (n->phrase->s[n->phrase->length-1]=='*') {
	    /* Wildcard */
	    if (strncmp(n->phrase->s,phrase,n->phrase->length-1)==0) {
		add_region(n->regions,start,end);
	    
	    }
	} else if (strcmp(n->phrase->s,phrase)==0) {
	    add_region(n->regions,start,end);
	}
    }
}

/*
 * Creates a new empty character list
 */
CharacterList *new_character_list(SgrepData *this_sgrep) {
	CharacterList *a;
    a= (struct CharacterListStruct *) ana_malloc(&(this_sgrep->this_book->anastasia_memory), sizeof(CharacterList), __FILE__, __LINE__);
    memset(a,0,sizeof(CharacterList));
    a->sgrep=this_sgrep;
    return a;
}

/*
 * Parses a given character list string adding them to a CharacterList 
 */
void character_list_add(CharacterList *a,const char *l) {
    int i;
    const unsigned char *list=(const unsigned char *)l;
    int previous;
    int expand_from;
    int current;
    SGREPDATA(a);

    previous=-1;
    expand_from=-1;
    i=0;
    while(list[i]) {
	current=list[i];
	i++;
	if (current=='\\') {
	    if (list[i]=='-') {
		/* Escape also \- */
		i++;
		current='-';
	    } else {
		current=expand_backslash_escape(this_sgrep,list,&i);
	    }
	} else if (current=='-' && i>1 && expand_from==-1) {
	    /* Mark the requirement to expand in next iteration */
	    expand_from=previous;
	    continue;
	}
	
	if (expand_from>=0 && current>=0) {
	    /* A region */
	    int j;
	    for(j=expand_from;j<=current;j++) {
		ADD_CLIST(a,j);
	    }
	} else if (current>=0) {
	    ADD_CLIST(a,current);
	}
	expand_from=-1;
	previous=current;
	current=-1;
    }
    if (expand_from>=0) {
	sgrep_error(this_sgrep,"Character list '%s' contains a region with no endpoint\n",
		    list);
    }
}

void reset_encoder(SGMLScanner *sgmls, Encoder *encoder) {
    SGREPDATA(sgmls);

    switch(this_sgrep->default_encoding) {
    case ENCODING_GUESS:
	switch(sgmls->type) {
	case SGML_SCANNER:
	    encoder->estate=EIGHT_BIT;
	    break;
	case XML_SCANNER:
	    encoder->estate=UTF8_1;
	    break;
	case TEXT_SCANNER:
	    encoder->estate=EIGHT_BIT;
	    break;
	}
	break;
    case ENCODING_8BIT:
	encoder->estate=EIGHT_BIT;
	break;
    case ENCODING_UTF8:
	encoder->estate=UTF8_1;
	break;
    case ENCODING_UTF16:
	encoder->estate=UTF8_1;
	break;
    }
    encoder->prev=-1;
}

SGMLScanner *new_sgml_phrase_scanner(SgrepData *this_sgrep, FileList *file_list, struct PHRASE_NODE *list) {    
    SGMLScanner *scanner;
    scanner=new_sgml_scanner_common(this_sgrep,file_list);
    scanner->phrase_list=list;
    scanner->entry=sgml_add_entry_to_gclist;
    scanner->data=NULL;
    return scanner;
}

void sgml_flush(SGMLScanner *sgmls) {
    SGREPDATA(sgmls);

    /* sgrep_progress(sgrep,"sgml_flush()\n"); */
    pop_elements_to(sgmls,NULL);
    if (sgmls->element_list && sgmls->entry==sgml_add_entry_to_index) {
	ListIterator l;
	Region r;
	struct IndexWriterStruct *writer=
	    (struct IndexWriterStruct *)sgmls->data;

	/* sgrep_progress(sgrep,"Adding element list to index\n"); */
	start_region_search(sgmls->element_list,&l);
	get_region(&l,&r);
	while(r.start!=-1) {
	    add_region_to_index(writer,"@elements",r.start,r.end);	    
	    get_region(&l,&r);
	}
	delete_region_list(sgmls->element_list);
	sgmls->element_list=new_region_list(this_sgrep);
	list_set_sorted(sgmls->element_list,NOT_SORTED);
	sgmls->element_list->nested=1;
    }
    reset_encoder(sgmls,&sgmls->encoder);
    sgmls->state=SGML_PCDATA;
}

int sgml_scan(SGMLScanner *scanner, const unsigned char *buf, int len, int start,int file_num) {
	#define POS (start+i)
	#define NEXT_CH do { encoder->prev=POS; ch=-1; } while(0)
	#define SGML_FOUND(SCANNER,END) do { \
	    sgml_found((SCANNER),state,(END)); if ((SCANNER)->failed) return SGREP_ERROR; \
	} while(0)
    int i; /*just force it to read first time*/
    int ch=-1;
     Encoder *encoder=&scanner->encoder;
    enum SGMLState state=scanner->state;
    SGREPDATA(scanner);
    if (encoder->prev==-1) encoder->prev=start;
    
    scanner->file_num=file_num;
    i=-1;
    ch=-1;
    while(1) {
	if (ch==-1) {
	    /* If no more bytes, break out */
	    if (i>=len) 
	    	break;
	    switch (encoder->estate) {
	    case EIGHT_BIT:
		ch=buf[i++];
		break;
	    case UTF8_1:
		if (buf[i]<0x80) {
	   		 ch=buf[i++];
		} else if (buf[i]<0xc0) {
		    ch=' ';
		    sgrep_error(this_sgrep,"UTF8 decoding error (<0xc0)\n");
		    scanner->parse_errors++;
		    i++;
		} else if (buf[i]<0xe0) {
			 encoder->estate=UTF8_2;
	    	encoder->char1=buf[i];
		    i++;
		    continue;
		} else if (buf[i]<0xf0) {
		    encoder->estate=UTF8_3_1;
			encoder->char1=buf[0];
		 	 i++;
		    continue;
		} else if (buf[i]==0xfe) {
		    encoder->estate=UTF16_BIG_START;
		    i++;
		    continue;
		} else if (buf[i]==0xff) {
		    encoder->estate=UTF16_SMALL_START;
		    i++;
		    continue;
		} else {
		    ch=' ';
			sgrep_error(this_sgrep,"UTF8 decoding error (%d>=0xf0)\nn",
			buf[i]);
		    scanner->parse_errors++;
		    i++;
		}
		break;
	    case UTF8_2:
			if (buf[i]>=0x80 && buf[i]<=0xbf) {
			    ch=((encoder->char1&0x1f)<<6) | (buf[i]&0x3f);
		    encoder->estate=UTF8_1;
		    i++;
		} else {
		    ch=' ';
			 sgrep_error(this_sgrep,"UTF8 decoding error 2 (%d<0x80 || >0xbf)\n",
				buf[i]);
		    scanner->parse_errors++;
		    encoder->estate=UTF8_1;
		    i++;
		}
		break;
	    case UTF8_3_1:
			if (buf[i]>=0x80 && buf[i]<=0xbf) {
		    encoder->char2=buf[i];
		    encoder->estate=UTF8_3_2;
		    i++;
		    continue;
		} else {
		    sgrep_error(this_sgrep,"UTF8 decoding error: 3,1 (%d<0x80 || >0xbf)\n",
				buf[i]);
		    ch=' ';
		    scanner->parse_errors++;
		    encoder->estate=UTF8_1;
		    i++;
		}
		break;
	    case UTF8_3_2:
		if (buf[i]>=0x80 && buf[i]<=0xbf) {
		    ch= ((encoder->char1&0x0f)<<12) |
			((encoder->char2&0x3f)<<6) | (buf[i]&0x3f);
		    encoder->estate=UTF8_1;
		    i++;
		    /* fprintf(sgrep->error_stream,"%x\n",ch); */
		} else {
		    sgrep_error(this_sgrep,"UTF8 decoding error: 3,2 (%d<0x80 || >0xbf)\n",
				buf[i]);
		    ch=' ';
		    scanner->parse_errors++;
		    encoder->estate=UTF8_1;
		    i++;
		}
		break;
	    case UTF16_BIG_START:
		if (buf[i]==0xff) {
		    encoder->estate=UTF16_BIG;
		    i++;
		    continue;
		} else {
		    sgrep_error(this_sgrep,"UTF16 decoding error Got 0xfe without 0xff\n");
		    ch=' ';
		    scanner->parse_errors++;
		    encoder->estate=UTF8_1;
		    i++;
		}
		break;
	    case UTF16_BIG:
		if (i+1>=len) {
		    sgrep_error(this_sgrep,"Odd number of bytes in UTF16-encoded file\n");
		    i++;
		    ch=-1;
		    continue;
		}
		ch=(buf[i]<<8)+buf[i+1];
		i+=2;
		/* sgrep_error(sgrep,"Char %d\n",ch); */
		break;
	    case UTF16_SMALL_START:
		if (buf[i]==0xfe) {
		    encoder->estate=UTF16_SMALL;
		    i++;
		    continue;
		} else {
		    sgrep_error(this_sgrep,"UTF16 decoding error Got 0xff without 0xfe\n");
		    ch=' ';
		    scanner->parse_errors++;
		    encoder->estate=UTF8_1;
		    i++;
		}
		break;
	    case UTF16_SMALL:
		if (i+1>=len) {
		    sgrep_error(this_sgrep,"Odd number of bytes in UTF16-encoded file\n");
		    i++;
		    ch=-1;
		    continue;
		}
		ch=(buf[i+1]<<8)+buf[i];
		i+=2;	
		break;
	    default:
		assert(0 && "Never here");
	    }
	}
	
	switch (state) {

	case SGML_PCDATA:
	    switch (ch) {
	    case '<':
		scanner->tags=encoder->prev;
		state=SGML_STAGO;
		NEXT_CH;
		break;
	    default:
		if (IN_CLIST(scanner->word_chars,ch)) {
		    state=SGML_WORD;
		    scanner->words=encoder->prev;
		    string_clear(scanner->word);
		    TERM_PUSH(scanner->word,'w');
		    TERM_PUSH(scanner->word,ch);
		} else if (ch=='&') {
		    scanner->entitys=encoder->prev;
		    push_state(scanner,SGML_PCDATA_ENTITY);
		    state=SGML_ENTITY_OPEN;
		}
		NEXT_CH;
	    }
	    break;	    

	case SGML_ENTITY_OPEN:
	    scanner->character_reference=0;
	    if (ch=='#') {
		state=SGML_CHARACTER_REFERENCE_OPEN;
		NEXT_CH;
	    } else if (IN_CLIST(scanner->name_start_chars,ch)) {
		string_clear(scanner->name);
		TERM_PUSH(scanner->name,'&');
		TERM_PUSH(scanner->name,ch);
		state=SGML_ENTITY;
		NEXT_CH;
	    } else {
		scanner->entitys=-1;
		state=pop_state(scanner);
	    }
	    break;

	case SGML_ENTITY:
	    if (IN_CLIST(scanner->name_chars,ch)) {
		TERM_PUSH(scanner->name,ch);
		NEXT_CH;
	    } else {
		if (ch==';') {
		    SGML_FOUND(scanner,POS);
		    NEXT_CH;
		} else {
		    SGML_FOUND(scanner,encoder->prev);
		}
		TERM_PUSH(scanner->name,';');
		state=pop_state(scanner);
	    }
	    break;
	    
	case SGML_CHARACTER_REFERENCE_OPEN:
	    if (ch=='x') {
		state=SGML_HEX_CHARACTER_REFERENCE;
		NEXT_CH;
	    } else if (ch>='0' && ch<='9') {
		scanner->character_reference=ch-'0';
		state=SGML_DECIMAL_CHARACTER_REFERENCE;
		NEXT_CH;
	    } else {
		scanner->entitys=-1;
		scanner->character_reference=0;
		scanner->parse_errors++;
		state=pop_state(scanner);
	    }
	    break;
	    
	case SGML_DECIMAL_CHARACTER_REFERENCE:
	    if (ch==';') {
		state=SGML_CHARACTER_REFERENCE_CLOSE;
		NEXT_CH;
		break;
	    } else if (ch>='0' && ch<='9') {
		scanner->character_reference=
		    scanner->character_reference*10+ch-'0';
		NEXT_CH;
	    } else {		
		state=SGML_CHARACTER_REFERENCE_CLOSE;
	    }	    
	    break;

	case SGML_HEX_CHARACTER_REFERENCE:
	    if (ch==';') {
		state=SGML_CHARACTER_REFERENCE_CLOSE;
		NEXT_CH;
	    } else if (ch>='0' && ch<='9') {
		scanner->character_reference=
		    scanner->character_reference*16+ch-'0';
		NEXT_CH;
	    } else  if (toupper(ch)>='A' && toupper(ch)<='F') {
		scanner->character_reference=scanner->character_reference*16+
		    toupper(ch)-'A'+10;
		NEXT_CH;
	    } else {
		state=SGML_CHARACTER_REFERENCE_CLOSE;
	    }
	    break;
		
	case SGML_CHARACTER_REFERENCE_CLOSE: {
	    char tmp[30];
	    sprintf(tmp,"&#x%x;",scanner->character_reference);
	    string_clear(scanner->name);
	    string_cat(scanner->name,tmp);
	    /* fprintf(stderr,"charref: %s\n",scanner->name); */
	    state=pop_state(scanner);
	    break;
	}

	case SGML_PCDATA_ENTITY:
	    if (scanner->character_reference>0 && 
		IN_CLIST(scanner->word_chars,scanner->character_reference)) {
		/* Entity was a character entity and was word character */
		scanner->words=scanner->entitys;
		string_clear(scanner->word);
		TERM_PUSH(scanner->word,'w');
		TERM_PUSH(scanner->word,scanner->character_reference);
		state=SGML_WORD;
	    } else if (scanner->entitys>=0 && 
		       IN_CLIST(scanner->word_chars,ch)) {
		/* Handles the case when word starts with some entity */
		scanner->words=scanner->entitys;
		string_clear(scanner->word);
		TERM_PUSH(scanner->word,'w');
		string_cat(scanner->word,string_to_char(scanner->name));
		state=SGML_WORD;
	    } else {
		state=SGML_PCDATA;
	    }
	    break;


	case SGML_WORD_ENTITY:
	    if (scanner->character_reference>0 && 
		IN_CLIST(scanner->word_chars,scanner->character_reference)) {
		/* Entity was a character entity and was word character */
		TERM_PUSH(scanner->word,scanner->character_reference);
		state=SGML_WORD;
	    } else {
		/* Handles the case when word continues with some entity */
		if (scanner->entitys>=0 && scanner->character_reference==0) {
		    /* Use the located entity only if it vas valid */
		    string_cat(scanner->word,string_to_char(scanner->name));
		    state=SGML_WORD;
		} else {
		    /* Entity ended the word: either in syntax error
		     * or non word char */
		    SGML_FOUND(scanner,scanner->word_end);
		    state=SGML_PCDATA;
		}
	    }
	    break;
	    
	case SGML_WORD:
	    if (IN_CLIST(scanner->word_chars,ch) && ch!='<') {
		TERM_PUSH(scanner->word,ch);
		NEXT_CH;
	    } else if (ch=='&') {
		scanner->word_end=encoder->prev;
		scanner->entitys=encoder->prev;
		push_state(scanner,SGML_WORD_ENTITY);
		state=SGML_ENTITY_OPEN;
		NEXT_CH;
	    } else {
		SGML_FOUND(scanner,encoder->prev);
		state=SGML_PCDATA;
	    }
	    break;

	case SGML_STAGO:
	    switch(ch) {
	    case '/':
		string_clear(scanner->gi);
		TERM_PUSH(scanner->gi,'e');
		state=SGML_END_TAG;
		NEXT_CH;
		break;
	    case '!':
		state=SGML_DECLARATION_START;
		NEXT_CH;
		break;
	    case '?':
		string_truncate(scanner->pi,1);
		state=SGML_PI;
		push_state(scanner,SGML_PCDATA);
		NEXT_CH;
		break;
	    default:
		if (IN_CLIST(scanner->name_start_chars,ch)) {
		    state=SGML_GI;
		    string_clear(scanner->gi);
		    TERM_PUSH(scanner->gi,'s');
		    TERM_PUSH(scanner->gi,ch);
		    NEXT_CH;
		} else {
		    state=SGML_PCDATA;
		}
		break;
	    }
	    break;

	case SGML_GI:
	    if (IN_CLIST(scanner->name_chars,ch)) {
		TERM_PUSH(scanner->gi,ch);
		NEXT_CH;
	    } else {
		state=SGML_W_ATTNAME;
	    }
	    break;

	case SGML_W_ATTNAME:
	    if (ch=='>') {
		state=SGML_STAGC;
	    } else if (IN_CLIST(scanner->name_start_chars,ch)) {
		string_truncate(scanner->aname,1);
		TERM_PUSH(scanner->aname,ch);
		scanner->anames=encoder->prev;
		state=SGML_ATTNAME;
		NEXT_CH;
	    } else {
		NEXT_CH;
	    }	    
	    break;

	case SGML_ATTNAME:
	    if (IN_CLIST(scanner->name_chars,ch)) {
		TERM_PUSH(scanner->aname,ch);
		NEXT_CH;
	    } else {
		state=SGML_W_ATTEQUAL;
	    }
	    break;

	case SGML_W_ATTEQUAL:
	    switch(ch) {
	    case ' ': case '\t':  case '\n':  case '\r': case '\v':
		NEXT_CH;
		break;
	    case '=':
		state=SGML_W_ATTVALUE;
		NEXT_CH;
		break;
		
	    default:
		if (ch=='>' || IN_CLIST(scanner->name_start_chars,ch)) {
		    /* FIXME: handle attribute value with no name */
		    /* fprintf(stderr,"Attribute with no value\n"); */
		}
		if (ch=='>') {
		    state=SGML_STAGC;
		} else if (IN_CLIST(scanner->name_start_chars,ch)) {
		    state=SGML_W_ATTNAME;
		} else {
		    /* Parse error (unexpected character) */
		    scanner->parse_errors++;
		    state=SGML_W_ATTNAME;
		}
		break;
	    }
	    break;

	case SGML_W_ATTVALUE:
	    switch(ch) {
	    case ' ': case '\t':  case '\n':  case '\r': case '\v':
		NEXT_CH;
		break;
	    case '>':
		/* Parse error.. */
		scanner->parse_errors++;
		state=SGML_STAGC;
		break;
	    case '\"':
		state=SGML_ATTVALUE_DQUOTED;
		scanner->avals=POS;
		string_truncate(scanner->aval,1);
		NEXT_CH;
		break;
	    case '\'':
		state=SGML_ATTVALUE_SQUOTED;
		scanner->avals=POS;
		string_truncate(scanner->aval,1);
		NEXT_CH;
		break;
	    default:
		if (IN_CLIST(scanner->name_chars,ch) || isgraph(ch)) {
		    state=SGML_ATTVALUE;
		    scanner->avals=encoder->prev;
		    string_truncate(scanner->aval,1);
		    string_push(scanner->aval,ch);
		    NEXT_CH;
		} else {
		    /* Parse error */
		    scanner->parse_errors++;
		    state=SGML_W_ATTNAME;
		    break;
		}
		break;
	    }
	    break;
	    
	case SGML_ATTVALUE:
	    if (ch=='>' || isspace(ch) ||
		!(IN_CLIST(scanner->name_chars,ch) || isgraph(ch))) {
		SGML_FOUND(scanner,encoder->prev);
		state=SGML_ATTRIBUTE_END;
		break;
	    }
	    TERM_PUSH(scanner->aval,ch);
	    NEXT_CH;
	    break;

	case SGML_ATTVALUE_DQUOTED:
	    if (ch!='\"') {
		TERM_PUSH(scanner->aval,ch);
		NEXT_CH;
		break;
	    }
	    SGML_FOUND(scanner,encoder->prev);
	    state=SGML_ATTRIBUTE_END;
	    NEXT_CH;
	    break;

	case SGML_ATTVALUE_SQUOTED:
	    if (ch!='\'') {
		TERM_PUSH(scanner->aval,ch);
		NEXT_CH;
		break;
	    }
	    SGML_FOUND(scanner,encoder->prev);
	    state=SGML_ATTRIBUTE_END;
	    NEXT_CH;
	    break;

	case SGML_ATTRIBUTE_END:
	    SGML_FOUND(scanner,encoder->prev);
	    state=SGML_W_ATTNAME;
	    break;   	    

	case SGML_STAGC:
	    SGML_FOUND(scanner,POS);
	    state=SGML_PCDATA;
	    NEXT_CH;
	    break;	    

	case SGML_END_TAG:
	    if (IN_CLIST(scanner->name_chars,ch)) {
		TERM_PUSH(scanner->gi,ch);
		NEXT_CH;
	    } else {
		state=SGML_W_ETAGC;
	    }
	    break;
	    
	case SGML_W_ETAGC:
	    if (ch=='>') {
		SGML_FOUND(scanner,POS);
		state=SGML_PCDATA;
	    }
	    NEXT_CH;
	    break;

	case SGML_DECLARATION_START:
	    switch(ch) {
	    case '-':
		scanner->comments=scanner->tags;
		state=SGML_COMMENT_START;
		push_state(scanner,SGML_PCDATA);
		NEXT_CH;
		break;
	    case '[':
		scanner->markeds=scanner->tags;
		state=SGML_MARKED_SECTION_START;
		string_clear(scanner->gi);
		TERM_PUSH(scanner->gi,'[');
		NEXT_CH;
		break;
	    default:
		if (ch=='D' || ch=='d') {
		    state=SGML_DOCTYPE_DECLARATION;
		    scanner->doctype_declarations=scanner->tags;
		    string_clear(scanner->name);
		    string_push(scanner->name,toupper(ch));
		    NEXT_CH;
		} else {
		    /* Parse error */
		    scanner->parse_errors++;
		    state=SGML_PCDATA;
		}
	    }
	    break;

	case SGML_COMMENT_START:
	    if (ch=='-') {
		state=SGML_COMMENT;
		NEXT_CH;
	    } else {
		scanner->parse_errors++;
		state=pop_state(scanner);
	    }
	    break;
	    
	case SGML_COMMENT:
	    if (ch=='-') {
		state=SGML_COMMENT_END1;
	    } else if (IN_CLIST(scanner->word_chars,ch)) {
		    state=SGML_COMMENT_WORD;
		    string_clear(scanner->comment_word);
		    TERM_PUSH(scanner->comment_word,'c');
		    TERM_PUSH(scanner->comment_word,ch);
		    scanner->comment_words=encoder->prev;
	    }
	    NEXT_CH;
	    break;

	case SGML_COMMENT_WORD:
	    /* FIXME: This does not accept - as word char inside comments */
	    if (IN_CLIST(scanner->word_chars,ch) && ch!='-') {
		TERM_PUSH(scanner->comment_word,ch);
		NEXT_CH;
	    } else {
		SGML_FOUND(scanner,encoder->prev);
		state=SGML_COMMENT;
	    }
	    break;

	case SGML_COMMENT_END1:
	    if (ch=='-') {
		state=SGML_COMMENT_END2;
		NEXT_CH;
	    } else {
		state=SGML_COMMENT;
	    }
	    break;

	    /* FIXME: generalize comment handling to situations
	     * like <!ATTLIST blah --sadfasdf-- blearhj --safsad--> */
	case SGML_COMMENT_END2:
	    if (ch=='>') {
		SGML_FOUND(scanner,POS);
		state=pop_state(scanner);
	    } else if (ch=='-') {
		state=SGML_COMMENT_START;
	    } else if (!isspace(ch)) {
		/* Since everything else except white space is
		 * a parse error */                 		   
		scanner->parse_errors++;
	    }
	    NEXT_CH;
	    break;

	case SGML_PI:
	    if (ch=='?' && scanner->type==XML_SCANNER) {
		state=SGML_PI_END;
	    } else if (ch=='>' && scanner->type!=XML_SCANNER) {
		SGML_FOUND(scanner,POS);
		state=pop_state(scanner);
	    } else {
		TERM_PUSH(scanner->pi,ch);
	    }
	    NEXT_CH;
	    break;

	case SGML_PI_END:
	    switch(ch) {
	    case '?':
		TERM_PUSH(scanner->pi,'?');
		break;
	    case '>':
		SGML_FOUND(scanner,POS);
		state=pop_state(scanner);
		break;
	    default:
		TERM_PUSH(scanner->pi,'?');
		TERM_PUSH(scanner->pi,ch);
		state=SGML_PI;
	    }
	    NEXT_CH;
	    break;

	case SGML_MARKED_SECTION_START:
	    /* Using gi also for marked section names. */
	    /* NOTE: entity references in marked section type are allowed
	     * to enable the SGML IGNORE and PCDATA entity tricks */
	    /* FIXME: this probably won't work (maybe not a big deal) */
	    if ( (string_len(scanner->gi)==1 &&
		  (IN_CLIST(scanner->name_start_chars,ch) || ch=='&' || ch==';')) ||
		 (string_len(scanner->gi)>1 &&
		  (IN_CLIST(scanner->name_chars,ch) || ch=='&' || ch==';')) ) {
		TERM_PUSH(scanner->gi,ch);
		NEXT_CH;
	    } else {
		state=SGML_MARKED_SECTION_START2;
	    }
	    break;

	case SGML_MARKED_SECTION_START2:
	    if (isspace(ch)) {
		NEXT_CH;
	    } else if (ch=='['){
		if (strcmp(string_to_char(scanner->gi),"[CDATA")==0) {
		    state=SGML_CDATA_MARKED_SECTION;
		} else {
		    /* Since extracting other than CDATA marked sections
		     * would need a stack this only reports the start of
		     * those sections. Maybe in version 3..*/
		    SGML_FOUND(scanner,POS);
		    state=SGML_PCDATA;
		}
		NEXT_CH;
	    } else {
		/* Parse error */
		scanner->parse_errors++;
		state=SGML_PCDATA;
	    }
	    break;
	    
	case SGML_CDATA_MARKED_SECTION:
	    if (ch==']') {
		state=SGML_CDATA_MARKED_SECTION_END1;
	    } else if (IN_CLIST(scanner->word_chars,ch)) {
		string_clear(scanner->word);
		TERM_PUSH(scanner->word,'w');
		TERM_PUSH(scanner->word,ch);
		scanner->words=encoder->prev;
		state=SGML_CDATA_MARKED_SECTION_WORD;
	    }
	    NEXT_CH;
	    break;

	case SGML_CDATA_MARKED_SECTION_WORD:
	    if (IN_CLIST(scanner->word_chars,ch) && ch!=']') {
		TERM_PUSH(scanner->word,ch);
		NEXT_CH;
	    } else {
		SGML_FOUND(scanner,encoder->prev);
		state=SGML_CDATA_MARKED_SECTION;
	    }
	    break;

	case SGML_CDATA_MARKED_SECTION_END1:
	    if (ch==']') {
		state=SGML_CDATA_MARKED_SECTION_END2;
		NEXT_CH;
	    } else {
		state=SGML_CDATA_MARKED_SECTION;
	    }
	    break;

	case SGML_CDATA_MARKED_SECTION_END2:
	    if (ch=='>') {
		SGML_FOUND(scanner,POS);
		NEXT_CH;
		state=SGML_PCDATA;
	    } else if (ch==']') {
		NEXT_CH;
	    } else {
		state=SGML_CDATA_MARKED_SECTION;
	    }
	    break;

	case SGML_DOCTYPE_DECLARATION: 
	    if (string_len(scanner->name)<7 
		&& toupper(ch)==("DOCTYPE")[string_len(scanner->name)]) {
	        TERM_PUSH(scanner->name,toupper(ch));
		NEXT_CH;
	    } else if (string_len(scanner->name)==7 && isspace(ch)) {
		string_clear(scanner->name);
		TERM_PUSH(scanner->name,'d');
		TERM_PUSH(scanner->name,'n');
		state=SGML_DOCTYPE;
		NEXT_CH;
	    } else {
		scanner->parse_errors++;
		state=SGML_PCDATA;
	    }
	    break;

	case SGML_DOCTYPE:
	    if (string_len(scanner->name)==2) {
		if (IN_CLIST(scanner->name_start_chars,ch)) {
		    TERM_PUSH(scanner->name,ch);
		    scanner->doctypes=encoder->prev;
		    NEXT_CH;
		} else if (!isspace(ch)) {
		    scanner->parse_errors++;
		    state=SGML_PCDATA;
		} else {
		    NEXT_CH;
		}
	    } else {
		if (IN_CLIST(scanner->name_chars,ch)) {
		    TERM_PUSH(scanner->name,ch);
		    NEXT_CH;
		} else {
		    SGML_FOUND(scanner,encoder->prev);
		    state=SGML_DOCTYPE_EXTERNAL;
		}
	    }
	    break;

	case SGML_DOCTYPE_EXTERNAL:
	    switch(ch) {
	    case '[':
		state=SGML_DOCTYPE_INTERNAL;
		NEXT_CH;
		break;
	    case 'P': case 'p':
		/* _P_ublic */
		scanner->publici=1;
		state=SGML_DOCTYPE_PUBLIC;
		NEXT_CH;
		break;
	    case 'S': case 's':
		scanner->systemi=1;
		state=SGML_DOCTYPE_SYSTEM;
		NEXT_CH;
		break;
	    case '>':
		state=SGML_DOCTYPE_END;
		break;
	    default:
		if (!isspace(ch)) {
		    scanner->parse_errors++;
		    state=SGML_PCDATA;
		}
		NEXT_CH;
	    }
	    break;
		
	case SGML_DOCTYPE_PUBLIC:
	    if (("PUBLIC")[scanner->publici]==toupper(ch)) {
		scanner->publici++;
		if (scanner->publici==6) {
		    state=SGML_DOCTYPE_PUBLIC_ID_START;
		}
		NEXT_CH;
	    } else {
		scanner->parse_errors++;
		state=SGML_PCDATA;
	    }
	    break;

	case SGML_DOCTYPE_SYSTEM:
	    if (("SYSTEM")[scanner->systemi]==toupper(ch)) {
		scanner->systemi++;
		if (scanner->systemi==6) {
		    state=SGML_DOCTYPE_SYSTEM_ID_START;
		}
		NEXT_CH;
	    } else {
		scanner->parse_errors++;
		state=SGML_PCDATA;
	    }
	    break;

	case SGML_DOCTYPE_PUBLIC_ID_START:
	    if (ch=='"' || ch=='\'') {
		state=SGML_LITERAL_START;
		push_state(scanner,SGML_DOCTYPE_PUBLIC_ID);
	    } else if (isspace(ch)) {
		NEXT_CH;
	    } else {
		scanner->parse_errors++;
		state=SGML_PCDATA;	
	    }
	    break;

	case SGML_WAITING_LITERAL:
	    if (isspace(ch)) {
		NEXT_CH;
	    } else {
		if (ch=='"' || ch=='\'') {
		    state=SGML_LITERAL_START;
		} else {
		    scanner->parse_errors++;
		    scanner->literals=-1;
		    state=pop_state(scanner);
		}
	    }
	    break;

	case SGML_LITERAL_START:
	    scanner->literals=start+i;
	    string_truncate(scanner->literal,3);
	    state=(ch=='"') ? SGML_LITERAL_DQUOTED : SGML_LITERAL_SQUOTED;
	    NEXT_CH;
	    break;

	case SGML_LITERAL_DQUOTED:
	    if (ch!='"') {
		TERM_PUSH(scanner->literal,ch);
		NEXT_CH;
	    } else {
		state=pop_state(scanner);
	    }
	    break;

	case SGML_LITERAL_SQUOTED:
	    if (ch!='\'') {
		TERM_PUSH(scanner->literal,ch);
		NEXT_CH;
	    } else {
		state=pop_state(scanner);
	    }
	    break;
	    
	case SGML_DOCTYPE_PUBLIC_ID:
	    SGML_FOUND(scanner,encoder->prev);
	    state=SGML_DOCTYPE_SYSTEM_ID_START;
	    NEXT_CH;
	    break;

	case SGML_DOCTYPE_SYSTEM_ID_START:
	    if (ch=='[') {
		state=SGML_DOCTYPE_INTERNAL;
	    } else if (ch=='"' || ch=='\'') {
		state=SGML_LITERAL_START;
		push_state(scanner,SGML_DOCTYPE_SYSTEM_ID);
	    } else if (isspace(ch)) {
		NEXT_CH;
	    } else {
		scanner->parse_errors++;
		state=SGML_PCDATA;	
	    }
	    break;

	case SGML_DOCTYPE_SYSTEM_ID:
	    SGML_FOUND(scanner,encoder->prev);
	    state=SGML_DOCTYPE_INTERNAL_START;
	    NEXT_CH;
	    break;
	    
	case SGML_DOCTYPE_INTERNAL_START:
	    switch(ch) {
	    case '[':
		state=SGML_DOCTYPE_INTERNAL;
		NEXT_CH;
		break;
	    case '>':
		state=SGML_DOCTYPE_END;
		break;
	    default:
		if (isspace(ch)) {
		    NEXT_CH;
		} else {
		    scanner->parse_errors++;
		    state=SGML_PCDATA;
		}
		break;
	    }
	    break;		    

	case SGML_DOCTYPE_INTERNAL:
	    switch(ch) {
	    case '<':
		scanner->internal_declarations=encoder->prev;
		state=SGML_INTERNAL_DECLARATION_START1;
		break;
	    case '%':
		/* sgrep_error(sgrep,"PER\n"); */
		scanner->entitys=encoder->prev;
		state=SGML_PEREFERENCE;
		NEXT_CH;
		break;
	    case ']':
		state=SGML_DOCTYPE_END;
		break;
	    default:
		if (!isspace(ch)) {
		scanner->parse_errors++;
		}
		break;
	    }
	    NEXT_CH;
	    break;

	case SGML_PEREFERENCE:
	    if (IN_CLIST(scanner->name_chars,ch)) {
		/* Nothing */
	    } else if (ch==';') {
		state=SGML_DOCTYPE_INTERNAL;
	    } else if (isspace(ch)) {
		state=SGML_DOCTYPE_INTERNAL;
	    } else {
		scanner->parse_errors++;
		state=SGML_DOCTYPE_INTERNAL;
	    }
	    NEXT_CH;
	    break;
		
	case SGML_INTERNAL_DECLARATION_START1:
	    switch (ch) {
	    case '!':
		state=SGML_INTERNAL_DECLARATION_START2;
		NEXT_CH;
		break;
	    case '?':
		scanner->tags=scanner->internal_declarations;
		string_truncate(scanner->pi,1);
		state=SGML_PI;
		push_state(scanner,SGML_DOCTYPE_INTERNAL);
		NEXT_CH;
		break;
	    default:
		scanner->parse_errors++;
		state=SGML_DOCTYPE_INTERNAL;
		break;
	    }
	    break;
	    
	case SGML_INTERNAL_DECLARATION_START2:
	    if (ch=='-') {
		scanner->comments=scanner->internal_declarations;
		push_state(scanner,SGML_DOCTYPE_INTERNAL);
		state=SGML_COMMENT_START;
		NEXT_CH;
	    } else if (isalpha(ch)) {
  		state=SGML_INTERNAL_DECLARATION_NAME;
		string_clear(scanner->name);
		TERM_PUSH(scanner->name,toupper(ch));
		NEXT_CH;
	    } else {
		scanner->parse_errors++;
		state=SGML_DOCTYPE_INTERNAL;
	    }
	    break;
		
	case SGML_INTERNAL_DECLARATION_NAME: 
	    if (isalpha(ch) &&
		string_len(scanner->name)<10) {
		TERM_PUSH(scanner->name,ch);
		NEXT_CH;
	    } else if (isspace(ch)) {
		const char *name;
		string_toupper(scanner->name,0);
		name=string_to_char(scanner->name);
		if (strcmp(name,"ENTITY")==0) {
		    scanner->entity_has_systemid=0;
		    scanner->entity_is_ndata=0;
		    state=SGML_ENTITY_DECLARATION;
		} else if (strcmp(name,"ELEMENT")==0) {
		    state=SGML_ELEMENT_TYPE_DECLARATION;
		} else if (strcmp(name,"NOTATION")==0) {
		    state=SGML_NOTATION_DECLARATION;
		} else if (strcmp(name,"ATTLIST")==0) {
		    state=SGML_ATTLIST_DECLARATION;
		} else {
		    scanner->parse_errors++;
		    state=SGML_DOCTYPE_INTERNAL;
		}
		NEXT_CH;
	    } else {
		scanner->parse_errors++;
		state=SGML_DOCTYPE_INTERNAL;
	    }
	    break;

	case SGML_ENTITY_DECLARATION:
	    if (ch=='%') {
		/* Parameter entities are handled just like any other
		 * entities */
		NEXT_CH;
	    } else if (IN_CLIST(scanner->name_start_chars,ch)) {
		state=SGML_ENTITY_DECLARATION_NAME;
		string_clear(scanner->name);
		string_cat(scanner->name,"!ed");
		string_push(scanner->name,ch);
		NEXT_CH;
	    } else if (isspace(ch)) {
		NEXT_CH;
	    } else {
		scanner->parse_errors++;
		state=SGML_DOCTYPE_INTERNAL;
	    }
	    break;

	case SGML_ENTITY_DECLARATION_NAME:
	    if (IN_CLIST(scanner->name_chars,ch)) {
		TERM_PUSH(scanner->name,ch);
		NEXT_CH;
	    } else {
		state=SGML_ENTITY_DEFINITION;
	    }
	    break;
	    
	case SGML_ENTITY_DEFINITION:
	    if (ch=='"') {
		state=SGML_GENERAL_ENTITY_DEFINITION_DQUOTED;
		scanner->literals=POS;
		NEXT_CH;
	    } else if (ch=='\'') {
		state=SGML_GENERAL_ENTITY_DEFINITION_SQUOTED;
		scanner->literals=POS;
		NEXT_CH;
	    } else if (isspace(ch)) {
		NEXT_CH;
	    } else if (isalpha(ch)) {
		push_state(scanner,SGML_ENTITY_DEFINITION_TYPE);
		string_clear(scanner->name2);
		state=SGML_RESERVED_WORD;
	    } else {
		scanner->parse_errors++;
		state=SGML_DOCTYPE_INTERNAL;
	    }
	    break;

	case SGML_RESERVED_WORD:
	    if (isalpha(ch)) {
		TERM_PUSH(scanner->name2,toupper(ch));
		NEXT_CH;
	    } else {
		state=pop_state(scanner);
	    }
	    break;

	case SGML_ENTITY_DEFINITION_TYPE:
	    if (strcmp(string_to_char(scanner->name2),"SYSTEM")==0) {
		push_state(scanner,SGML_ENTITY_DEFINITION_SYSTEM_ID);
		state=SGML_WAITING_LITERAL;
	    } else if (strcmp(string_to_char(scanner->name2),"PUBLIC")==0) {
		push_state(scanner,SGML_ENTITY_DEFINITION_PUBLIC_ID);
		state=SGML_WAITING_LITERAL;
	    } else if (strcmp(string_to_char(scanner->name2),"CDATA")==0 ||
		       strcmp(string_to_char(scanner->name2),"SDATA")==0) {
		state=SGML_LITERAL_ENTITY;
	    } else {
		scanner->parse_errors++;
		state=SGML_DOCTYPE_INTERNAL;
	    }
	    break;
		    
	case SGML_LITERAL_ENTITY:
	    if (ch=='"') {
		state=SGML_GENERAL_ENTITY_DEFINITION_DQUOTED;
		scanner->literals=POS;
		NEXT_CH;
	    } else if (ch=='\'') {
		state=SGML_GENERAL_ENTITY_DEFINITION_SQUOTED;
		scanner->literals=POS;
		NEXT_CH;
	    } else if (isspace(ch)) {
		NEXT_CH;
	    } else {
		scanner->parse_errors++;
	    }
	    break;
		
		
	case SGML_GENERAL_ENTITY_DEFINITION_DQUOTED:
	    if (ch=='"') {
		SGML_FOUND(scanner,encoder->prev);
		state=SGML_ENTITY_DEFINITION_END;
	    }
	    NEXT_CH;
	    break;
		    
	case SGML_GENERAL_ENTITY_DEFINITION_SQUOTED:
	    if (ch=='\'') {	
		SGML_FOUND(scanner,encoder->prev);
		state=SGML_ENTITY_DEFINITION_END;
	    }
	    NEXT_CH;
	    break;
	
	case SGML_ENTITY_DEFINITION_END:
	    if (ch=='>') {
		SGML_FOUND(scanner,POS);
		state=SGML_DOCTYPE_INTERNAL;
		NEXT_CH;
		break;
	    } else if (isspace(ch)) {
		NEXT_CH;
	    } else {
		scanner->parse_errors++;
		state=SGML_DOCTYPE_INTERNAL;
	    }
	    break;
	    
	case SGML_ENTITY_DEFINITION_PUBLIC_ID:
	    if (scanner->literals>0) {
		SGML_FOUND(scanner,encoder->prev);
		state=SGML_WAITING_ENTITY_DEFINITION_SYSTEM_ID;
		NEXT_CH;
	    } else {
		state=SGML_DOCTYPE_INTERNAL;
	    }
	    break;
	    
	case SGML_WAITING_ENTITY_DEFINITION_SYSTEM_ID:
	    switch(ch) {
	    case '"':
	    case '\'':
	        push_state(scanner,SGML_ENTITY_DEFINITION_SYSTEM_ID);
		state=SGML_LITERAL_START;
		break;
	    default:
		if (isspace(ch)) {
		    NEXT_CH;
		} else {
		    state=SGML_ENTITY_DEFINITION_NDATA;
		}
	    }
	    break;
	    
	case SGML_ENTITY_DEFINITION_SYSTEM_ID:
	    if (scanner->literals>0) {
		SGML_FOUND(scanner,encoder->prev);
		scanner->entity_has_systemid=1;
		state=SGML_ENTITY_DEFINITION_NDATA;
		NEXT_CH;
	    }
	    break;
	    
	case SGML_ENTITY_DEFINITION_NDATA:
	    if (ch=='>') {
		state=SGML_ENTITY_DEFINITION_END;
	    } else if (isalpha(ch)) {
		string_clear(scanner->name2);
		push_state(scanner,SGML_ENTITY_DEFINITION_NDATA2);
		state=SGML_RESERVED_WORD;
	    } else if (isspace(ch)) {
		NEXT_CH;
	    } else {
		scanner->parse_errors++;
		state=SGML_DOCTYPE_INTERNAL;
	    }
	    break;

	case SGML_ENTITY_DEFINITION_NDATA2:
	    if (strcmp(string_to_char(scanner->name2),"NDATA")==0) {
		scanner->entity_is_ndata=1;
		if (isspace(ch)) {
		    NEXT_CH;
		} else if (IN_CLIST(scanner->name_start_chars,ch)){
		    string_clear(scanner->name2);
		    string_cat(scanner->name2,"!en");
		    scanner->name2s=encoder->prev;
		    state=SGML_ENTITY_DEFINITION_NDATA_NAME;
		} else {
		    scanner->parse_errors++;
		    state=SGML_DOCTYPE_INTERNAL;
		}
	    } else {
		scanner->parse_errors++;
		state=SGML_DOCTYPE_INTERNAL;
	    }
	    break;

	case SGML_ENTITY_DEFINITION_NDATA_NAME:
	    if (IN_CLIST(scanner->name_chars,ch)) {
		TERM_PUSH(scanner->name2,ch);
		NEXT_CH;
	    } else {
		SGML_FOUND(scanner,encoder->prev);		
		state=SGML_ENTITY_DEFINITION_END;
	    }
	    break;

	    /* FIXME: could be useful */
	case SGML_ELEMENT_TYPE_DECLARATION:
	    if (ch=='>') {
		SGML_FOUND(scanner,POS);
		state=SGML_DOCTYPE_INTERNAL;
	    }
	    NEXT_CH;
	    break;

	    /* FIXME: could be useful */
	case SGML_ATTLIST_DECLARATION:
	    if (ch=='>') {
		SGML_FOUND(scanner,POS);
		state=SGML_DOCTYPE_INTERNAL;
	    }
	    NEXT_CH;
	    break;
	    
	    /* FIXME: could be useful */
	case SGML_NOTATION_DECLARATION:
	    if (ch=='>') {
		SGML_FOUND(scanner,POS);
		state=SGML_DOCTYPE_INTERNAL;
	    }
	    NEXT_CH;
	    break;
	

	case SGML_DOCTYPE_END:
	    if (ch=='>') {
		SGML_FOUND(scanner,POS);
		state=SGML_PCDATA;
		NEXT_CH;
	    } else if (isspace(ch)) {
		NEXT_CH;
	    } else {
		scanner->parse_errors++;
		state=SGML_PCDATA;
	    }
	    break;

	default:
	    sgrep_error(scanner->sgrep,
			"SGML-scanner in unimplemented state. Switching to PCDATA\n");
	    state=SGML_PCDATA;
	    break;
	}
	

    }
    scanner->state=state;
    return SGREP_OK;
#undef NEXT_CH
}

void pop_elements_to(SGMLScanner *state,ElementStack *p) {
    ElementStack *q;
    SGREPDATA(state);
    assert(p==NULL || state->top);
    /* Element is in the stack */
    q=state->top;
    while(p!=q) {
	/* All elements in the stack not having an end tag
	 * are considered as empty. Sad but true */
	state->top=q->prev;
	SGML_ENTRY("elements","","@elements",q->start,q->end, this_sgrep);
	/* fprintf(stderr,"<%s/>\n",q->gi); */
	ana_free(&(this_sgrep->this_book->anastasia_memory), q->gi);
	ana_free(&(this_sgrep->this_book->anastasia_memory), q);
	q=state->top;
    }
}

void sgml_add_entry_to_index(SGMLScanner *state, const char *phrase, int start, int end, struct SgrepStruct *this_sgrep) {
    if (phrase[0]=='@') {
	add_region(state->element_list,start,end);
    } else {
	if (add_region_to_index((struct IndexWriterStruct *)state->data,
				phrase,start,end)==SGREP_ERROR) {
	    state->failed=1;
	}
    }
}

void push_state(SGMLScanner *scanner, enum SGMLState state) {
    assert(scanner->state_stack_ptr<SGML_SCANNER_STACK_SIZE);
    scanner->state_stack[scanner->state_stack_ptr++]=state;
}

enum SGMLState pop_state(SGMLScanner *scanner) {
    assert(scanner->state_stack_ptr>0);
    return scanner->state_stack[--scanner->state_stack_ptr];
}

void sgml_found(SGMLScanner *state,enum SGMLState s,int end_index) {
    SGREPDATA(state);
    end_index--;

    switch(s) {
    case SGML_WORD:
    case SGML_WORD_ENTITY:
    case SGML_CDATA_MARKED_SECTION_WORD:
	assert(string_to_char(state->word)[0]=='w');
	if (state->ignore_case) {
	    string_tolower(state->word,1);
	}
	SGML_ENTRY("word",
		   string_escaped(state->word)+1,
		   string_to_char(state->word),
		   state->words,end_index, this_sgrep);
	break;
	
    case SGML_ENTITY:
	assert(string_to_char(state->name)[0]=='&');
	SGML_ENTRY("entity",
		   string_escaped(state->name)+1,
		   string_to_char(state->name),
		   state->entitys,end_index, this_sgrep);
	break;

    case SGML_STAGC:
	if (state->type!=XML_SCANNER) {
	    string_toupper(state->gi,1);
	}
	SGML_ENTRY("stag",
		   string_escaped(state->gi)+1,
		   string_to_char(state->gi),
		   state->tags,end_index, this_sgrep);
	if (state->maintain_element_stack) {
	    /* Push to element stack */
            ElementStack *e= (struct ElementStackStruct *) ana_malloc(&(this_sgrep->this_book->anastasia_memory), sizeof(ElementStack), __FILE__, __LINE__);
 	    e->gi= (char *) apr_pstrdup(this_sgrep->current_pool, string_to_char(state->gi)+1);
	    e->start=state->tags;
	    e->end=end_index;
	    e->prev=state->top;
	    state->top=e;
	}
	break;

    case SGML_ATTVALUE:
    case SGML_ATTVALUE_DQUOTED:
    case SGML_ATTVALUE_SQUOTED:
	SGML_ENTRY("attvalue",
		   string_escaped(state->aval)+1,
		   string_to_char(state->aval),
		   state->avals,end_index, this_sgrep);
	break;

    case SGML_ATTRIBUTE_END:
	if (state->type!=XML_SCANNER) {
	    string_toupper(state->aname,1);
	}
	SGML_ENTRY("attribute",
		   string_escaped(state->aname)+1,
		   string_to_char(state->aname),
		   state->anames,end_index, this_sgrep);
	break;
	
    case SGML_W_ETAGC:
	if (state->type!=XML_SCANNER) {
	    string_toupper(state->gi,1);
	}
	SGML_ENTRY("etag",
		   string_escaped(state->gi)+1,
		   string_to_char(state->gi),
		   state->tags,end_index, this_sgrep);
	if (state->maintain_element_stack) {
	    /* First check that the element is on the stack */
	    ElementStack *p=state->top;
	    while(p && strcmp(string_to_char(state->gi)+1,p->gi)!=0) {
		p=p->prev;
	    }
	    if (p) {
		/* Take elements until p is in top */
		pop_elements_to(state,p);
		/* Pop p */
		state->top=p->prev;
		SGML_ENTRY("elements","","@elements",p->start,end_index, this_sgrep);
		/* fprintf(stderr,"<%s>..</%s>\n",p->gi,p->gi);*/
		ana_free(&(this_sgrep->this_book->anastasia_memory), p->gi);
		ana_free(&(this_sgrep->this_book->anastasia_memory), p);
	    }
	}
	break;

    case SGML_COMMENT_WORD:
	if (state->ignore_case) {
	    string_tolower(state->comment_word,1);
	}
	SGML_ENTRY("comment_word",
		   string_escaped(state->comment_word)+1,
		   string_to_char(state->comment_word),
		   state->comment_words,end_index, this_sgrep);
	break;
	   
    case SGML_COMMENT_END2:
	SGML_ENTRY("comment",(const unsigned char *)"",
		   (const unsigned char *)"-",
		   state->comments,end_index, this_sgrep);
	break;

    case SGML_PI:
    case SGML_PI_END:
	SGML_ENTRY("pi",
		   string_escaped(state->pi)+1,
		   string_to_char(state->pi),
		   state->tags,end_index, this_sgrep);
	if (state->type==XML_SCANNER &&
	    toupper(string_to_char(state->pi)[1])=='X' &&
	    toupper(string_to_char(state->pi)[2])=='M' &&
	    toupper(string_to_char(state->pi)[3])=='L') {
	    parse_xml_declaration(state);
	}
	break;

    case SGML_CDATA_MARKED_SECTION_END2:
	SGML_ENTRY("cdata",(const unsigned char *)"",
		   (const unsigned char *)"[CDATA",
		   state->markeds,end_index, this_sgrep);
	break;

    case SGML_DOCTYPE:
	assert(string_to_char(state->name)[0]=='d' &&
	       string_to_char(state->name)[1]=='n');
	if (state->type!=XML_SCANNER) {
	    string_toupper(state->name,2);
	}
	SGML_ENTRY("doctype",
		   string_escaped(state->name)+2,
		   string_to_char(state->name),
		   state->doctypes, end_index, this_sgrep);
	/* Empty the element stack */
	pop_elements_to(state,NULL);
	break;

    case SGML_DOCTYPE_PUBLIC_ID:
	state->literal->s[1]='d';
	state->literal->s[2]='p';
	SGML_ENTRY("doctype_pid",
		   string_escaped(state->literal)+3,
		   string_to_char(state->literal)+1,
		   state->literals,end_index, this_sgrep);
	break;
	
    case SGML_DOCTYPE_SYSTEM_ID:
	state->literal->s[1]='d';
	state->literal->s[2]='s';
	SGML_ENTRY("doctype_sid",
		   string_escaped(state->literal)+3,
		   string_to_char(state->literal)+1,
		   state->literals,end_index, this_sgrep);
	break;

    case SGML_DOCTYPE_END:
	SGML_ENTRY("prologs",(const unsigned char *)"d",
		   (const unsigned char *)"d!",
		   state->doctype_declarations, end_index, this_sgrep);
	break;

	/* FIXME: add the literal instead of it's name */
    case SGML_GENERAL_ENTITY_DEFINITION_DQUOTED:
    case SGML_GENERAL_ENTITY_DEFINITION_SQUOTED:
	assert(string_to_char(state->name)[0]=='!' &&
	       string_to_char(state->name)[1]=='e');
	state->name->s[2]='l';
	SGML_ENTRY("entity_literal",
		   string_escaped(state->name)+3,
		   string_to_char(state->name),
		   state->literals,end_index, this_sgrep);
	break;

    case SGML_ENTITY_DEFINITION_END:
	assert(string_to_char(state->name)[0]=='!' &&
	       string_to_char(state->name)[1]=='e');
	state->name->s[2]='d';
	SGML_ENTRY("entity_declaration",
		   string_escaped(state->name)+3,
		   string_to_char(state->name),
		   state->internal_declarations,end_index, this_sgrep);
	/* Check if this is an system entity reference we should include */
	if (state->entity_has_systemid && 
	    (!state->entity_is_ndata) &&
	    state->include_system_entities) {
	    const char *url=string_to_char(state->literal)+3;
	    if (!flist_exists(state->file_list, url)) {
	    }
	}
	break;

    case SGML_ENTITY_DEFINITION_PUBLIC_ID:
	state->literal->s[0]='!';
	state->literal->s[1]='e';
	state->literal->s[2]='p';
	SGML_ENTRY("entity_pid",
		   string_escaped(state->literal)+3,
		   string_to_char(state->literal),
		   state->literals,end_index, this_sgrep);
	break;

    case SGML_ENTITY_DEFINITION_SYSTEM_ID:
	state->literal->s[0]='!';
	state->literal->s[1]='e';
	state->literal->s[2]='s';
	SGML_ENTRY("entity_sid",
		   string_escaped(state->literal)+3,
		   string_to_char(state->literal),
		   state->literals,end_index, this_sgrep);
	break;

    case SGML_ENTITY_DEFINITION_NDATA_NAME:
	if (state->type!=XML_SCANNER) {
	    string_toupper(state->name2,3);
	}
	SGML_ENTRY("entity_ndata",
		   string_escaped(state->name2)+3,
		   string_to_char(state->name2),
		   state->name2s,end_index, this_sgrep);
	break;

    case SGML_ELEMENT_TYPE_DECLARATION:
    case SGML_ATTLIST_DECLARATION:
    case SGML_NOTATION_DECLARATION:
	/* Not used */
	break;
	
    default:
	sgrep_error(this_sgrep,"SGML huh?\n");
	break;
    }
}

/*
 * If you think that this function is dull to read, I can assure that is was
 * even duller to write
 */
void parse_xml_declaration(SGMLScanner *scanner) {
    SGREPDATA(scanner);
    const char *version="version";
    const char *encoding="encoding";
    const unsigned char *ptr= (const unsigned char *) string_to_char(scanner->pi)+4;
    SgrepString *encoding_name;
    int quote_ch;

    encoding_name=new_string(this_sgrep,MAX_TERM_SIZE);
    /* Whitespace */
    while(*ptr && isspace(*ptr)) ptr++;

    /* "version" */
    while(*ptr && *ptr==*version) {
	ptr++;
	version++;
    }
    if (*version) goto error;
    /* Whitespace */
    while(*ptr && isspace(*ptr)) ptr++;
    /* "=" */
    if (*ptr!='=') goto error;
    ptr++;
    /* Whitespace */
    while(*ptr && isspace(*ptr)) ptr++;
    /* quote */
    if (*ptr!='\'' && *ptr!='\"') goto error;
    quote_ch=*(ptr++);
    /* Ignores version */
    while(*ptr && *ptr!=quote_ch) ptr++;
    if (*ptr!=quote_ch) goto error;
    ptr++;

    /* Whitespace */
    while(*ptr && isspace(*ptr)) ptr++;
    if (!*ptr) {
	delete_string(encoding_name);
	return;
    }

    /* "encoding" */
    while(*ptr && *ptr==*encoding) {
	ptr++;
	encoding++;
    }
    if (*encoding) goto error;
    /* Whitespace */
    while(*ptr && isspace(*ptr)) ptr++;
    /* "=" */
    if (*ptr!='=') goto error;
    ptr++;
    /* Whitespace */
    while(*ptr && isspace(*ptr)) ptr++;

    /* quote */
    if (*ptr!='\'' && *ptr!='\"') goto error;
    quote_ch=*(ptr++);
    
    /* Scan the encoding */
    while(*ptr && *ptr!=quote_ch) {
	string_push(encoding_name,*ptr);
	ptr++;
    }
    if (*ptr!=quote_ch) goto error;
    ptr++;
    /* Ignore the rest of declaration, whatever there is */
    /* If we have been given the encoding, ignore the encoding parameter
     */
    if (this_sgrep->default_encoding==ENCODING_GUESS) {
	string_tolower(encoding_name,0);
	if (strcmp(string_to_char(encoding_name),"iso-8859-1")==0 ||
	    strcmp(string_to_char(encoding_name),"us-ascii")==0) {
	    scanner->encoder.estate=EIGHT_BIT;
	} else if (strcmp(string_to_char(encoding_name),"utf-8")==0) {
	    scanner->encoder.estate=UTF8_1;
	} else if (strcmp(string_to_char(encoding_name),"utf-16")==0) {
	    if (scanner->encoder.estate==UTF8_1 || 
		scanner->encoder.estate==EIGHT_BIT) {
		sgrep_error(this_sgrep,"File '%s': utf-16 encoding given in 8-bit encoding declaration?",
		flist_name(scanner->file_list,scanner->file_num));
	    }
	} else {	
	    sgrep_error(this_sgrep,"File '%s':Unknown encoding '%s'. Using default.\n",
			flist_name(scanner->file_list,scanner->file_num),
			string_to_char(encoding_name));	
	    reset_encoder(scanner,&scanner->encoder);
	}		    
    }
    delete_string(encoding_name);
    return;

 error:
    delete_string(encoding_name);
    scanner->parse_errors++;
    sgrep_error(this_sgrep,"File '%s':Parse error in XML-declaration.\n",
		flist_name(scanner->file_list,scanner->file_num));    
}

void delete_sgml_scanner(SGMLScanner *s) {
 //   SgrepData *this_sgrep=s->sgrep;
    /* Empty the element stack if there is one */
    pop_elements_to(s,NULL);
    if (s->element_list) {
	delete_region_list(s->element_list);
    }
    delete_string(s->word);
    delete_string(s->name2);
    delete_string(s->comment_word);
    delete_string(s->gi);
    delete_string(s->aname);
    delete_string(s->aval);
    delete_string(s->name);
    delete_string(s->literal);
    delete_string(s->pi);
    if (s->name_start_chars) ana_free(&(s->sgrep->this_book->anastasia_memory), s->name_start_chars);
    if (s->name_chars) ana_free(&(s->sgrep->this_book->anastasia_memory), s->name_chars);
    ana_free(&(s->sgrep->this_book->anastasia_memory), s->word_chars);
    ana_free(&(s->sgrep->this_book->anastasia_memory), s);
}

