#ifndef __MM_RB_TREE_H__
#define __MM_RB_TREE_H__

#include <aspace/region_tracking/node_struct.h>

#define RB_TREE_CHECK_AFTER_TRAVERSAL 0
#define MAX_DEPTH_LEVEL_ORDER_PRINT 6
#define MAX_SIZE_INORDER_PRINT 20
enum rb_tree_node_color {
    BLACK,
    RED
};

typedef struct mm_rb_node {
    enum rb_tree_node_color color;
    nk_aspace_region_t region;
    struct mm_rb_node * parent;
    struct mm_rb_node * left;
    struct mm_rb_node * right;
} mm_rb_node_t;

typedef struct rb_tree
{
    mm_struct_t super;
    mm_rb_node_t * NIL;
    mm_rb_node_t * root;
    
    int (*compf)(mm_rb_node_t * n1, mm_rb_node_t * n2);
} mm_rb_tree_t;

mm_struct_t * mm_rb_tree_create();
int rb_tree_check(mm_struct_t * self);

#endif
