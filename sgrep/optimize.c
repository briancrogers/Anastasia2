#include "sgrep.h"
#include "common.h"
#include "optimize.h"

/*
 * Performs operator tree optimizations
 */
void optimize_tree(struct SgrepStruct *this_sgrep, ParseTreeNode **root, struct PHRASE_NODE **phrase_list) {
	Optimizer optimizer;
	optimizer.sgrep=this_sgrep;
	optimizer.label_c=LABEL_FIRST;
	optimizer.root=root;
	optimizer.phrase_list=phrase_list;
	optimizer.tree_size=0;
	optimizer.optimized_nodes=0;
	optimizer.optimized_phrases=0;
	/* We need nodes parent information for optimization */
	optimizer.tree_size=add_parents(*root,NULL);
	/* Duplicate phrases are removed and their parents labeled */
	remove_duplicate_phrases(&optimizer);
	/* Duplicate subtrees are removed */
	shrink_tree(&optimizer);
	create_reference_counters(*root);
	stats.parse_tree_size+=optimizer.tree_size;
	stats.optimized_phrases+=optimizer.optimized_phrases;
	stats.optimized_nodes+=optimizer.optimized_nodes;
}

/* 
 * Recursively adds pointers to parents to every tree and phrase node 
 * counts also operator tree size
 */
int add_parents(ParseTreeNode *node,ParseTreeNode *parent)
{
    int nodes=1; /* This node */
    node->parent=parent;

    assert(node->label_right==LABEL_NOTKNOWN);
	
    node->refcount=0;
    if (node->oper==PHRASE)
    {
	node->leaf->parent=node;
    } else
    {
	assert(node->left!=NULL);
	nodes+=add_parents(node->left,node);
	if (node->right!=NULL)
	{
	    nodes+=add_parents(node->right,node);
	}
    }
    return nodes;
}

/*
 * Merges duplicate phrases in phrase list
 */
void remove_duplicate_phrases(Optimizer *o)
{
	struct PHRASE_NODE *pn;
	struct PHRASE_NODE *lpn=NULL;
	struct PHRASE_NODE *tmp;
//	SgrepData *sgrep=o->sgrep;
	char *last;
	
	/* we need to sort phrase list first */
	qsort_phrases(o->phrase_list);
	pn=*o->phrase_list;
	
	last=""; /* It's not possible to have empty phrase in the list,
		    so this can never be matched */
	
	while (pn!=NULL)
	{
		if (strcmp(last,(char *)pn->phrase->s)==0)
		{
			/* Phrase was already in the list */
			
			/* We give parent same label the first alike phrase had */
			pn->parent->label_left=o->label_c;
			/* Removing pn from phrase list */
			lpn->next=pn->next;
			pn->parent->leaf=lpn;
			/* Freeing memory allocated to pn */
			tmp=pn;
			pn=pn->next;
			assert(pn==NULL || (
			    pn->parent!=NULL && 
			    pn->parent->label_left==LABEL_PHRASE
			    ));
			delete_string(tmp->phrase);
			tmp->phrase=NULL;
			ana_free(&(o->sgrep->this_book->anastasia_memory), tmp);
			
			/* Statistics... */
			o->optimized_phrases++;
		} 
		else
		{
			last=(char *)pn->phrase->s;
			o->label_c++;
			pn->parent->label_left=o->label_c;
			lpn=pn;
			pn=pn->next;

			assert(pn==NULL || pn->parent!=NULL);
			assert(pn==NULL || pn->parent->label_left==LABEL_PHRASE);

		}
	}		
}

/*
 * Removes duplicate subtrees from operator tree
 */
void shrink_tree(Optimizer *o) {
	int leaf_list_size;
	int i;
	ParseTreeNode *dad;
	ParseTreeNode *me;
	ParseTreeNode *big_brother;
	int imleft;
	ParseTreeNode **list0;
	int list0_size;
	ParseTreeNode **list1;
	int list1_size;
	ParseTreeNode **tmp;
	ParseTreeNode *root=*o->root;
	SgrepData *sgrep=o->sgrep;

	leaf_list_size=o->tree_size*sizeof(ParseTreeNode *);
	list0=(ParseTreeNode **) ana_malloc(&(sgrep->this_book->anastasia_memory), leaf_list_size, __FILE__, __LINE__);
	list1=(ParseTreeNode **) ana_malloc(&(sgrep->this_book->anastasia_memory), leaf_list_size, __FILE__, __LINE__);
	list0_size=create_leaf_list(root,list0,0);
	list1_size=0;
	
	while (list0_size>1) {
		/* or and equal operators parameters can be swapped */
		for (i=0;i<list0_size;i++)
		{
		        if ((list0[i]->oper==OR ||
			     list0[i]->oper==EQUAL ||
			     list0[i]->oper==SGNEAR)
			    && list0[i]->label_left<list0[i]->label_right)
			{
				int tmp;
				ParseTreeNode *tree_tmp;
				tmp=list0[i]->label_left;
				list0[i]->label_left=list0[i]->label_right;
				list0[i]->label_right=tmp;
				tree_tmp=list0[i]->left;
				list0[i]->left=list0[i]->right;
				list0[i]->right=tree_tmp;
			}
		}
		
		sort_leaf_list(list0,list0_size);
		big_brother=NULL;
		for (i=0;i<list0_size;i++)
		{
			me=list0[i];
			dad=me->parent;
			imleft= (dad->left==me);

			if (big_brother==NULL || comp_tree_nodes(&big_brother,&me)!=0 )
			{
				o->label_c++;
				big_brother=me;
			} else
			{	
			    o->optimized_nodes++;
				/* These don't really need to be changed,
				   It just might help catch some bugs */
				me->left=NULL;
				me->right=NULL;
				me->oper=INVALID;
				ana_free(&(o->sgrep->this_book->anastasia_memory), me);
			}
			
			if (imleft)
			{
				dad->label_left=o->label_c;
				dad->left=big_brother;
			} else
			{
				dad->label_right=o->label_c;
				dad->right=big_brother;
			}
			assert(dad->left!=NULL);

			if (dad->label_left!=LABEL_NOTKNOWN &&
			     (dad->label_right!=LABEL_NOTKNOWN ||
			      dad->right==NULL) )
			{
				if (dad->right==NULL) dad->label_right=LABEL_NOTKNOWN;
				list1[list1_size++]=dad;
			}				
		}
		tmp=list0;
		list0=list1;
		list1=tmp;
		list0_size=list1_size;
		list1_size=0;
	}
	ana_free(&(o->sgrep->this_book->anastasia_memory), list0);
	ana_free(&(o->sgrep->this_book->anastasia_memory), list1);
}

/*
 * Creates the reference counters
 */
void create_reference_counters(ParseTreeNode *root) {
    if (root==NULL) return;	

    if (root->label_left==LABEL_CONS || root->label_left==LABEL_CHARS) {
	/* Lists with these labels should never be freed, because
	 * they are still valid when reusing parse tree
	 */
	root->refcount=-1;
    } else {
	if (root->refcount==0) {
	    /* This node is visited first time. So we need to go down too */
	    create_reference_counters(root->left);
	    create_reference_counters(root->right);
	}
	root->refcount++;
    }    
}

/*
 * sorts phrase list, so that same phrases can easily be detected
 */
struct PHRASE_NODE *qsort_phrases(struct PHRASE_NODE **phrase_list)
{
	struct PHRASE_NODE *list1,*list2,*comp,*next,*p_list;
	p_list=*phrase_list;
	if (p_list==NULL) 
	{
		return NULL; /* Empty list. Return from recursion */
	}
	comp=p_list;
	p_list=p_list->next;
	if (p_list==NULL)
	{
		/* Only one phrase in list. Return from recursion */
		return *phrase_list;
	}
	
	list1=NULL;
	list2=comp;
	comp->next=NULL;
	while(p_list!=NULL)
	{
		next=p_list->next;
		if ( strcmp((char *)comp->phrase->s,
		     (char *)p_list->phrase->s)<0 )
		{
			p_list->next=list2;
			list2=p_list;
		} else
		{
			p_list->next=list1;
			list1=p_list;
		}
		p_list=next;
	}
	/* order should be now list1 .. comp .. list2 */
	comp=qsort_phrases(&list2);
	if (list1==NULL) 
	{
		*phrase_list=list2;
		return comp;
	}
	qsort_phrases(&list1)->next=list2;
	*phrase_list=list1;
	return comp;
}

/*
 * Recursively creates a list of leaf nodes from parse tree
 */
int create_leaf_list(ParseTreeNode *root, ParseTreeNode **list, int ind)
{
	if (root->oper==PHRASE)
	{
		list[ind]=root;
		return ind+1;
	}
	ind=create_leaf_list(root->left,list,ind);
	if (root->right!=NULL)
	{
		ind=create_leaf_list(root->right,list,ind);
	}
	return ind;
}

/*
 * sorts a leaf list using stdlib qsort and comp_tree_nodes 
 */
void sort_leaf_list(ParseTreeNode **leaf_list,int nmemb)
{
	qsort(leaf_list,nmemb,sizeof(ParseTreeNode **),
		(int (*)(const void*,const void*))comp_tree_nodes);
}

/*
 * Compares two tree nodes. returns 0 if they are alike
 * alike means: same oper, and same subtrees
 */
int comp_tree_nodes(ParseTreeNode **n1, ParseTreeNode **n2)
{
    int x;
    if ( ((*n1)->oper==JOIN || (*n1)->oper==FIRST || (*n1)->oper==LAST) &&
	 (*n2)->oper==(*n1)->oper ) {
	/* Join operation takes int parameter, which much be checked */
	x=(*n1)->number - (*n2)->number;
    } else {
	x=(*n1)->oper - (*n2)->oper;
    }
    if (x!=0) return x;

    /* if label_left==LABEL_CONS right subtree must be NULL ! */
    assert( (*n1)->label_left!=LABEL_CONS || (*n1)->right==NULL );
    assert( (*n2)->label_left!=LABEL_CONS || (*n2)->right==NULL );

    if ( (*n1)->label_left==LABEL_CONS && (*n2)->label_left==LABEL_CONS ) 
	return (*n1)!=(*n2); /* FIXME: this might be wrong */
    x=(*n1)->label_left - (*n2)->label_left;
    if (x!=0) return x;
    x=(*n1)->label_right - (*n2)->label_right;
    return x;
}