#include "parser.h"
#include "sgrep.h"
#include "common.h"
#include <ctype.h>

void free_parse_tree(SgrepData *this_sgrep, ParseTreeNode *root) {
	assert(root->oper!=INVALID && root->refcount!=0);
	if (root->refcount==-1) {
		assert(root->oper==PHRASE && root->leaf && root->leaf->phrase==NULL);
		root->refcount=0;
		ana_free(&(this_sgrep->this_book->anastasia_memory), root->leaf);
		ana_free(&(this_sgrep->this_book->anastasia_memory), root->result);
		ana_free(&(this_sgrep->this_book->anastasia_memory), root);
	} else {
		root->refcount--;
		if (root->refcount==0) {
			if (root->left) free_parse_tree(this_sgrep,root->left);
			if (root->right) free_parse_tree(this_sgrep,root->right);
			if (root->oper==PHRASE) {
				assert(root->leaf);
				delete_string(root->leaf->phrase);
				ana_free(&(this_sgrep->this_book->anastasia_memory), root->leaf);
				root->leaf=NULL;
			}
			root->oper=INVALID;
			ana_free(&(this_sgrep->this_book->anastasia_memory), root);
		}
	}
}

/*
 * Parses the given command string. Returns root node of the parse tree.
 * phrase_list returns pointer to phrase list 
 */
ParseTreeNode *parse_string(SgrepData *this_sgrep,const char *str, struct PHRASE_NODE **phrase_list) {
	ParseTreeNode *root;
	Parser parser_l;
	Parser *parser=&parser_l;
	parser->sgrep=this_sgrep;
	parser->line=1;
	parser->column=0;
	parser->file=new_string(this_sgrep,8);
	parser->expr_str=str;
	parser->expr_ind=0;
	parser->ch=-1;
	parser->first_phrase=NULL;
	parser->scanner_state=SCANNER_START;
	parser->nodes=0;

	NEXT_TOKEN;
	root=parse_reg_expr(parser);

	if (token==W_RPAREN && root!=NULL) {
		real_parse_error(parser,"Too many ')'s");
		root=NULL;
	}
	delete_string(parser->file);
	
	if (token!=W_EOF) {
		root=NULL;
	}
	if (root==NULL) {
		/* Free whatever was parsed before parse error */
		while(parser->nodes) {
			ParseTreeLeaf *leaf;
			parser->nodes--;
			leaf=parser->node_array[parser->nodes]->leaf;
			if (leaf) {
				if (leaf->phrase) {
				delete_string(leaf->phrase);
				}
				ana_free(&(this_sgrep->this_book->anastasia_memory), leaf);
			}
			ana_free(&(this_sgrep->this_book->anastasia_memory), parser->node_array[parser->nodes]);
		}
	}
	*phrase_list=parser->first_phrase;
	return root;	
}

/*
 * Scans next token from input string. If token was a phrase sets **phrase 
 */
enum Token next_token(Parser *parser) {
    char str[MAX_PHRASE_LENGTH+1];
    int i=0,j=0;
    int start;
    int ch;
    SgrepString **phrase;
    SGREPDATA(parser);

/* if we already have next character in ch then ch!=-1. Otherwise we have to
   get one */
    ch=parser->ch;
    phrase=&parser->string_token;
    if (ch==-1) ch=get_next_char(parser);
 
/* These show the place where possible error started */
    parser->errline=parser->line;
    parser->errcol=parser->column;
    parser->errind=parser->expr_ind;

/* Finite automaton for recognizing next word */	
    do {
	switch (parser->scanner_state) {
	
	    /* This is the start state */
	case SCANNER_START: 
	    /* skip the whitespace */
	    while (isspace(ch)) ch=get_next_char(parser);

	    /* These show the place where possible error started */
	    parser->errline=parser->line;
	    parser->errcol=parser->column;
	    parser->errind=parser->expr_ind;
		
	    switch(ch) {
	    case 0:		        
		return W_EOF;
	    case '(':
		parser->ch=-1;
		return W_LPAREN;
	    case ')': 
		parser->ch=-1;
		return W_RPAREN;
	    case '[':
		parser->ch=-1;
		return W_LBRACK;
	    case ']':
		parser->ch=-1;
		return W_RBRACK;
	    case ',':
		parser->ch=-1;
		return W_COMMA;
	    case '\"': 
		i=0;
		parser->scanner_state=SCANNER_STRING;
		break;
	    case '#':
		parser->scanner_state=SCANNER_COMMENT;
		break;
	    }
	    if ( isalpha(ch) || ch=='.' || ch=='_' )
	    {
		i=1;
		str[0]=tolower(ch);
		parser->scanner_state=SCANNER_NAME;
		start=parser->column;
	    }
	    if (ch>='0' && ch<='9')
	    {
		i=1;
		str[0]=ch;
		start=parser->column;
		parser->scanner_state=SCANNER_NUMBER;
	    }
	    if ( parser->scanner_state==SCANNER_START )
	    {
		real_parse_error(parser,"Invalid character");
		return W_PARSE_ERROR;
	    }
	    break;
		
	    /* This state reads a phrase string */
	case SCANNER_STRING:
	    if (ch==0 || ch=='\n') 
	    {
		real_parse_error(parser,"Unterminated phrase string");
		return W_PARSE_ERROR;
	    }
	    if ( ch<' ' ) {
		real_parse_error(parser,"Unprintable character in phrase");
		return W_PARSE_ERROR;
	    }
	    if (ch=='\"') 
	    {
		str[i]=0;
		*phrase=expand_backslashes(this_sgrep,str);
		if (!(*phrase) || string_len(*phrase)==0) {
		    real_parse_error(parser,"Empty phrase");		    
		    return W_PARSE_ERROR;
		}		    
		parser->scanner_state=SCANNER_START;
		parser->ch=-1;
		return W_PHRASE;
	    }
	    if ( i==MAX_PHRASE_LENGTH )
	    {
		sgrep_error(this_sgrep,"%s ( > %d ) %s %d\n%s\n%s\n",
			    "Phrase length exceeds",MAX_PHRASE_LENGTH,"on line",parser->line,
			    "Either you have forgotten the quote  at the end of phrase or",
			    "you have to recompile with greater MAX_PHRASE_LENGTH.");
		real_parse_error(parser,"");
		return W_PARSE_ERROR;
	    }
	    if ( parser->scanner_state==SCANNER_STRING ) str[i++]=ch;
	    break;	    
	    /* This state reads a reserved word (operator) */
	case SCANNER_NAME:
	    if ( ch==0 || ( !isalpha(ch) && ch!='.' && ch!='_' ) )
	    {
		parser->scanner_state=SCANNER_START;
		str[i]=0;
		for(j=0;reserved_words[j].word;j++)
		    if (strcmp(reserved_words[j].word,str)==0) 
			return reserved_words[j].t;
		real_parse_error(parser,"Unknown word");
		return W_PARSE_ERROR;
	    }
	    if (i==MAX_R_WORD)
	    {
		real_parse_error(parser,"Unknown word");
		return W_PARSE_ERROR;
	    }
	    str[i++]=tolower(ch);
	    break;
	    /* This state skips comments */
	case SCANNER_LINE_START_COMMENT:
	    if (parser->column<=6 && 
		("#line ")[parser->column-1]==ch) {
		break;
	    } else if (parser->column==7 && ch>='0' && ch<='9') {
		parser->line=ch-'0'-1;
		parser->scanner_state=SCANNER_LINE_NUMBER_COMMENT;
	    } else {
		parser->scanner_state=SCANNER_COMMENT;
	    }
	    /* Fall through */		
	case SCANNER_COMMENT:
	    if (ch==0) {
		parser->scanner_state=SCANNER_START;
		return W_EOF;
	    } else if (ch=='\n') {
		parser->scanner_state=SCANNER_START;
	    } else if (ch=='l' && parser->column==2) {
		parser->scanner_state=SCANNER_LINE_START_COMMENT;
	    }
	    break;
	case SCANNER_LINE_NUMBER_COMMENT:	       
	    if (ch>='0' && ch<='9') {
		parser->line=parser->line*10+(ch-'0');    
	    } else if (ch==' ') {
		parser->scanner_state=SCANNER_FILE_NAME_COMMENT_START;
	    } else if (ch=='\n') {
		parser->scanner_state=SCANNER_START;
	    } else if (ch==0) {
		parser->scanner_state=SCANNER_START;
		return W_EOF;
	    } else {
		sgrep_error(this_sgrep,"Warning: '%s':%d: Malformed #line directive.\n",
			    string_to_char(parser->file),parser->errline);
		parser->line=parser->errline;
		parser->scanner_state=SCANNER_COMMENT;
	    }
	    break;
	case SCANNER_FILE_NAME_COMMENT_START:
	    if (ch=='\"') {
		string_clear(parser->file);
		parser->scanner_state=SCANNER_FILE_NAME_COMMENT;
		break;
	    }		
	    sgrep_error(this_sgrep,"Warning: '%s':%d: Malformed #line directive.\n",
			string_to_char(parser->file),parser->errline);
	    parser->line=parser->errline;
	    if (ch==0) {
		parser->scanner_state=SCANNER_START;
		return W_EOF;
		break;
	    }
	    if (ch=='\n') {
		parser->scanner_state=SCANNER_START;
		break;
	    }
	    parser->scanner_state=SCANNER_COMMENT;
	    break;		
	case SCANNER_FILE_NAME_COMMENT:
	    if (ch=='\"') {
		parser->scanner_state=SCANNER_COMMENT;
		break;
	    }
	    if (ch=='\n' || ch==0) {
		string_clear(parser->file);
		string_cat(parser->file,"[unknown]");
		sgrep_error(this_sgrep,"Warning: '%s':%d: Malformed #line directive.\n",
			    string_to_char(parser->file),parser->errline);
		parser->line=parser->errline;
		parser->scanner_state=SCANNER_START;
		if (ch==0) return W_EOF;
		break;
	    }
	    string_push(parser->file,ch);
	    break;	        
	    /* We read a number */
	case SCANNER_NUMBER:
	    if ( ch<'0'|| ch>'9' )
	    {
		parser->scanner_state=SCANNER_START;
		str[i]=0;
		if (*phrase!=NULL && *phrase!=parser->first_phrase->phrase) {
			delete_string(*phrase);
		}
		*phrase=init_string(this_sgrep,i,str);
		return W_NUMBER;
	    }
	    if (i==9)
	    {
		real_parse_error(parser,"Too big number");
		return W_PARSE_ERROR;
	    }
	    str[i++]=ch;
	    break;
	}
	
	ch=get_next_char(parser);
    } while (1);

}

/* Which kind region_expression */
ParseTreeNode *parse_reg_expr(Parser *parser) {
	ParseTreeNode *left;
	if (token==W_EOF)
		parse_error("Unexpected end of expression");

	left=parse_basic_expr(parser);
	if (left==NULL) return NULL;
	if ( token==W_EOF || token==W_RPAREN ) return left;
	return parse_oper_expr(parser,left);
}	

/* Which kind of operator expression */
ParseTreeNode *parse_oper_expr(Parser *parser,ParseTreeNode *left) {
	ParseTreeNode *o=NULL;
	switch (token)
	{
	case W_NEAR:
	        o=parse_int_oper_argument(parser,SGNEAR);
		break;
	case W_NEAR_BEFORE:
	        o=parse_int_oper_argument(parser,NEAR_BEFORE);
		break;
	case W_IN:		
		o=create_tree_node(parser,SGIN);
		break;
	case W_CONTAINING:	
		o=create_tree_node(parser,CONTAINING);
		break;
/* PK Febr 95 */
	case W_EQUAL:	
		o=create_tree_node(parser,EQUAL);
		break;
/* PK Febr 95 */
	case W_PARENTING:
	        o=create_tree_node(parser,PARENTING);
                break;
	case W_CHILDRENING:
	        o=create_tree_node(parser,CHILDRENING);
		break;
	case W_OR: 		
		o=create_tree_node(parser,OR);
		break;
	case W_ORDERED: 
		o=create_tree_node(parser,ORDERED);
		break;
	case W_L_ORDERED:
		o=create_tree_node(parser,L_ORDERED);
		break;
	case W_R_ORDERED:
		o=create_tree_node(parser,R_ORDERED);
		break;
	case W_LR_ORDERED:
		o=create_tree_node(parser,LR_ORDERED);
		break;
	case W_EXTRACTING:
		o=create_tree_node(parser,EXTRACTING);
		break;
	case W_QUOTE:
		o=create_tree_node(parser,QUOTE);
		break;
	case W_R_QUOTE:
		o=create_tree_node(parser,R_QUOTE);
		break;
	case W_L_QUOTE:
		o=create_tree_node(parser,L_QUOTE);
		break;
	case W_LR_QUOTE:
		o=create_tree_node(parser,LR_QUOTE);
		break;
	case W_NOT:
		NEXT_TOKEN;
		if (token==W_CONTAINING) o=create_tree_node(parser,NOT_CONTAINING);
		else if (token==W_IN) o=create_tree_node(parser,NOT_IN);
		else if (token==W_EQUAL) o=create_tree_node(parser,NOT_EQUAL);
		else parse_error("'not' must be followed by 'in', 'containing' or 'equal'");
		break;
	default:
		parse_error("Operator expected");
	}
	if (!o) return NULL;
	NEXT_TOKEN;
	o->right=parse_basic_expr(parser);
	if (o->right==NULL) return NULL;
	o->left=left;
	if (token==W_EOF || token==W_RPAREN) return o;
	return parse_oper_expr(parser,o);
}

/* 
 * Shows a given error message & where it occurred 
 */
void real_parse_error(Parser *parser, char *error) {
	char erlin[ELLENGTH+1];
	int i;
	SGREPDATA(parser);
	
	if (parser->errcol-ELLENGTH+SHOWR>0)
		parser->errind-=ELLENGTH-SHOWR;
	else parser->errind-=parser->errcol;
	for(i=0;i<ELLENGTH && 
		parser->expr_str[i+parser->errind] 
		&& parser->expr_str[i+parser->errind]!='\n';
	    i++) {
	    erlin[i]=parser->expr_str[i+parser->errind];
	    if (erlin[i]=='\t') erlin[i]=' ';
	}
	
	erlin[i]=0;
	if (string_len(parser->file)==0) {
	    sgrep_error(parser->sgrep,"Parse error in command line expression");
	} else if (string_to_char(parser->file)[0]=='-') {
	    sgrep_error(parser->sgrep,"Parse error in stdin line %d",
			parser->errline);
	} else {
	    sgrep_error(parser->sgrep,"Parse error in file '%s' line %d",
			string_to_char(parser->file),parser->errline);
	}
        sgrep_error(parser->sgrep," column %d :\n\t%s\n%s\n",
		    parser->errcol,
		    error,erlin); 
	if ( parser->errcol>ELLENGTH-SHOWR ) parser->errcol=ELLENGTH-SHOWR; 
//        for (i=0;i<parser->errcol-1;i++) sgrep_error(this_sgrep," "); 
        sgrep_error(this_sgrep,"\n");
}

/*
 * Gets next character from input expression. Counts also lines and columns
 * return character must be unsigned to keep sgrep is 8-bit clean
 */

int get_next_char(Parser *parser) {
	switch (parser->expr_str[parser->expr_ind]) {
		case '\n':
			parser->column=-1;
			parser->line++;
			break;
	}
	parser->column++;
	return parser->ch=((unsigned char *)parser->expr_str)[parser->expr_ind++];	
}
/* Which kind of basic expressions */			
ParseTreeNode *parse_basic_expr(Parser *parser) {
    ParseTreeNode *n;
    
    switch (token) {
    case W_LBRACK:
	/* We got a constant region list */
	return parse_cons_list(parser);	
    case W_CHARS:
	/* reserved wors 'chars' was found */
	parse_error("'chars' disabled until I figure out how to fix it (JJ)");
	n=create_leaf_node(parser,PHRASE,NULL,LABEL_CHARS);
	if (!n) return NULL;
	/* we use already created list (see create_constant_lists() )*/
	n->leaf->regions=parser->sgrep->chars_list;
	NEXT_TOKEN;
	return n;
    case W_LPAREN:
	NEXT_TOKEN;
	n=parse_reg_expr(parser);
	if (n==NULL) return NULL;
	if (token!=W_RPAREN)
	    parse_error(") expected");
	NEXT_TOKEN;
	return n;
    case W_OUTER:
    case W_INNER:
    case W_CONCAT:
	/* So we have function. Let's create a tree node for it */
	n=create_tree_node(parser, 
			   (token==W_OUTER) ? OUTER:
			   ((token==W_INNER) ? INNER:CONCAT) ); 
	if (!n) return NULL;
	/* Let's parse the parameter */
	NEXT_TOKEN;
	if (token!=W_LPAREN)
	    parse_error("( expected");
	NEXT_TOKEN;
	n->left=parse_reg_expr(parser);
	if (n->left==NULL) return NULL;
	n->right=NULL; /* Function has only one parameter */
	if (token!=W_RPAREN)
	    parse_error(") expected");
	NEXT_TOKEN;
	return n;
    case W_JOIN:
	return parse_integer_function(parser,JOIN,"join");
    case W_FIRST:
	return parse_integer_function(parser,FIRST,"first");
    case W_LAST:
	return parse_integer_function(parser,LAST,"last");
    case W_FIRST_BYTES:
	return parse_integer_function(parser,FIRST_BYTES,"firs_bytes");
    case W_LAST_BYTES:
	return parse_integer_function(parser,LAST_BYTES,"last_bytes");
	/* phrases */
    case W_PHRASE:
	/* A simple phrase */
	n=new_string_phrase(parser,parser->string_token, "n");
	if (!n) return NULL;
	NEXT_TOKEN;
	return n;
    case W_START:
	/* "start", first of possible regions */
	n=new_string_phrase(parser,NULL,"#start");
	if (!n) return NULL;
	NEXT_TOKEN;
	return n;
    case W_END:
	/* "end", last of possible regions */
	n=new_string_phrase(parser,NULL, "#end");
	if (!n) return NULL;
	NEXT_TOKEN;
	return n;
    case W_COMMENT: 
	n=new_string_phrase(parser,NULL,"-");
	if (!n) return NULL;
	NEXT_TOKEN;
	return n;
    case W_ELEMENTS:
	n=new_string_phrase(parser,NULL,"@elements");
	if (!n) return NULL;
	NEXT_TOKEN;
	return n;
    case W_PROLOG:
	n=new_string_phrase(parser,NULL,"d!");
	if (!n) return NULL;
	NEXT_TOKEN;
	return n;
    case W_CDATA:
	n=new_string_phrase(parser,NULL,"[CDATA");
	if (!n) return NULL;
	NEXT_TOKEN;
	return n;
	/* phrases with argument */
    case W_RAW:
	return parse_phrase(parser,"");
    case W_FILE:
	return parse_phrase(parser,"f");
    case W_STRING: 
	return parse_phrase(parser,"n");
    case W_REGEX:
	return parse_phrase(parser,"r");
    case W_DOCTYPE:
	return parse_phrase(parser, "dn");
    case W_DOCTYPE_PID: 
	return parse_phrase(parser, "dp");
    case W_DOCTYPE_SID:
	return parse_phrase(parser,"ds");
    case W_ENTITY_DECLARATION:
	return parse_phrase(parser,"!ed");
    case W_ENTITY_LITERAL:
	return parse_phrase(parser,"!el");
    case W_ENTITY_PID:
	return parse_phrase(parser,"!ep");
    case W_ENTITY_SID:
	return parse_phrase(parser,"!es");
    case W_ENTITY_NDATA:
	return parse_phrase(parser,"!en");
    case W_PI:
	return parse_phrase(parser,"?");	    
    case W_ATTRIBUTE:
	return parse_phrase(parser,"a");
    case W_ATTVALUE:
	return parse_phrase(parser,"v");	    
    case W_STAG: 
	return parse_phrase(parser,"s");
    case W_ETAG:
	return parse_phrase(parser,"e");
    case W_COMMENT_WORD:
	return parse_phrase(parser,"c");	    
    case W_WORD: 
	return parse_phrase(parser,"w");
    case W_ENTITY:
	return parse_phrase(parser,"&");

	
    default:
	parse_error("Basic expression expected\n");
    }

    /* Dummy return for keeping c++ compilers quiet */
    return NULL;
}

ParseTreeNode *parse_int_oper_argument(Parser *parser, enum Oper oper) {
    ParseTreeNode *n=create_tree_node(parser,oper);
    if (!n) return NULL;
    NEXT_TOKEN;
    if (token!=W_LPAREN) {
	parse_error("Expecting '(' starting integer argument for operator");
	return NULL;    
    }
    NEXT_TOKEN;
    if (token!=W_NUMBER) {
	parse_error("Expecting integer argument for operator");
	return NULL;
    }
    n->number=atoi(parser->string_token->s);
    if (n->number<0) {
	parse_error("Expecting integer value >=0");
	return NULL;
    }
    delete_string(parser->string_token);
    NEXT_TOKEN;
    if (token!=W_RPAREN) {
	parse_error("')' expected");
    }
    return n;    
}

/*
 * Creates and initializez an operator node to parse tree
 */
ParseTreeNode *create_tree_node(Parser *parser, enum Oper oper) {
	ParseTreeNode *n;
	SgrepData *sgrep=parser->sgrep;

	assert(parser->nodes>=0);
	if (parser->nodes==sizeof(parser->node_array)/
	    sizeof(ParseTreeNode *)) {
	    parse_error("Suspiciously many tree nodes\n");
	    return NULL;
	}
	n=(ParseTreeNode *) ana_malloc(&(sgrep->this_book->anastasia_memory), sizeof(ParseTreeNode), __FILE__, __LINE__);    
	n->left=NULL;
	n->right=NULL;
	n->parent=NULL;
	n->oper=oper;
	n->number=-1;
	n->leaf=NULL;
	n->label_left=LABEL_NOTKNOWN;
	n->label_right=LABEL_NOTKNOWN;
	n->refcount=0;
	n->result=NULL;

	parser->node_array[parser->nodes++]=n;
	return n;
}

/* production basic_expr->constant_list */
ParseTreeNode *parse_cons_list(Parser *parser) {
    int s,e,ps,pe;
    char *cons_err="invalid constant region list";
    ParseTreeNode *n;
    RegionList *c_list;
    SGREPDATA(parser);
    
    n=create_leaf_node(parser,PHRASE,NULL,LABEL_CONS);
    if (!n) return NULL;
    NEXT_TOKEN;
    c_list=n->leaf->regions=new_region_list(this_sgrep);
    
    ps=-1;pe=-1;
    while (token!=W_RBRACK) /* We can fall out here immediately,
			       which means we have empty list */
    {
	if (token!=W_LPAREN)
	    parse_error(cons_err);
	NEXT_TOKEN;
	if (token!=W_NUMBER)
	    parse_error(cons_err);
	s=atoi(parser->string_token->s);
	NEXT_TOKEN;
	if (token!=W_COMMA)
	    parse_error(cons_err);
	NEXT_TOKEN;
	if (token!=W_NUMBER)
	    parse_error(cons_err);
	e=atoi((char *)(parser->string_token->s));
	NEXT_TOKEN;
	if (token!=W_RPAREN)
	    parse_error(cons_err);
	if (e<s)
	    parse_error("region end point must be greater than start point");
	NEXT_TOKEN;
	if (s<ps || (s==ps && e<=pe) )
	    parse_error("constant gc list must be sorted");
	if (e<=pe || s==ps)
	{
	    /* nesting detected */
	    c_list->nested=1;
	}
	add_region(c_list,s,e);
	ps=s;pe=e;
    }
    parser->sgrep->statistics.constant_lists++;
    NEXT_TOKEN;
	//Delete any remaining phrases
	if ((*parser).string_token!=NULL && (*parser).string_token!=parser->first_phrase->phrase) {
		delete_string((*parser).string_token);
	}
    return n;
}

/* Parses function(integer,expression) */
ParseTreeNode *parse_integer_function(Parser *parser, enum Oper oper, const char *name) {
    ParseTreeNode *n=create_tree_node(parser, oper);
    if (!n) return NULL;
    /* Let's parse the parameters */
    NEXT_TOKEN;
    if (token!=W_LPAREN)
	parse_error("( expected");
    NEXT_TOKEN;
    if (token!=W_NUMBER) {
	char buf[1000];
	sprintf(buf,"integer expected: %s(integer,expression)",name);
	parse_error(buf);
    }
    n->number=atoi((char *)parser->string_token->s);
    delete_string(parser->string_token);
    parser->string_token=NULL;
    if (n->number<0)
	parse_error("Negative ints not implemented for last");
    NEXT_TOKEN;
    if (token!=W_COMMA) {
	char buf[100];
	sprintf(buf,"',' expected: %s(integer, expression)",name);
	parse_error(buf);
	return NULL;
    }
    NEXT_TOKEN;
    n->left=parse_reg_expr(parser);
    if (n->left==NULL) return NULL;
    n->right=NULL;
    if (token!=W_RPAREN)
	parse_error(") expected");
    NEXT_TOKEN;
    return n;
}

/* 
 * Creates and initializes a leaf node to parse tree 
 */
ParseTreeNode *create_leaf_node(Parser *parser, enum Oper oper, SgrepString *phrase, int phrase_label) {
    ParseTreeNode *n;
    SgrepData *sgrep=parser->sgrep;

    /* Create a leaf node */
    n=create_tree_node(parser,oper);
    if (!n) return NULL;
    n->label_left=phrase_label;
    /* Now create a phrase node for the phrase */
    n->leaf= (struct PHRASE_NODE *) ana_malloc(&(sgrep->this_book->anastasia_memory), sizeof(struct PHRASE_NODE), __FILE__, __LINE__);
    n->leaf->phrase=phrase;
    n->leaf->regions=NULL;
    n->result=NULL;
    return n;
}

ParseTreeNode *new_string_phrase(Parser *parser, SgrepString *phrase, const char *typestr) {
    ParseTreeNode *n;
    SGREPDATA(parser);

    if (phrase==NULL) {
	phrase=init_string(this_sgrep,strlen(typestr),typestr);
    } else {
	push_front(phrase,typestr);
    }
    n=create_leaf_node(parser,PHRASE,phrase,LABEL_PHRASE);
    if (!n) {
	delete_string(phrase);
	return NULL;
    }

    /* Put the new phrase node to phrase list */
    n->leaf->next=parser->first_phrase;
    parser->first_phrase=n->leaf;
    return n;
}

/* 
 * Here starts recursive parser 
 */

ParseTreeNode *parse_phrase(Parser *parser, const char *typestr) {
    ParseTreeNode *n;
    NEXT_TOKEN;
    if (token!=W_LPAREN) parse_error("Expecting '('");
    NEXT_TOKEN;
    if (token!=W_PHRASE) parse_error("Expecting phrase string");
    n=new_string_phrase(parser,parser->string_token,typestr);
    if (!n) return NULL;
    NEXT_TOKEN;
    if (token!=W_RPAREN) parse_error("Expecting ')'");
    NEXT_TOKEN;
    return n;
}

