typedef struct {
    struct SgrepStruct *sgrep;
    const FileList *files;
    Region *tmp_stack;
    int tmp_stack_size;
} Evaluator;

#define DEFAULT_STACK_SIZE 1024

#define contains(r1,r2) \
	(((r1).start<(r2).start && (r1).end>=(r2).end) || \
	((r1).start<=(r2).start && (r1).end>(r2).end))

RegionList *eval(struct SgrepStruct *this_sgrep, const FileList *file_list, ParseTreeNode *root);
RegionList *recursive_eval(Evaluator *evaluator,ParseTreeNode *root);
RegionList *eval_operator(Evaluator *evaluator,ParseTreeNode *root, struct SgrepStruct *this_sgrep);
int free_tree_node(ParseTreeNode *node);
RegionList *or(RegionList *l,RegionList *r);
RegionList *nest_order(Evaluator *evaluator, RegionList *l,RegionList *r,int type);
RegionList *quote(RegionList *l,RegionList *r,int type);
RegionList *in(RegionList *l,RegionList *r, int not);
RegionList *containing(Evaluator *evaluator,RegionList *l, RegionList *r,int not);
RegionList *equal(RegionList *l,RegionList *r,int not);
RegionList *parenting(Evaluator *evaluator,RegionList *l, RegionList *r);
RegionList *childrening(RegionList *children, RegionList *parents);
RegionList *outer(RegionList *gcl);
RegionList *inner(Evaluator *evaluator,RegionList *gcl);
RegionList *extracting(RegionList *l,RegionList *r);
RegionList *concat(RegionList *l);
RegionList *join(Evaluator *evaluator,RegionList *l,int number);
RegionList *first(RegionList *input, int num);
RegionList *last(RegionList *input, int num);
RegionList *first_bytes(RegionList *input, int num, struct SgrepStruct *this_sgrep);
RegionList *last_bytes(RegionList *input, int num, struct SgrepStruct *this_sgrep);
RegionList *eval_near(RegionList *l, RegionList *r, int how_near);
RegionList *near_before(RegionList *l, RegionList *r, int how_near);
Region first_of(ListIterator *lp,ListIterator *rp);
int list_find_first_start(RegionList *list, int start, int index);

