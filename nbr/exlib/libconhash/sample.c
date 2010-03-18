

#include <stdio.h>
#include <stdlib.h>

#include "nbr.h"
#include "macro.h"

CHNODE g_nodes[64];
int main()
{
    int i, len;
    const struct node_s *node;
    char str[128];
    long hashes[512];

	/* init nbr */
	nbr_init(NULL);

    /* init conhash instance max 16 node, average 50 replica (vnode)*/
    CONHASH conhash = nbr_conhash_init(NULL, 150, 3);
    if(conhash)
    {
        /* set nodes */
        nbr_conhash_set_node(&g_nodes[0], "titanic", 32);
        nbr_conhash_set_node(&g_nodes[1], "terminator2018", 24);
        nbr_conhash_set_node(&g_nodes[2], "Xenomorph", 25);
        nbr_conhash_set_node(&g_nodes[3], "True Lies", 10);
        nbr_conhash_set_node(&g_nodes[4], "avantar", 48);

        /* add nodes */
        nbr_conhash_add_node(conhash, &g_nodes[0]);
        nbr_conhash_add_node(conhash, &g_nodes[1]);
        nbr_conhash_add_node(conhash, &g_nodes[2]);
        nbr_conhash_add_node(conhash, &g_nodes[3]);
        nbr_conhash_add_node(conhash, &g_nodes[4]);

        ASSERT(nbr_conhash_node_registered(&g_nodes[0]));
        ASSERT(nbr_conhash_node_registered(&g_nodes[1]));
        ASSERT(nbr_conhash_node_registered(&g_nodes[2]));
        ASSERT(nbr_conhash_node_registered(&g_nodes[3]));
        ASSERT(nbr_conhash_node_registered(&g_nodes[4]));


        printf("virtual nodes number %d\n", nbr_conhash_get_vnodes_num(conhash));
        printf("the hashing results--------------------------------------:\n");

        /* try object */
        for(i = 0; i < 20; i++)
        {
            len = sprintf(str, "James.km%03d", i);
            node = nbr_conhash_lookup(conhash, str, len);
            if(node) printf("[%16s] is in node: [%16s]\n", str, node->iden);
        }
        nbr_conhash_get_vnodes(conhash, hashes, sizeof(hashes)/sizeof(hashes[0]));
        nbr_conhash_del_node(conhash, &g_nodes[2]);
        printf("remove node[%s], virtual nodes number %d\n", g_nodes[2].iden, nbr_conhash_get_vnodes_num(conhash));
        printf("the hashing results--------------------------------------:\n");
        for(i = 0; i < 20; i++)
        {
            len = sprintf(str, "James.km%03d", i);
            node = nbr_conhash_lookup(conhash, str, len);
            if(node) printf("[%16s] is in node: [%16s]\n", str, node->iden);
        }
    }
    nbr_conhash_fin(conhash);
    return 0;
}
