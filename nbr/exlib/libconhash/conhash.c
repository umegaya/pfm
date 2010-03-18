
/* Copyright (C) 2010. sparkling.liang@hotmail.com. All rights reserved. */

#include "conhash.h"
#include "conhash_inter.h"
#include "macro.h"

NBR_API
CONHASH nbr_conhash_init(STRHASHFUNC pfhash, int max_vnode, int multiplex)
{
    /* alloc memory and set to zero */
    struct conhash_s *conhash = (struct conhash_s*)calloc(1, sizeof(struct conhash_s));
    if(conhash == NULL)
    {
        return NULL;
    }
    do
	{
        /* setup callback functions */
        if(pfhash != NULL)
        {
            conhash->cb_hashfunc = pfhash;
        }
        else
        {
            conhash->cb_hashfunc = __conhash_hash_def;
        }
		util_rbtree_init(&conhash->vnode_tree);
		if (!(conhash->vnode_a = nbr_array_create(max_vnode,
			sizeof(struct virtual_node_s), NBR_PRIM_EXPANDABLE))) {
			goto error;
		}
		if (!(conhash->rbnode_a = nbr_array_create(max_vnode,
			sizeof(util_rbtree_node_t), NBR_PRIM_EXPANDABLE))) {
			goto error;
		}
		if (!(conhash->lock = nbr_rwlock_create())) {
			goto error;
		}
		return (CONHASH)conhash;
	}while(0);
error:
	nbr_conhash_fin(conhash);
	return NULL;
}

NBR_API
void nbr_conhash_fin(CONHASH ch)
{
	struct conhash_s *conhash = (struct conhash_s *)ch;
	if(conhash != NULL)
	{
		/* free rb tree */
        while(!util_rbtree_isempty(&(conhash->vnode_tree)))
        {
            util_rbtree_node_t *rbnode = conhash->vnode_tree.root;
            util_rbtree_delete(&(conhash->vnode_tree), rbnode);
            __conhash_del_rbnode(conhash, rbnode);
        }
		if (conhash->vnode_a) {
			nbr_array_destroy(conhash->vnode_a);
			conhash->vnode_a = NULL;
		}
		if (conhash->rbnode_a) {
			nbr_array_destroy(conhash->rbnode_a);
			conhash->rbnode_a = NULL;
		}
		if (conhash->lock) {
			nbr_rwlock_destroy(conhash->lock);
			conhash->lock = NULL;
		}
		free(conhash);
	}
}

NBR_API
void nbr_conhash_set_node(struct node_s *node, const char *iden, u_int replica)
{
    strncpy(node->iden, iden, sizeof(node->iden)-1);
    node->replicas = replica;
    node->flag = NODE_FLAG_INIT;
}

NBR_API int
nbr_conhash_node_registered(const CHNODE *node)
{
	return (node->flag&(NODE_FLAG_INIT | NODE_FLAG_IN)
			== (NODE_FLAG_INIT | NODE_FLAG_IN));
}

NBR_API int
nbr_conhash_node_fault(const CHNODE *node)
{
	return (node->flag&(NODE_FLAG_INIT | NODE_FLAG_FAULT)
			== (NODE_FLAG_INIT | NODE_FLAG_FAULT));
}

NBR_API int
nbr_conhash_node_set_fault(CHNODE *node)
{
	if (nbr_conhash_node_registered(node)) {
		node->flag |= NODE_FLAG_FAULT;
		return NBR_OK;
	}
	return NBR_EINVAL;
}

NBR_API
int nbr_conhash_add_node(CONHASH ch, struct node_s *node)
{
	TRACE("ch_add : id = <%s>\n", node->iden);
	struct conhash_s *conhash = (struct conhash_s *)ch;
    if((conhash==NULL) || (node==NULL)) 
    {
        return NBR_EINVAL;
    }
    /* check node fisrt */
    if(!(node->flag&NODE_FLAG_INIT) || (node->flag&NODE_FLAG_IN))
    {
        return NBR_EALREADY;
    }
    node->flag |= NODE_FLAG_IN;
    /* add replicas of server */
    nbr_rwlock_wrlock(conhash->lock);
    __conhash_add_replicas(conhash, node);
    nbr_rwlock_unlock(conhash->lock);
 
    return NBR_OK;
}

NBR_API
int nbr_conhash_del_node(CONHASH ch, struct node_s *node)
{
	TRACE("ch_del : id = <%s>\n", node->iden);
 struct conhash_s *conhash = (struct conhash_s *)ch;
   if((conhash==NULL) || (node==NULL)) 
    {
        return NBR_EINVAL;
    }
    /* check node first */
    if(!(node->flag&NODE_FLAG_INIT) || !(node->flag&NODE_FLAG_IN))
    {
        return NBR_EALREADY;
    }
    node->flag &= (~NODE_FLAG_IN);
    /* add replicas of server */
    nbr_rwlock_wrlock(conhash->lock);
    __conhash_del_replicas(conhash, node);
    nbr_rwlock_unlock(conhash->lock);

    return NBR_OK;
}

NBR_API
CHNODE *nbr_conhash_lookup(CONHASH ch, const char *object, size_t sz)
{
	TRACE("ch_lkup: obj size = %d\n", sz);
 struct conhash_s *conhash = (struct conhash_s *)ch;
    long hash;
    const util_rbtree_node_t *rbnode;
    if((conhash==NULL) || (conhash->ivnodes==0) || (object==NULL)) 
    {
        return NULL;
    }
    /* calc hash value */
    hash = conhash->cb_hashfunc(object, sz);
    TRACE("ch_lkup : hash value = %lu\n", hash);
    
    nbr_rwlock_rdlock(conhash->lock);
    rbnode = util_rbtree_lookup(&(conhash->vnode_tree), hash);
    nbr_rwlock_unlock(conhash->lock);
    if(rbnode != NULL)
    {
        struct virtual_node_s *vnode = rbnode->data;
        return (CHNODE *)vnode->node;
    }
    return NULL;
}
