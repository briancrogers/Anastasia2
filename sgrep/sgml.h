/* These come from the XML-specs */
#define SGML_NameChars ("-a-zA-Z0-9._:")
#define SGML_NameStartChars ("a-zA-Z_:")
#define DEFAULT_WordChars ("a-zA-Z")

#define MAX_TERM_SIZE 256
#define TERM_PUSH(STRING,CHAR) do { \
if (string_len(STRING)<MAX_TERM_SIZE) {string_push((STRING),(CHAR)); } \
      } while (0);
#define IN_CLIST(LIST,CHAR) ( \
    ( (LIST)->bitmap[(CHAR)/sizeof(unsigned long int)] ) & \
    ( (1UL) << ((CHAR)%sizeof(unsigned long int)) ) )
#define ADD_CLIST(LIST,CHAR) ( \
    ((LIST)->bitmap[(CHAR)/sizeof(unsigned long int)]) |= \
    ( (1UL) << ((CHAR)%sizeof(unsigned long int)) ) )
#define SGML_ENTRY(QUERY,NAME,RAW_NAME,START,END, this_sgrep) \
do { if (this_sgrep->sgml_debug) sgrep_error(this_sgrep,"%s(\"%s\"):%s:(%d,%d)\n",(QUERY),(NAME),(RAW_NAME),(START),(END)); \
if ((START)<=(END)) state->entry(state,(char *)(RAW_NAME),(START),(END), this_sgrep); } while (0)

/*
 * The XML character class definitions. These come straight from the
 * XML spesification
 */

const char *XML_BaseChar=
"\\#x0041-\\#x005A\\#x0061-\\#x007A\\#x00C0-\\#x00D6"
"\\#x00D8-\\#x00F6\\#x00F8-\\#x00FF\\#x0100-\\#x0131"
"\\#x0134-\\#x013E\\#x0141-\\#x0148\\#x014A-\\#x017E"
"\\#x0180-\\#x01C3\\#x01CD-\\#x01F0\\#x01F4-\\#x01F5"
"\\#x01FA-\\#x0217\\#x0250-\\#x02A8\\#x02BB-\\#x02C1\\#x0386"
"\\#x0388-\\#x038A\\#x038C\\#x038E-\\#x03A1\\#x03A3-\\#x03CE"
"\\#x03D0-\\#x03D6\\#x03DA\\#x03DC\\#x03DE\\#x03E0"
"\\#x03E2-\\#x03F3\\#x0401-\\#x040C\\#x040E-\\#x044F"
"\\#x0451-\\#x045C\\#x045E-\\#x0481\\#x0490-\\#x04C4"
"\\#x04C7-\\#x04C8\\#x04CB-\\#x04CC\\#x04D0-\\#x04EB"
"\\#x04EE-\\#x04F5\\#x04F8-\\#x04F9\\#x0531-\\#x0556\\#x0559"
"\\#x0561-\\#x0586\\#x05D0-\\#x05EA\\#x05F0-\\#x05F2"
"\\#x0621-\\#x063A\\#x0641-\\#x064A\\#x0671-\\#x06B7"
"\\#x06BA-\\#x06BE\\#x06C0-\\#x06CE\\#x06D0-\\#x06D3\\#x06D5"
"\\#x06E5-\\#x06E6\\#x0905-\\#x0939\\#x093D\\#x0958-\\#x0961"
"\\#x0985-\\#x098C\\#x098F-\\#x0990\\#x0993-\\#x09A8"
"\\#x09AA-\\#x09B0\\#x09B2\\#x09B6-\\#x09B9\\#x09DC-\\#x09DD"
"\\#x09DF-\\#x09E1\\#x09F0-\\#x09F1\\#x0A05-\\#x0A0A"
"\\#x0A0F-\\#x0A10\\#x0A13-\\#x0A28\\#x0A2A-\\#x0A30"
"\\#x0A32-\\#x0A33\\#x0A35-\\#x0A36\\#x0A38-\\#x0A39"
"\\#x0A59-\\#x0A5C\\#x0A5E\\#x0A72-\\#x0A74\\#x0A85-\\#x0A8B"
"\\#x0A8D\\#x0A8F-\\#x0A91\\#x0A93-\\#x0AA8\\#x0AAA-\\#x0AB0"
"\\#x0AB2-\\#x0AB3\\#x0AB5-\\#x0AB9\\#x0ABD\\#x0AE0"
"\\#x0B05-\\#x0B0C\\#x0B0F-\\#x0B10\\#x0B13-\\#x0B28"
"\\#x0B2A-\\#x0B30\\#x0B32-\\#x0B33\\#x0B36-\\#x0B39\\#x0B3D"
"\\#x0B5C-\\#x0B5D\\#x0B5F-\\#x0B61\\#x0B85-\\#x0B8A"
"\\#x0B8E-\\#x0B90\\#x0B92-\\#x0B95\\#x0B99-\\#x0B9A\\#x0B9C"
"\\#x0B9E-\\#x0B9F\\#x0BA3-\\#x0BA4\\#x0BA8-\\#x0BAA"
"\\#x0BAE-\\#x0BB5\\#x0BB7-\\#x0BB9\\#x0C05-\\#x0C0C"
"\\#x0C0E-\\#x0C10\\#x0C12-\\#x0C28\\#x0C2A-\\#x0C33"
"\\#x0C35-\\#x0C39\\#x0C60-\\#x0C61\\#x0C85-\\#x0C8C"
"\\#x0C8E-\\#x0C90\\#x0C92-\\#x0CA8\\#x0CAA-\\#x0CB3"
"\\#x0CB5-\\#x0CB9\\#x0CDE\\#x0CE0-\\#x0CE1\\#x0D05-\\#x0D0C"
"\\#x0D0E-\\#x0D10\\#x0D12-\\#x0D28\\#x0D2A-\\#x0D39"
"\\#x0D60-\\#x0D61\\#x0E01-\\#x0E2E\\#x0E30\\#x0E32-\\#x0E33"
"\\#x0E40-\\#x0E45\\#x0E81-\\#x0E82\\#x0E84\\#x0E87-\\#x0E88"
"\\#x0E8A\\#x0E8D\\#x0E94-\\#x0E97\\#x0E99-\\#x0E9F"
"\\#x0EA1-\\#x0EA3\\#x0EA5\\#x0EA7\\#x0EAA-\\#x0EAB"
"\\#x0EAD-\\#x0EAE\\#x0EB0\\#x0EB2-\\#x0EB3\\#x0EBD"
"\\#x0EC0-\\#x0EC4\\#x0F40-\\#x0F47\\#x0F49-\\#x0F69"
"\\#x10A0-\\#x10C5\\#x10D0-\\#x10F6\\#x1100\\#x1102-\\#x1103"
"\\#x1105-\\#x1107\\#x1109\\#x110B-\\#x110C\\#x110E-\\#x1112"
"\\#x113C\\#x113E\\#x1140\\#x114C\\#x114E\\#x1150"
"\\#x1154-\\#x1155\\#x1159\\#x115F-\\#x1161\\#x1163\\#x1165"
"\\#x1167\\#x1169\\#x116D-\\#x116E\\#x1172-\\#x1173\\#x1175"
"\\#x119E\\#x11A8\\#x11AB\\#x11AE-\\#x11AF\\#x11B7-\\#x11B8"
"\\#x11BA\\#x11BC-\\#x11C2\\#x11EB\\#x11F0\\#x11F9"
"\\#x1E00-\\#x1E9B\\#x1EA0-\\#x1EF9\\#x1F00-\\#x1F15"
"\\#x1F18-\\#x1F1D\\#x1F20-\\#x1F45\\#x1F48-\\#x1F4D"
"\\#x1F50-\\#x1F57\\#x1F59\\#x1F5B\\#x1F5D\\#x1F5F-\\#x1F7D"
"\\#x1F80-\\#x1FB4\\#x1FB6-\\#x1FBC\\#x1FBE\\#x1FC2-\\#x1FC4"
"\\#x1FC6-\\#x1FCC\\#x1FD0-\\#x1FD3\\#x1FD6-\\#x1FDB"
"\\#x1FE0-\\#x1FEC\\#x1FF2-\\#x1FF4\\#x1FF6-\\#x1FFC\\#x2126"
"\\#x212A-\\#x212B\\#x212E\\#x2180-\\#x2182\\#x3041-\\#x3094"
"\\#x30A1-\\#x30FA\\#x3105-\\#x312C\\#xAC00-\\#xD7A3"
;

const char *XML_Ideographic=
"\\#x4E00-\\#x9FA5\\#x3007\\#x3021-\\#x3029"
;

const char *XML_CombiningChar=
"\\#x0300-\\#x0345\\#x0360-\\#x0361\\#x0483-\\#x0486"
"\\#x0591-\\#x05A1\\#x05A3-\\#x05B9\\#x05BB-\\#x05BD\\#x05BF"
"\\#x05C1-\\#x05C2\\#x05C4\\#x064B-\\#x0652\\#x0670"
"\\#x06D6-\\#x06DC\\#x06DD-\\#x06DF\\#x06E0-\\#x06E4"
"\\#x06E7-\\#x06E8\\#x06EA-\\#x06ED\\#x0901-\\#x0903\\#x093C"
"\\#x093E-\\#x094C\\#x094D\\#x0951-\\#x0954\\#x0962-\\#x0963"
"\\#x0981-\\#x0983\\#x09BC\\#x09BE\\#x09BF\\#x09C0-\\#x09C4"
"\\#x09C7-\\#x09C8\\#x09CB-\\#x09CD\\#x09D7\\#x09E2-\\#x09E3"
"\\#x0A02\\#x0A3C\\#x0A3E\\#x0A3F\\#x0A40-\\#x0A42"
"\\#x0A47-\\#x0A48\\#x0A4B-\\#x0A4D\\#x0A70-\\#x0A71"
"\\#x0A81-\\#x0A83\\#x0ABC\\#x0ABE-\\#x0AC5\\#x0AC7-\\#x0AC9"
"\\#x0ACB-\\#x0ACD\\#x0B01-\\#x0B03\\#x0B3C\\#x0B3E-\\#x0B43"
"\\#x0B47-\\#x0B48\\#x0B4B-\\#x0B4D\\#x0B56-\\#x0B57"
"\\#x0B82-\\#x0B83\\#x0BBE-\\#x0BC2\\#x0BC6-\\#x0BC8"
"\\#x0BCA-\\#x0BCD\\#x0BD7\\#x0C01-\\#x0C03\\#x0C3E-\\#x0C44"
"\\#x0C46-\\#x0C48\\#x0C4A-\\#x0C4D\\#x0C55-\\#x0C56"
"\\#x0C82-\\#x0C83\\#x0CBE-\\#x0CC4\\#x0CC6-\\#x0CC8"
"\\#x0CCA-\\#x0CCD\\#x0CD5-\\#x0CD6\\#x0D02-\\#x0D03"
"\\#x0D3E-\\#x0D43\\#x0D46-\\#x0D48\\#x0D4A-\\#x0D4D\\#x0D57"
"\\#x0E31\\#x0E34-\\#x0E3A\\#x0E47-\\#x0E4E\\#x0EB1"
"\\#x0EB4-\\#x0EB9\\#x0EBB-\\#x0EBC\\#x0EC8-\\#x0ECD"
"\\#x0F18-\\#x0F19\\#x0F35\\#x0F37\\#x0F39\\#x0F3E\\#x0F3F"
"\\#x0F71-\\#x0F84\\#x0F86-\\#x0F8B\\#x0F90-\\#x0F95\\#x0F97"
"\\#x0F99-\\#x0FAD\\#x0FB1-\\#x0FB7\\#x0FB9\\#x20D0-\\#x20DC"
"\\#x20E1\\#x302A-\\#x302F\\#x3099\\#x309A"
;

const char *XML_Digit=
"\\#x0030-\\#x0039\\#x0660-\\#x0669\\#x06F0-\\#x06F9"
"\\#x0966-\\#x096F\\#x09E6-\\#x09EF\\#x0A66-\\#x0A6F"
"\\#x0AE6-\\#x0AEF\\#x0B66-\\#x0B6F\\#x0BE7-\\#x0BEF"
"\\#x0C66-\\#x0C6F\\#x0CE6-\\#x0CEF\\#x0D66-\\#x0D6F"
"\\#x0E50-\\#x0E59\\#x0ED0-\\#x0ED9\\#x0F20-\\#x0F29"
;

const char *XML_Extender=
"\\#x00B7\\#x02D0\\#x02D1\\#x0387\\#x0640\\#x0E46\\#x0EC6"
"\\#x3005\\#x3031-\\#x3035\\#x309D-\\#x309E\\#x30FC-\\#x30FE"
;

typedef struct CharacterListStruct {
    unsigned long int bitmap[65536/sizeof(unsigned long int)];
    SgrepData *sgrep;
} CharacterList;

enum SGMLState { SGML_PCDATA, SGML_WORD, 
		 SGML_ENTITY_OPEN,SGML_ENTITY,SGML_PCDATA_ENTITY,
		 SGML_WORD_ENTITY,
		 SGML_CHARACTER_REFERENCE_OPEN,
		 SGML_DECIMAL_CHARACTER_REFERENCE,
		 SGML_HEX_CHARACTER_REFERENCE,
		 SGML_CHARACTER_REFERENCE_CLOSE,		 
		 SGML_STAGO, SGML_GI, SGML_W_ATTNAME, 
                 SGML_ATTNAME,
                 SGML_W_ATTEQUAL, SGML_W_ATTVALUE, 
		 SGML_ATTVALUE, SGML_ATTVALUE_DQUOTED, 
		 SGML_ATTVALUE_SQUOTED, SGML_ATTRIBUTE_END,
		 SGML_STAGC, SGML_END_TAG, SGML_W_ETAGC,
		 SGML_DECLARATION_START,SGML_DOCTYPE_DECLARATION,
		 SGML_COMMENT_START, SGML_COMMENT, SGML_COMMENT_WORD,
		 SGML_COMMENT_END1, SGML_COMMENT_END2,
		 SGML_PI, SGML_PI_END,
		 SGML_MARKED_SECTION_START, SGML_MARKED_SECTION_START2,
		 SGML_CDATA_MARKED_SECTION, SGML_CDATA_MARKED_SECTION_WORD,
		 SGML_CDATA_MARKED_SECTION_END1,
		 SGML_CDATA_MARKED_SECTION_END2,
		 SGML_DOCTYPE, SGML_DOCTYPE_EXTERNAL,
		 SGML_DOCTYPE_PUBLIC, SGML_DOCTYPE_SYSTEM,
		 SGML_WAITING_LITERAL,
		 SGML_LITERAL_START,SGML_LITERAL_SQUOTED,SGML_LITERAL_DQUOTED,
		 SGML_DOCTYPE_PUBLIC_ID_START, SGML_DOCTYPE_PUBLIC_ID,
		 SGML_DOCTYPE_SYSTEM_ID_START, SGML_DOCTYPE_SYSTEM_ID,
		 SGML_DOCTYPE_INTERNAL_START,
		 SGML_DOCTYPE_INTERNAL,
		 SGML_INTERNAL_DECLARATION_START1,
		 SGML_INTERNAL_DECLARATION_START2,
		 SGML_PEREFERENCE,
		 SGML_INTERNAL_DECLARATION_NAME,
		 SGML_ENTITY_DECLARATION,
		 SGML_ENTITY_DECLARATION_NAME,
		 SGML_ENTITY_DEFINITION,
		 SGML_ENTITY_DEFINITION_TYPE,
		 SGML_LITERAL_ENTITY,
		 SGML_GENERAL_ENTITY_DEFINITION_DQUOTED,
		 SGML_GENERAL_ENTITY_DEFINITION_SQUOTED,
		 SGML_ENTITY_DEFINITION_PUBLIC_ID,
		 SGML_WAITING_ENTITY_DEFINITION_SYSTEM_ID,
		 SGML_ENTITY_DEFINITION_SYSTEM_ID,
		 SGML_ENTITY_DEFINITION_NDATA,
		 SGML_ENTITY_DEFINITION_NDATA2,
		 SGML_ENTITY_DEFINITION_NDATA_NAME,
		 SGML_ENTITY_DEFINITION_END,
		 SGML_ELEMENT_TYPE_DECLARATION,
		 SGML_ATTLIST_DECLARATION,
		 SGML_NOTATION_DECLARATION,
		 SGML_DOCTYPE_END,
		 SGML_RESERVED_WORD
                };

typedef struct ElementStackStruct {
    char *gi;
    int start;
    int end;
    struct ElementStackStruct *prev;
} ElementStack;

enum EncoderState { 
    EIGHT_BIT, 
    UTF8_1, UTF8_2, UTF8_3_1, UTF8_3_2,
    UTF16_1,
    UTF16_BIG_START,UTF16_BIG,
    UTF16_SMALL_START,UTF16_SMALL
};

typedef struct EncoderStruct {
    enum EncoderState estate;
    int char1;
    int char2;
    int prev;
} Encoder;


struct SGMLScannerStruct {
    SgrepData *sgrep;
    FileList *file_list;
    int file_num;

    /* The Encoder */
    Encoder encoder;

    /* Scanner "configuration" */
    ScannerType type;
    CharacterList *name_start_chars;
    CharacterList *name_chars;
    CharacterList *word_chars;
    CharacterList *indexed_chars;
    int ignore_case;
    int include_system_entities;

    /* Maintaining the element stack */
    int maintain_element_stack;
    ElementStack *top;
    RegionList *element_list;
    
    /* Scanner state */
    int parse_errors;
    struct PHRASE_NODE *phrase_list;
    int words;
    int word_end;
    SgrepString *word;

    /* Start and end tag */
    int tags;
    SgrepString *gi;

    /* Attributes */
    int anames;
    SgrepString *aname;
    int avals;
    SgrepString *aval;

    /* Comments */
    int comments;
    int comment_words;
    SgrepString *comment_word;

    int markeds;
    
    int doctypes;
    int doctype_declarations;
    int internal_declarations;

    int entity_has_systemid;
    int entity_is_ndata;
    int entitys; /* Start of entity reference */
    int character_reference;

    SgrepString *name;

    int name2s;
    SgrepString *name2;
    
    int literals;
    SgrepString *literal;

    int publici;
    int systemi;

    SgrepString *pi;

    enum SGMLState state;
    
    /* This isn't a real stack of context free language parser;
     * it's just a trick for some state reuse */
#define SGML_SCANNER_STACK_SIZE 10
    enum SGMLState state_stack[SGML_SCANNER_STACK_SIZE];
    int state_stack_ptr;

    void (*entry)(struct SGMLScannerStruct *state,
		  const char *str, int start, int end, struct SgrepStruct *this_sgrep);
    void *data;

    int failed;
};

SGMLScanner *new_sgml_phrase_scanner(SgrepData *this_sgrep, FileList *file_list, struct PHRASE_NODE *list);
SGMLScanner *new_sgml_scanner_common(SgrepData *this_sgrep, FileList *file_list);
void sgml_add_entry_to_gclist(SGMLScanner *state, const char *phrase,int start, int end, struct SgrepStruct *this_sgrep);
CharacterList *new_character_list(SgrepData *this_sgrep);
void character_list_add(CharacterList *a,const char *l);
void reset_encoder(SGMLScanner *sgmls, Encoder *encoder);
SGMLScanner *new_sgml_phrase_scanner(SgrepData *this_sgrep, FileList *file_list, struct PHRASE_NODE *list);
void sgml_flush(SGMLScanner *sgmls);
int sgml_scan(SGMLScanner *scanner, const unsigned char *buf, int len, int start,int file_num);
void pop_elements_to(SGMLScanner *state,ElementStack *p);
void sgml_add_entry_to_index(SGMLScanner *state, const char *phrase, int start, int end, struct SgrepStruct *this_sgrep);
void push_state(SGMLScanner *scanner, enum SGMLState state);
enum SGMLState pop_state(SGMLScanner *scanner);
void sgml_found(SGMLScanner *state,enum SGMLState s,int end_index);
void parse_xml_declaration(SGMLScanner *scanner);
void delete_sgml_scanner(SGMLScanner *s);
