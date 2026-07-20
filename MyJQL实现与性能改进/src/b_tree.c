#include "b_tree.h"
#include "buffer_pool.h"

#include <stdio.h>

void b_tree_init(const char *filename, BufferPool *pool) {
    init_buffer_pool(filename, pool);
    /* TODO: add code here */
    if(pool->file.length == 0)
    {
        Page * init = get_page(pool, 0);
        release(pool, 0);
        init = get_page(pool, PAGE_SIZE);
        release(pool, PAGE_SIZE);
        BCtrlBlock* control = (BCtrlBlock*)get_page(pool, 0);
        control->root_node = PAGE_SIZE;
        control->free_node_head = -1;
        control->number = 2;
        release(pool, 0);
        BNode* root = (BNode*)get_page(pool, PAGE_SIZE);
        root->n = 0;
        root->next = -1;
        for(int i = 0; i < 2 * DEGREE + 1; i++) 
        {
            root->child[i] = -1;
            get_rid_block_addr(root->row_ptr[i]) = -1;
            get_rid_idx(root->row_ptr[i]) = 0;
        }
        release(pool, PAGE_SIZE);
    }
}

void b_tree_close(BufferPool *pool) {
    close_buffer_pool(pool);
}

RID searchnode(BufferPool* pool, void* key, size_t size, b_tree_ptr_row_cmp_t cmp, off_t rootaddress) 
{
    RID rid;
    get_rid_block_addr(rid) = -1;
    get_rid_idx(rid) = 0;
    if(rootaddress != -1) 
    {
        BNode* root = (BNode*)get_page(pool, rootaddress);
        release(pool, rootaddress);
        if(cmp(key, size, root->row_ptr[0]) < 0)
        {
            return rid;
        }
        int time = 0;
        while(time < root->n && cmp(key, size, root->row_ptr[time]) >= 0) 
        {
            if(cmp(key, size, root->row_ptr[time]) == 0)
            {
                return root->row_ptr[time];
            }
            time++;
        }
        if(root->child[0] == -1) 
        {
            if(cmp(key, size, root->row_ptr[time]) != 0 || time == root->n)
            {
                return rid;
            }
        }
        else if(time == root->n || cmp(key, size, root->row_ptr[time]) < 0) 
        {
            time--;
        }
        if(root->child[0] != -1) 
        {
            rid = searchnode(pool, key, size, cmp, root->child[time]);
        }
        else 
        {
            return root->row_ptr[time];
        }
    }
    return rid;
}

RID b_tree_search(BufferPool *pool, void *key, size_t size, b_tree_ptr_row_cmp_t cmp) {
    BCtrlBlock* control = (BCtrlBlock*)get_page(pool, 0);
    release(pool, 0);
    off_t rootaddress = control->root_node;
    return searchnode(pool, key, size, cmp, rootaddress);
}

void insertitem(BufferPool* pool, int mode, off_t fatheraddress, off_t nodeaddress, RID rid, int location, int startnum) 
{
    BNode* father = (BNode*)get_page(pool, fatheraddress);
    BNode* node = (BNode*)get_page(pool, nodeaddress);
    if(mode == 0) 
    {
        if(node->child[0] == -1) 
        {
            if(location != 0 && father->child[location - 1] != -1) 
            {
                BNode* lastchild = (BNode*)get_page(pool, father->child[location - 1]);
                lastchild->next = nodeaddress;
                release(pool, father->child[location - 1]);
            }
            node->next = father->child[location];
        }
        for(int i = (int)(father->n - 1); i >= location; i--) 
        {
            father->child[i + 1] = father->child[i];
            father->row_ptr[i + 1] = father->row_ptr[i];
        }
        father->n++;
        father->row_ptr[location] = node->row_ptr[0];
        father->child[location] = nodeaddress;
    }
    else 
    {
        for(int i = (int)(node->n - 1); i >= startnum; i--)
        {
            node->row_ptr[i + 1] = node->row_ptr[i]; 
        }
        node->row_ptr[startnum] = rid;
        if(fatheraddress != -1) 
        {
            father->row_ptr[location] = node->row_ptr[0];
        }
        node->n++;
    }
    release(pool, nodeaddress);
    release(pool, fatheraddress);
}

off_t splitnodes(BufferPool* pool, off_t fatheraddress, off_t nodesplitaddress, int num) 
{
    BCtrlBlock* control = (BCtrlBlock*)get_page(pool, 0);
    off_t address;
    BNode* emptynode;
    if(control->free_node_head == -1) 
    {
        address = PAGE_SIZE * control->number;
        control->number++;
    }
    else 
    {
        address = control->free_node_head;
        emptynode = (BNode*)get_page(pool, control->free_node_head);
        release(pool, control->free_node_head);
        control->free_node_head = emptynode->next;
    }
    release(pool, 0);
    BNode* nodesplit = (BNode*)get_page(pool, nodesplitaddress);
    RID splited;
    get_rid_block_addr(splited) = -1;
    get_rid_idx(splited) = 0;
    BNode* splitednode = (BNode*)get_page(pool, address);
    splitednode->n = 0;
    splitednode->next = -1;
    for(int i = 0; i < 2 * DEGREE + 1; i++) 
    {
        splitednode->child[i] = -1;
        get_rid_block_addr(splitednode->row_ptr[i]) = -1;
        get_rid_idx(splitednode->row_ptr[i]) = 0;
    }
    int newnum = 0;
    int maxn = (int)(nodesplit->n);
    for(int i = (int)(nodesplit->n / 2); i < maxn; i++) 
    {
        if(nodesplit->child[0] != -1) 
        {
            splitednode->child[newnum] = nodesplit->child[i];
            nodesplit->child[i] = -1;
        }
        splitednode->row_ptr[newnum] = nodesplit->row_ptr[i];
        nodesplit->row_ptr[i] = splited;
        nodesplit->n--;
        splitednode->n++; 
        newnum++;
    }
    release(pool, address);
    release(pool, nodesplitaddress);
    if(fatheraddress != -1)
    {
        insertitem(pool, 0, fatheraddress, address, splited, num + 1, -1);
        return nodesplitaddress;
    }
    else 
    {
        control = (BCtrlBlock*)get_page(pool, 0);
        if(control->free_node_head == -1) 
        {
            fatheraddress = PAGE_SIZE * control->number;
            control->number++;
        }
        else 
        {
            fatheraddress = control->free_node_head;
            emptynode = (BNode*)get_page(pool, control->free_node_head);
            release(pool, control->free_node_head);
            control->free_node_head = emptynode->next;
        }
        control->root_node = fatheraddress;
        release(pool, 0);
        BNode* father = (BNode*)get_page(pool, fatheraddress);
        father->n = 0;
        father->next = -1;
        for(int i = 0; i < 2 * DEGREE + 1; i++) 
        {
            father->child[i] = -1;
            get_rid_block_addr(father->row_ptr[i]) = -1;
            get_rid_idx(father->row_ptr[i]) = 0;
        }
        release(pool, fatheraddress);
        insertitem(pool, 0, fatheraddress, nodesplitaddress, splited, 0, -1);
        insertitem(pool, 0, fatheraddress, address, splited, 1, -1);
        return fatheraddress;
    }
    return nodesplitaddress;
}

off_t brothers(BufferPool* pool, off_t fatheraddress, int startnum) 
{
    off_t address = -1;
    int maxn = 2 * DEGREE;
    BNode* father = (BNode*)get_page(pool, fatheraddress);
    if(startnum == 0)
    {
        if(father->child[1] != -1) 
        {
            BNode* firstchild = (BNode*)get_page(pool, father->child[1]);
            if(firstchild->n < maxn)
            {
                address = father->child[1];
                release(pool, father->child[1]);
                release(pool, fatheraddress);
                return address;
            }
            release(pool, father->child[1]);
        }
    }
    else
    {
        if(father->child[startnum - 1] != -1) 
        {
            BNode* lastchild = (BNode*)get_page(pool, father->child[startnum - 1]);
            if(lastchild->n < maxn) 
            {
                address = father->child[startnum - 1];
                release(pool, father->child[startnum - 1]);
                release(pool, fatheraddress);
                return address;
            }
            release(pool, father->child[startnum - 1]);
        }
        if(startnum + 1 < father->n && father->child[startnum + 1] != -1) 
        {
            BNode* nextchild = (BNode*)get_page(pool, father->child[startnum + 1]);
            if(nextchild->n < maxn)
            {
                address = father->child[startnum + 1];
                release(pool, father->child[startnum + 1]);
                release(pool, fatheraddress);
                return address;
            }
            release(pool, father->child[startnum + 1]);
        }
    }
    release(pool, fatheraddress);
    return address;
}

void removeitem(BufferPool* pool, int mode, off_t fatheraddress, off_t nodeaddress, int location, int startnum) 
{
    BNode* node = (BNode*)get_page(pool, nodeaddress);
    BNode* father = (BNode*)get_page(pool, fatheraddress);
    RID removed;
    get_rid_block_addr(removed) = -1;
    get_rid_idx(removed) = 0;
    if(mode == 0) 
    {
        if(node->child[0] == -1 && startnum > 0 && father->child[startnum - 1] != -1) 
        {
            BNode* lastchild = (BNode*)get_page(pool, father->child[startnum - 1]);
            lastchild->next = father->child[startnum - 1];
            release(pool, father->child[startnum - 1]);
        }
        int maxn = (int)(father->n);
        for(int i = startnum + 1 ; i < maxn ; i++)
        {
            father->child[i - 1] = father->child[i];
            father->row_ptr[i - 1] = father->row_ptr[i];
        }
        father->n--;
        father->child[father->n] = -1;
        father->row_ptr[father->n] = removed;
    }
    else 
    {
        int maxn = (int)(node->n);
        for(int i = location + 1 ; i < maxn ; i++)
        {
            node->row_ptr[i - 1] = node->row_ptr[i]; 
        }
        node->n--;
        node->row_ptr[node->n] = removed;
        father->row_ptr[startnum] = node->row_ptr[0];
    }
    release(pool, nodeaddress);
    release(pool, fatheraddress);
}

off_t leftest(BufferPool* pool, off_t startaddress) 
{
    off_t address = startaddress;
    BNode* node = (BNode*)get_page(pool, address);
    while(node->child[0] != -1) 
    {
        release(pool, address);
        address = node->child[0];
        node = (BNode*)get_page(pool, address);
    }
    release(pool, address);
    return address;
}

off_t rightest(BufferPool* pool, off_t startaddress) 
{
    off_t address = startaddress;
    BNode* node = (BNode*)get_page(pool, address);
    while(node->child[node->n - 1] != -1) 
    {
        release(pool, address);
        address = node->child[node->n - 1];
        node = (BNode*)get_page(pool, address);
    }
    release(pool, address);
    return address;
}

void moveitem(BufferPool* pool, off_t movedaddress, off_t nowaddress, off_t fatheraddress, int num, int n, b_tree_row_row_cmp_t cmp) 
{
    BNode* movednode = (BNode*)get_page(pool, movedaddress);
    BNode* nownode = (BNode*)get_page(pool, nowaddress);
    release(pool, nowaddress);
    release(pool, movedaddress);
    RID moved;
    get_rid_block_addr(moved) = -1;
    get_rid_idx(moved) = 0;
    off_t lastchildaddress;
    if(cmp(movednode->row_ptr[0], nownode->row_ptr[0]) < 0) 
    {
        if(movednode->child[0] == -1) 
        {
            for(int i = 0 ; i < n ; i++)
            {
                movednode = (BNode*)get_page(pool, movedaddress);
                release(pool, movedaddress);
                RID mid = movednode->row_ptr[movednode->n - 1];
                removeitem(pool, 1, fatheraddress, movedaddress, (int)(movednode->n - 1), num);
                insertitem(pool, 1, fatheraddress, nowaddress, mid, num + 1, 0);
            }
        }
        else 
        {
            for(int i = 0 ; i < n ; i++) 
            {
                movednode = (BNode*)get_page(pool, movedaddress);
                release(pool, movedaddress);
                lastchildaddress = movednode->child[movednode->n - 1];
                removeitem(pool, 0, movedaddress, lastchildaddress, -1, (int)(movednode->n - 1));
                insertitem(pool, 0, nowaddress, lastchildaddress, moved, 0, -1);
            }
        }
        BNode* father = (BNode*)get_page(pool, fatheraddress);
        nownode = (BNode*)get_page(pool, nowaddress);
        movednode = (BNode*)get_page(pool, movedaddress);
        father->row_ptr[num + 1] = nownode->row_ptr[0];
        release(pool, movedaddress);
        release(pool, nowaddress);
        release(pool, fatheraddress);
    }
    else 
    {
        if(movednode->child[0] == -1) 
        {
            for(int i = 0 ; i < n ; i++)
            {
                movednode = (BNode*)get_page(pool, movedaddress);
                release(pool, movedaddress);
                RID mid = movednode->row_ptr[0];
                removeitem(pool, 1, fatheraddress, movedaddress, 0, num);
                movednode = (BNode*)get_page(pool, movedaddress);
                nownode = (BNode*)get_page(pool, nowaddress);
                release(pool, nowaddress);
                release(pool, movedaddress);
                insertitem(pool, 1, fatheraddress, nowaddress, mid, num - 1, (int)(nownode->n));
            }
        }
        else 
        {
            for(int i = 0 ; i < n ; i++)
            {
                movednode = (BNode*)get_page(pool, movedaddress);
                release(pool, movedaddress);
                lastchildaddress = movednode->child[0];
                removeitem(pool, 0, movedaddress, lastchildaddress, -1, 0); 
                movednode = (BNode*)get_page(pool, movedaddress);
                nownode = (BNode*)get_page(pool, nowaddress);
                release(pool, nowaddress);
                release(pool, movedaddress);
                insertitem(pool, 0, nowaddress, lastchildaddress, moved, (int)(nownode->n), -1);
            }
        }
        BNode* father = (BNode*)get_page(pool, fatheraddress);
        movednode = (BNode*)get_page(pool, movedaddress);
        father->row_ptr[num] = movednode->row_ptr[0];
        release(pool, movedaddress);
        release(pool, fatheraddress);
    }
    if(movednode->n > 0) 
    {
        off_t rightaddress = rightest(pool, movedaddress);
        off_t leftaddress = leftest(pool, nowaddress);
        BNode* right = (BNode*)get_page(pool, rightaddress);
        right->next = leftaddress;
        release(pool, rightaddress);
    }
}

off_t insertnode(BufferPool* pool, off_t rootaddress, RID rid, int num, off_t fatheraddress, b_tree_row_row_cmp_t cmp) 
{
    BNode* root = (BNode*)get_page(pool, rootaddress);
    release(pool, rootaddress);
    int time = 0;
    while(time < root->n && cmp(rid, root->row_ptr[time]) >= 0) 
    {
        if(cmp(rid, root->row_ptr[time]) == 0)//same rid
        {
            return rootaddress;
        }
        time++;
    }
    if(time > 0 && root->child[0] != -1) 
    {
        time--;
    }
    if(root->child[0] == -1) 
    {
        insertitem(pool, 1, fatheraddress, rootaddress, rid, num, time);
    }
    else 
    {
        off_t mid = insertnode(pool, root->child[time], rid, time, rootaddress, cmp);
        root = (BNode*)get_page(pool, rootaddress);
        root->child[time] = mid;
        release(pool, rootaddress);
    }
    int maxn = 2 * DEGREE;
    root = (BNode*)get_page(pool, rootaddress);
    release(pool, rootaddress);
    if(root->n > maxn) 
    {
        if(fatheraddress == -1) 
        {
            rootaddress = splitnodes(pool, fatheraddress, rootaddress, num);
        }
        else 
        {
            off_t address = brothers(pool, fatheraddress, num);
            if(address == -1) 
            {
                rootaddress = splitnodes(pool, fatheraddress, rootaddress, num);
            }
            else 
            {
                moveitem(pool, rootaddress, address, fatheraddress, num, 1, cmp);
            }
        }
    }
    if(fatheraddress == -1)
    {
        return rootaddress;
    } 
    else
    {
        BNode* father = (BNode*)get_page(pool, fatheraddress);
        root = (BNode*)get_page(pool, rootaddress);
        release(pool, rootaddress);
        father->row_ptr[num] = root->row_ptr[0];
        release(pool, fatheraddress);
    }
    return rootaddress;
}

RID b_tree_insert(BufferPool *pool, RID rid, b_tree_row_row_cmp_t cmp, b_tree_insert_nonleaf_handler_t insert_handler) {
    BCtrlBlock* control = (BCtrlBlock*)get_page(pool, 0);
    release(pool, 0);
    insertnode(pool, control->root_node, rid, 0, -1, cmp);
    return rid;
}

off_t keybrothers(BufferPool* pool, off_t fatheraddress, int startnum, int* location) 
{
    off_t address = -1;
    int maxn = DEGREE;
    BNode* father = (BNode*)get_page(pool, fatheraddress);
    if(startnum == 0)
    {
        if(father->child[1] != -1) 
        {
            BNode* firstchild = (BNode*)get_page(pool, father->child[1]);
            if(firstchild->n > maxn)
            {
                address = father->child[1];
                *location = 1;
                release(pool, father->child[1]);
                release(pool, fatheraddress);
                return address;
            }
            release(pool, father->child[1]);
        }
    }
    else
    {
        if(father->child[startnum - 1] != -1) 
        {
            BNode* lastchild = (BNode*)get_page(pool, father->child[startnum - 1]);
            if(lastchild->n > maxn) 
            {
                address = father->child[startnum - 1];
                *location = startnum - 1;
                release(pool, father->child[startnum - 1]);
                release(pool, fatheraddress);
                return address;
            }
            release(pool, father->child[startnum - 1]);
        }
        if(startnum + 1 < father->n && father->child[startnum + 1] != -1) 
        {
            BNode* nextchild = (BNode*)get_page(pool, father->child[startnum + 1]);
            if(nextchild->n > maxn)
            {
                address = father->child[startnum + 1];
                *location = startnum + 1;
                release(pool, father->child[startnum + 1]);
                release(pool, fatheraddress);
                return address;
            }
            release(pool, father->child[startnum + 1]);
        }
    }
    release(pool, fatheraddress);
    return address;
}

off_t deletenode(BufferPool* pool, off_t rootaddress, RID rid, int num, off_t fatheraddress, b_tree_row_row_cmp_t cmp) 
{
    BNode* father;
    if(rootaddress != -1) 
    {
        BNode* root = (BNode*)get_page(pool, rootaddress);
        int time = 0;
        while(time < root->n && cmp(rid, root->row_ptr[time]) >= 0) 
        {
            if(cmp(rid, root->row_ptr[time]) == 0)
            {
                break;
            }
            time++;
        }
        if(root->child[0] == -1) 
        {
            if(cmp(rid, root->row_ptr[time]) != 0 || time == root->n)
            {
                return rootaddress;
            }
        }
        else if(time == root->n || cmp(rid, root->row_ptr[time]) < 0) 
        {
            time--;
        }
        release(pool, rootaddress);
        off_t midaddress;
        if(root->child[0] == -1) 
        {
            removeitem(pool, 1, fatheraddress, rootaddress, time, num);
        }
        else 
        {
            midaddress = deletenode(pool, root->child[time], rid, time, rootaddress, cmp);
            root = (BNode*)get_page(pool, rootaddress);
            root->child[time] = midaddress;
            release(pool, rootaddress);
        }
        int change = 0;
        if(fatheraddress == -1 && root->child[0] != -1 && root->n < 2)
        {
            change = 1;
        } 
        else if(fatheraddress != -1 && root->n < DEGREE)
        {
            change = 1;
        }
        if(change == 1) 
        {
            if(fatheraddress != -1) 
            {
                off_t address = keybrothers(pool, fatheraddress, num, &time);
                if(address != -1) 
                {
                    moveitem(pool, address, rootaddress, fatheraddress, time, 1, cmp);
                }
                else 
                {
                    father = (BNode*)get_page(pool, fatheraddress);
                    release(pool, fatheraddress);
                    if(num != 0)
                    {
                        address = father->child[num - 1];
                    }
                    else
                    {
                        address = father->child[1];
                    }
                    BNode* brother = (BNode*)get_page(pool, address);
                    if(brother->n > DEGREE) 
                    {
                        release(pool, address);
                        moveitem(pool, address, rootaddress, fatheraddress, num, 1, cmp);
                    }
                    else 
                    {
                        BNode* node = (BNode*)get_page(pool, rootaddress);
                        int maxn = (int)(node->n);
                        release(pool, rootaddress);
                        release(pool, address);
                        moveitem(pool, rootaddress, address, fatheraddress, num, maxn, cmp);
                        removeitem(pool, 0, fatheraddress, rootaddress, -1, num);
                        BCtrlBlock* control = (BCtrlBlock*)get_page(pool, 0);
                        node = (BNode*)get_page(pool, rootaddress);
                        node->next = control->free_node_head;
                        control->free_node_head = rootaddress;
                        node->n = 0;
                        for(int i = 0; i < 2 * DEGREE + 1; i++) 
                        {
                            node->child[i] = -1;
                            get_rid_block_addr(node->row_ptr[i]) = -1;
                            get_rid_idx(node->row_ptr[i]) = 0;
                        }
                        release(pool, rootaddress);
                        release(pool, 0);
                    } 
                    father = (BNode*)get_page(pool, fatheraddress);
                    release(pool, fatheraddress);
                    rootaddress = father->child[num];
                }
            }
            else 
            {
                if(root->child[0] != -1 && root->n < 2) 
                {
                    midaddress = rootaddress;
                    rootaddress = root->child[0];
                    BCtrlBlock* control = (BCtrlBlock*)get_page(pool, 0);
                    BNode* mid = (BNode*)get_page(pool, midaddress);
                    mid->next = control->free_node_head;
                    control->free_node_head = midaddress;
                    mid->n = 0;
                    for(int i = 0; i < 2 * DEGREE + 1; i++) 
                    {
                        mid->child[i] = -1;
                        get_rid_block_addr(mid->row_ptr[i]) = -1;
                        get_rid_idx(mid->row_ptr[i]) = 0;
                    }
                    control->root_node = rootaddress;
                    release(pool, midaddress);
                    release(pool, 0);
                    return rootaddress;
                }
            }
        }
    }
    if(fatheraddress != -1) 
    {
        father = (BNode*)get_page(pool, fatheraddress);
        off_t childaddress = father->child[num];
        BNode* midnode = (BNode*)get_page(pool, childaddress);
        father->row_ptr[num] = midnode->row_ptr[0];
        release(pool, childaddress);
        release(pool, fatheraddress);
    }
    return rootaddress;
}

void b_tree_delete(BufferPool *pool, RID rid, b_tree_row_row_cmp_t cmp, b_tree_insert_nonleaf_handler_t insert_handler, b_tree_delete_nonleaf_handler_t delete_handler) {
    BCtrlBlock* control = (BCtrlBlock*)get_page(pool, 0);
    release(pool, 0);
    deletenode(pool, control->root_node, rid, 0, -1, cmp);
}