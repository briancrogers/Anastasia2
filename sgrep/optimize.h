typedef struct {
    SgrepData *sgrep;
    int label_c;
    ParseTreeNode **root;
    ParseTreeLeaf **phrase_list;
    int tree_size;
    int optimized_nodes;
    int optimized_phrases;
} Optimizer;

void optimize_tree(struct SgrepStruct *this_sgrep, ParseTreeNode **root, struct PHRASE_NODE **phrase_list);
int add_parents(ParseTreeNode *node,ParseTreeNode *parent);
void remove_duplicate_phrases(Optimizer *o);
void shrink_tree(Optimizer *o);
void create_reference_counters(ParseTreeNode *root);
struct PHRASE_NODE *qsort_phrases(struct PHRASE_NODE **phrase_list);
int create_leaf_list(ParseTreeNode *root, ParseTreeNode **list, int ind);
void sort_leaf_list(ParseTreeNode **leaf_list,int nmemb);
int comp_tree_nodes(ParseTreeNode **n1, ParseTreeNode **n2);

