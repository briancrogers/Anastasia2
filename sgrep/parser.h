#include "sgrep.h"

/* Maximum length of one phrase. */
#define MAX_PHRASE_LENGTH 512
/* Maximum length of a reserved word. Could also be smaller */
#define MAX_R_WORD 20
#define ELLENGTH 79
#define SHOWR 5

enum Token { 
    /* Tokens for reserved words */
    W_IN,W_NOT,W_CONTAINING,W_OR,
    W_ORDERED,W_L_ORDERED,W_R_ORDERED,W_LR_ORDERED,
    W_EXTRACTING,
    W_QUOTE,W_L_QUOTE,W_R_QUOTE,W_LR_QUOTE,
    W_EQUAL,		/* PK Febr 12 '96 */
    W_PARENTING,W_CHILDRENING,
    W_NEAR, W_NEAR_BEFORE,
    W_OUTER,W_INNER,W_CONCAT,
    /* number functions */
    W_JOIN, W_FIRST, W_LAST,
    W_FIRST_BYTES,W_LAST_BYTES,
    /* FIXME: what to do with "chars" */
    W_CHARS,
    /* The automagical phrases */
    W_START,W_END,
    /* Phrase types */
    W_PROLOG,
    W_ELEMENTS,
    W_FILE,
    W_STRING, W_REGEX,
    W_DOCTYPE, W_DOCTYPE_PID, W_DOCTYPE_SID,
    W_PI, W_ATTRIBUTE, W_ATTVALUE,
    W_STAG, W_ETAG, W_COMMENT, W_COMMENT_WORD,
    W_WORD, W_CDATA,
    W_ENTITY, W_ENTITY_DECLARATION, W_ENTITY_LITERAL,
    W_ENTITY_PID, W_ENTITY_SID, W_ENTITY_NDATA,
    W_RAW,
    R_WORDS, 
    /* And the rest */
    W_LPAREN,W_RPAREN,W_LBRACK,W_RBRACK,W_COMMA,W_PHRASE,W_NUMBER,
    /* End of file */
    W_EOF,
    /* In sgrep, parse error while scanning is just a kind of token */
    W_PARSE_ERROR,
    W_END_TOKENS
};

enum ScannerState {
    SCANNER_START,
    SCANNER_STRING,
    SCANNER_NAME,
    SCANNER_NUMBER,
    SCANNER_COMMENT,
    SCANNER_LINE_START_COMMENT,
    SCANNER_LINE_NUMBER_COMMENT,
    SCANNER_FILE_NAME_COMMENT_START,
    SCANNER_FILE_NAME_COMMENT
};

/*
 * Parser object
 */
typedef struct {
    SgrepData *sgrep;
    /* The command string and a index to it */
    const char *expr_str;
    int expr_ind;

    /* Current character, token and string of current string token */
    int ch;
    enum Token token;
    SgrepString *unexpanded_string_token;
    SgrepString *string_token;
    
    /* This points to first phrase in the phrase list */
    struct PHRASE_NODE *first_phrase;

    /* for telling where errors occurred */
    int line,column;
    SgrepString *file;
    int errline,errcol,errind;

    /* Scanners state */
    enum ScannerState scanner_state;
    
    /* Needed for freeing data after parse error */
    int nodes;
    ParseTreeNode *node_array[5000];
} Parser;

/*
 * When looking for reserved words they are matched to these strings 
 */
struct {
    const char *word;
    enum Token t;
} reserved_words[]=
{ 
    {"in",W_IN},
    {"not",W_NOT},
    {"containing",W_CONTAINING},
    {"or",W_OR},
    {"..",W_ORDERED},
    {"_.",W_L_ORDERED},
    {"._",W_R_ORDERED},
    {"__",W_LR_ORDERED},
    {"extracting", W_EXTRACTING},
    {"quote",W_QUOTE},
    {"_quote",W_L_QUOTE},
    {"quote_",W_R_QUOTE},
    {"_quote_",W_LR_QUOTE },
    {"equal",W_EQUAL},	 /* PK Febr 12 '96 */
    {"parenting",W_PARENTING},
    {"childrening",W_CHILDRENING},
    {"near",W_NEAR},
    {"near_before",W_NEAR_BEFORE},
    {"outer",W_OUTER},
    {"inner",W_INNER},
    {"concat",W_CONCAT},
    /* Constant lists */
    {"chars",W_CHARS},
    /* Number functions */
    {"first",W_FIRST},
    {"last",W_LAST},
    {"first_bytes",W_FIRST_BYTES},
    {"last_bytes",W_LAST_BYTES},
    {"join",W_JOIN},
    /* Automagical phrases */
    {"start",W_START},
    {"end",W_END},
    /* Different phrases */
    {"file",W_FILE},
    {"string", W_STRING},
    {"regex",W_REGEX},
    {"pi", W_PI},
    {"attribute", W_ATTRIBUTE},
    {"attvalue",W_ATTVALUE},
    {"stag", W_STAG},
    {"etag", W_ETAG},
    {"comments", W_COMMENT},
    {"elements", W_ELEMENTS},
    {"comment_word",W_COMMENT_WORD},
    {"word", W_WORD},
    {"cdata",W_CDATA},
    {"entity",W_ENTITY},

    /* Stuff from DTD */
    {"prologs", W_PROLOG},
    {"doctype", W_DOCTYPE},
    {"doctype_pid", W_DOCTYPE_PID},
    {"doctype_sid",W_DOCTYPE_SID},
    {"entity_declaration",W_ENTITY_DECLARATION },
    {"entity_literal",W_ENTITY_LITERAL },
    {"entity_pid",W_ENTITY_PID },
    {"entity_sid",W_ENTITY_SID },
    {"entity_ndata",W_ENTITY_NDATA },
    /* And the raw phrase */
    {"raw", W_RAW },
    {NULL,W_END_TOKENS}
};

#define NEXT_TOKEN \
    do { token=next_token(parser); if (token==W_PARSE_ERROR) return NULL; } while (0);
#define parse_error(E) \
  do { real_parse_error(parser,(E));return NULL; } while (0)
#define token (parser->token)

void free_parse_tree(SgrepData *this_sgrep, ParseTreeNode *root);
ParseTreeNode *parse_string(SgrepData *this_sgrep,const char *str, struct PHRASE_NODE **phrase_list);
enum Token next_token(Parser *parser);
ParseTreeNode *parse_reg_expr(Parser *parser);
ParseTreeNode *parse_oper_expr(Parser *parser,ParseTreeNode *left);
void real_parse_error(Parser *parser, char *error);
int get_next_char(Parser *parser);
ParseTreeNode *parse_basic_expr(Parser *parser);
ParseTreeNode *parse_int_oper_argument(Parser *parser, enum Oper oper);
ParseTreeNode *create_tree_node(Parser *parser, enum Oper oper);
ParseTreeNode *parse_cons_list(Parser *parser);
ParseTreeNode *parse_integer_function(Parser *parser, enum Oper oper, const char *name);
ParseTreeNode *create_leaf_node(Parser *parser, enum Oper oper, SgrepString *phrase, int phrase_label);
ParseTreeNode *new_string_phrase(Parser *parser, SgrepString *phrase, const char *typestr);
ParseTreeNode *parse_phrase(Parser *parser, const char *typestr);
