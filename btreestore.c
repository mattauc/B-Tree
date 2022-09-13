#include "btreestore.h"
#include <pthread.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>

#define BYTE unsigned char

void * init_store(uint16_t branching, uint8_t n_processors) {

    struct btree* my_tree = (struct btree*)malloc(sizeof(struct btree));

    my_tree->branching = branching;
    my_tree->n_processors = n_processors;
    my_tree->root = NULL;
    my_tree->largest_key = 0;
    my_tree->node_count = 0;

    //Set up concurrency environment...
    pthread_mutex_init(&my_tree->mutex, NULL);
    my_tree->pool = malloc(sizeof(pthread_t)*n_processors);

    return my_tree;
}

int free_node(struct btree_node* node) {

    //Free individual nodes and all their allocated content
    if (node == NULL) {
        return 1;
    }
    free(node->children);
    for (int i = 0; i < node->link_count; i++) {
        free(node->key_values[i]->tmp2);
        free(node->key_values[i]->data);
        free(node->key_values[i]);
    }
    free(node->key_values);
    free(node);

    return 0;
}

void postorder_traversal(struct btree_node* current) {

    if (current == NULL) {
        return;
    }
    for (int i = 0; i < current->child_count; i++) {
        postorder_traversal(current->children[i]);
    }
    free_node(current);

    return;
}

void close_store(void * helper) {

    if (helper == NULL) {
        return;
    }
    struct btree* my_tree = (struct btree*)helper;
    pthread_mutex_destroy(&my_tree->mutex);
    free(my_tree->pool);

    if (my_tree->root == NULL || my_tree->node_count == 0) {
        free(helper);
        return;
    }
    postorder_traversal(my_tree->root);
    free(helper);

    return;
}

struct btree_node* create_node(struct btree* my_tree) {
    
    struct btree_node* node;

    node = (struct btree_node*)malloc(sizeof(struct btree_node));
    node->children = malloc(sizeof(struct btree_node*)*(my_tree->branching+1));
    node->key_values = malloc(sizeof(struct dict*)*(my_tree->branching+1));

    node->child_count = 0;
    node->link_count = 0;
    node->parent = NULL;
    node->leaf = 1;

    return node;
}

struct dict* create_key(uint32_t key, uint64_t * plaintext, size_t count, uint32_t encryption_key[4], uint64_t nonce, void * helper) {

    struct btree* my_tree = (struct btree*)helper;
    
    struct dict* new;
    new = (struct dict*)malloc(sizeof(struct dict));
    new->nonce = nonce;
    new->size = count;
    new->key = key;
    new->tmp2 = 0;

    int block_num = ((count + (8-1))/8);

    new->data = (uint64_t*)malloc(sizeof(uint64_t)*block_num);
    new->tmp2 = malloc(sizeof(uint64_t)*block_num);
    void* text = malloc(sizeof(uint64_t)*block_num);
    memmove(text, plaintext, count);
    memmove(new->encrypt_key, encryption_key, sizeof(uint32_t)*4);
    
    if (new->size > 600) {
        btree_encrpyt(text, new->encrypt_key, new->nonce, new->data, block_num, helper, new->tmp2);
    } else{
        my_tea_ctr(text, new->encrypt_key, new->nonce, new->data, block_num, new->tmp2);
    }   
    free(text);

    return new;
}

struct btree_node* btree_search(uint32_t key, struct btree* helper, struct btree_node* node) {

    if (node == NULL) {
        return NULL;
    } else if (node->leaf == 1) {
        return node;
    }

    for (int i = 0; i < node->link_count; i++) {

        if (node->key_values[i]->key == key) {
            return node;
        }
        if (key < node->key_values[i]->key) {
            node = btree_search(key, helper, node->children[i]);
            break;
        } else if (i == node->link_count-1) {
            node = btree_search(key, helper, node->children[i+1]);
            break;
        }
    }
    return node;
}

int retreive_key(struct btree_node* flag, uint32_t key) {
    
    int i = 0;
    for(; i < flag->link_count; i++) {
        if (flag->key_values[i]->key == key) {
            return i;
        }
    }
    return -1;
}

int retreive_child(struct btree_node* parent, struct btree_node* target) {

    int i = 0;
    for (; i < parent->child_count; i++) {
        if (parent->children[i] == target) {
            return i;
        }
    }
    return -1;
}

int key_shift(struct btree_node* flag, struct btree* my_tree, uint32_t key) {

    int i = 0;
    for (; i < flag->link_count; i++) {
        if ((flag->key_values[i])->key > key || (flag->key_values[i]) == NULL) {
            if ((flag->key_values[i]) != NULL) {
                memmove(flag->key_values+(i+1), flag->key_values+i, sizeof(struct dict*)*(flag->link_count-(i)));
            }
            break;
        }
    }
    return i;
}

int child_shift(struct btree_node* flag, struct btree* my_tree, uint32_t key) {

    int i = 0;
    for (;i < flag->child_count; i++) {
        if (flag->children[i]->key_values[0]->key > key || flag->children[i] == NULL) {
            if ((flag->key_values[i]) != NULL) {
                memmove(flag->children+(i+1), flag->children+i, sizeof(struct btree_node*)*(flag->child_count-(i)));
            }
            break;
        }
    }
    return i;
}

int child_rearrangement(struct btree_node* flag, struct btree_node* right, struct btree_node* new_root) {

    for (int i = 0; i < flag->child_count; i++) {
        if (flag->children[i]->key_values[0]->key > flag->key_values[flag->link_count-1]->key) {
            i++;
            memmove(right->children, flag->children+i, sizeof(struct btree_node*)*(flag->child_count-i));
            
            right->child_count += (flag->child_count-i);
            for (int j = 0; j < flag->child_count-i; j++) {
                right->children[j]->parent = right;
            }
            flag->child_count -= (flag->child_count-i);
            return 1;
        }
    }
    return 0;
}

void create_right_node(struct btree* my_tree, struct btree_node* flag, struct btree_node* right, int median, char check) {

    //Moving the right half of left sibling key_values into right sibling. Deleting median
    if (check == 1) {
        memmove(right->key_values, (flag->key_values+median+1), sizeof(struct dict*)*(median+1));
        flag->key_values[median] = NULL;
    } else {
        memmove(right->key_values, (flag->key_values+median), sizeof(struct dict*)*(median+1));
        right->parent = flag->parent;
    }
    
    flag->link_count -= flag->link_count-median;
    if ((my_tree->branching-1) % 2 != 0) {
        right->link_count += median+1;
    } else {
        right->link_count += median;
    }

    int ret = child_rearrangement(flag, right, flag->parent);
    if (ret != 0) {
        right->leaf = 0;
        flag->leaf = 0;
    }
}

void create_new_root(struct btree_node* flag, struct btree* my_tree, int median, int pos) {

    struct btree_node* right = create_node(my_tree);
    struct btree_node* new_root = create_node(my_tree);

    new_root->key_values[0] = flag->key_values[median];
    
    new_root->leaf = 0;
    new_root->children[1] = right;
    new_root->children[0] = flag;
    new_root->child_count += 2;
    new_root->link_count += 1;

    my_tree->root = new_root;

    flag->parent = new_root;
    right->parent = new_root;
    new_root->parent = NULL;

    create_right_node(my_tree, flag, right, median, 1);
    my_tree->node_count += 2;
}


void split_node(int pos, struct btree_node* flag, struct btree* my_tree) {

    int median = ((flag->link_count)/2);
    if (flag->link_count % 2 == 0) {
        median -= 1;
    } 
    if (flag == my_tree->root) {
        create_new_root(flag, my_tree, median, pos);
        return;
    }

    //Shifting the parent key_values, adding the median to the parent
    int link_pos = key_shift(flag->parent, my_tree, flag->key_values[median]->key);
    flag->parent->key_values[link_pos] = flag->key_values[median];
    flag->parent->link_count++;

    memmove(flag->key_values+median, flag->key_values+median+1, sizeof(struct dict*)*(median+1));
    flag->link_count--;  

    if (flag->parent->child_count+1 <= my_tree->branching || flag->parent->key_values[link_pos]->key > 
    flag->key_values[0]->key) {
    
        //Creating a right sibling, adding sibling to the parent
        struct btree_node* right = create_node(my_tree);
        int child_pos = child_shift(flag->parent, my_tree, flag->key_values[median]->key);
        flag->parent->children[child_pos] = right;
        flag->parent->child_count++;

        create_right_node(my_tree, flag, right, median, 0);
        my_tree->node_count += 1;
    } 
    if (flag->parent->link_count > my_tree->branching-1)  {
        split_node(pos, flag->parent, my_tree);
    }
    return;
}   

int btree_insert(uint32_t key, void * plaintext, size_t count, uint32_t encryption_key[4], uint64_t nonce, void * helper) {

    struct btree* my_tree = (struct btree*)helper;
    pthread_mutex_lock(&my_tree->mutex);

    struct btree_node* flag = btree_search(key, my_tree, my_tree->root);
    
    if (my_tree->root == NULL) {
        flag = create_node(my_tree);
        my_tree->node_count += 1;
        my_tree->root = flag;
    }
    if (retreive_key(flag, key) != -1) {
        pthread_mutex_unlock(&my_tree->mutex);
        return 1;
    }
    if (key > my_tree->largest_key) {
        my_tree->largest_key = key;
    }
    
    struct dict* new_key = create_key(key, plaintext, count, encryption_key, nonce, my_tree);
    int pos = key_shift(flag, my_tree, key);

    flag->key_values[pos] = new_key;
    flag->link_count += 1;

    if (flag->link_count <= my_tree->branching-1) {
        pthread_mutex_unlock(&my_tree->mutex);
        return 0;
    } else {
        split_node(pos, flag, my_tree);
        pthread_mutex_unlock(&my_tree->mutex);
        return 0;
    }
    pthread_mutex_unlock(&my_tree->mutex);
    return 1;
}

int btree_retrieve(uint32_t key, struct info * found, void * helper) {

    struct btree* my_tree = (struct btree*)helper;
    if (my_tree->root == NULL) {
        return 1;
    }
    pthread_mutex_lock(&my_tree->mutex);
    struct btree_node* flag = btree_search(key, my_tree, my_tree->root);

    int i = 0;
    char check = 0;
    for (;i < flag->link_count; i++) {
        if (flag->key_values[i]->key == key) {
            check = 1;
            break;
        }
    }
    if (check == 1) {
        found->size = flag->key_values[i]->size;
        found->nonce = flag->key_values[i]->nonce;
        found->data = flag->key_values[i]->data;
        memmove(found->key, flag->key_values[i]->encrypt_key, sizeof(uint32_t)*4);

        pthread_mutex_unlock(&my_tree->mutex);
        return 0;
    }
    pthread_mutex_unlock(&my_tree->mutex);
    return 1;
}

int btree_decrypt(uint32_t key, void * output, void * helper) {

    struct btree* my_tree = (struct btree*)helper;
    if (my_tree->root == NULL) {
        return 1;
    }
    pthread_mutex_lock(&my_tree->mutex);
    struct btree_node* flag = btree_search(key, my_tree, my_tree->root);

    int i = 0;
    char check = 0;
    for (;i < flag->link_count; i++) {
        if (flag->key_values[i]->key == key) {
            check = 1;
            break;
        }
    }
    if (check == 1) {
        
        int block_num = ((flag->key_values[i]->size + (8-1))/8);
        uint64_t* text = (uint64_t*)malloc(sizeof(uint64_t)*block_num);

        uint64_t* encrypted = (uint64_t*)flag->key_values[i]->data;
        uint64_t* cipher_key = (uint64_t*)flag->key_values[i]->tmp2;
        for(int j = 0; j < block_num; j++) {
            text[j] = encrypted[j] ^ cipher_key[j];
        }
        memmove(output, text, flag->key_values[i]->size);
        free(text);

        pthread_mutex_unlock(&my_tree->mutex);
        return 0;
    }
    pthread_mutex_unlock(&my_tree->mutex);
    return 1;
}

void delete_key(struct btree_node* flag, int index) {

    free(flag->key_values[index]->data);
    free(flag->key_values[index]->tmp2);
    free(flag->key_values[index]);

    flag->key_values[index] = NULL;
    memmove(flag->key_values+index, flag->key_values+(index+1), sizeof(struct dict*)*(flag->link_count-(index)));

    flag->link_count -= 1;
}

void left_swap(struct btree* my_tree, struct btree_node* parent, struct btree_node* target, struct btree_node* left, int target_index) {

    target->key_values[0] = parent->key_values[target_index];
    parent->key_values[target_index] = left->key_values[left->link_count-1];
    left->key_values[left->link_count-1] = NULL;

    target->link_count += 1;
    left->link_count -= 1;
}

void right_swap(struct btree* my_tree, struct btree_node* parent, struct btree_node* target, struct btree_node* right, int target_index) {

    target->key_values[0] = parent->key_values[target_index-1];
    parent->key_values[target_index-1] = right->key_values[0];
    memmove(right->key_values, right->key_values+1, sizeof(struct dict*)*(right->link_count-1));

    target->link_count += 1;
    right->link_count -= 1;
}

void left_push(struct btree* my_tree, struct btree_node* parent, struct btree_node* target, struct btree_node* left, int target_index) {

    struct btree_node* largest_child = left->children[left->child_count-1];
    memmove(target->children+1, target->children, sizeof(struct btree_node*)*(target->child_count));
    target->children[0] = largest_child;
    left->children[left->child_count-1] = NULL;
    largest_child->parent = target;

    target->child_count += 1;
    left->child_count -= 1;
}

void right_push(struct btree* my_tree, struct btree_node* parent, struct btree_node* target, struct btree_node* right, int target_index) {

    struct btree_node* smallest_child = right->children[0];
    target->children[target->child_count] = smallest_child;
    memmove(right->children, right->children+1, sizeof(struct btree_node*)*(right->child_count-1));
    smallest_child->parent = target;

    target->child_count += 1;
    right->child_count -= 1;
}

struct btree_node* left_merge(struct btree* my_tree, struct btree_node* parent, struct btree_node* target, struct btree_node* left, int target_index) {

    left->key_values[1] = parent->key_values[target_index];
    parent->children[target_index+1] = NULL;
    left->parent = parent;

    memmove(parent->key_values+target_index, parent->key_values+target_index+1, sizeof(struct dict*)*(parent->link_count-(target_index+1)));

    memmove(parent->children+target_index+1, parent->children+target_index+2, sizeof(struct btree_node*)*(parent->child_count-(target_index+1)));
    memmove(left->children+left->child_count, target->children, sizeof(struct btree_node*)*(target->child_count));

    for (int i = 0; i < target->child_count; i++) {
        target->children[i]->parent = left;
    }

    parent->link_count -= 1;
    parent->child_count -= 1;
    my_tree->node_count -= 1;
    left->link_count += 1;
    target->link_count -= 1;
    left->child_count += target->child_count;
    free_node(target);

    return left;
}

struct btree_node* right_merge(struct btree* my_tree, struct btree_node* parent, struct btree_node* target, struct btree_node* right, int target_index) {

    right->key_values[1] = right->key_values[0];
    right->key_values[0] = parent->key_values[target_index];
    right->parent = parent;
    
    memmove(parent->children+target_index, parent->children+target_index+1, sizeof(struct btree_node*)*(parent->child_count-(target_index+1)));
    memmove(parent->key_values+target_index, parent->key_values+target_index+1, sizeof(struct dict*)*(parent->link_count-(target_index+1)));

    memmove(right->children+target->child_count, right->children, sizeof(struct btree_node*)*(right->child_count));
    memmove(right->children, target->children, sizeof(struct btree_node*)*(target->child_count));

    for (int i = 0; i < target->child_count; i++) {
        target->children[i]->parent = right;
    }

    target->link_count -= 1;
    parent->child_count -= 1;
    parent->link_count -= 1;
    my_tree->node_count -= 1;
    right->link_count += 1;
    right->child_count += target->child_count;
    free_node(target);

    return right;
}

int rearrange_keys(struct btree* my_tree, struct btree_node* target, uint32_t key) {

    if (target == my_tree->root && target->link_count < 1) {
        my_tree->root = target->children[0];
        free_node(target);
        my_tree->node_count -= 1;
        return 0;
    }

    int left_index = (retreive_child(target->parent, target)-1);
    int right_index = (left_index+2);

    struct btree_node* left = NULL;
    struct btree_node* right = NULL;
    if (left_index >= 0) {
         left = target->parent->children[left_index];
    }
    if (target->parent->child_count-1 >= right_index) {
        right = target->parent->children[right_index];
    }

    int check = 0;
    if (left != NULL && (left->link_count > 1 && left->leaf == 1)) {
        left_swap(my_tree, target->parent, target, left, left_index);
        check++;
    }
    if ((right != NULL && check == 0) && (right->link_count > 1 && right->leaf == 1)) {
        right_swap(my_tree, target->parent, target, right, right_index);
        check++;
    }
    if (check == 0 && (left != NULL && left->leaf == 0) && left->link_count > 1) {
        left_swap(my_tree, target->parent, target, left, left_index);
        left_push(my_tree, target->parent, target, left, left_index);
        check++;
    }
    if (check == 0 && (right != NULL && right->leaf == 0) && right->link_count > 1) {
        right_swap(my_tree, target->parent, target, right, right_index);
        right_push(my_tree, target->parent, target, right, right_index);
        check++;
    }
    if (check == 0) {
        if (left != NULL) {
            target = left_merge(my_tree, target->parent, target, left, left_index);
        } else if (right != NULL) {
            target = right_merge(my_tree, target->parent, target, right, right_index-1);
        }
        check++;
    }
    if (target->parent->link_count <= 0) {
        rearrange_keys(my_tree, target->parent, key);
    }
    return 0;
}

int btree_delete(uint32_t key, void * helper) {

    struct btree* my_tree = (struct btree*)helper;
    if (my_tree->root == NULL) {
        return 1;
    }
    pthread_mutex_lock(&my_tree->mutex);

    struct btree_node* flag = btree_search(key, my_tree, my_tree->root);
    
    int flag_index = retreive_key(flag, key);
    if (flag == NULL || flag_index == -1) {
        pthread_mutex_unlock(&my_tree->mutex);
        return 1;
    }

    struct btree_node* swap = NULL;
    struct btree_node* target = flag;

    if(flag->leaf != 1) {

        swap = btree_search(my_tree->largest_key, my_tree, flag->children[flag_index]);
        free(flag->key_values[flag_index]->data);
        free(flag->key_values[flag_index]->tmp2);
        free(flag->key_values[flag_index]);

        flag->key_values[flag_index] = swap->key_values[swap->link_count-1];
        swap->key_values[swap->link_count-1] = NULL;

        swap->link_count -= 1;
        target = swap;
        
    } else {
        delete_key(flag, flag_index);
    }
    if (target->link_count >= 1) {

        pthread_mutex_unlock(&my_tree->mutex);
        return 0;
    }
    int ret = rearrange_keys(my_tree, target, key);
    pthread_mutex_unlock(&my_tree->mutex);
    return ret;
}

void preorder_traversal(struct btree_node* current, struct btree* my_tree, int* count, struct node* list) {

    if (current == NULL) {
        return;
    }
    struct node new_node = {.num_keys = current->link_count};
    new_node.keys = (uint32_t*)malloc(sizeof(uint32_t)*new_node.num_keys);
    
    for (int i = 0; i < current->link_count; i++) {
       new_node.keys[i] = current->key_values[i]->key;
    }

    list[*count] = new_node;
    for (int i = 0; i < current->child_count; i++) {
        *count += 1;
        preorder_traversal(current->children[i], my_tree, count, list);
    }
    return;
}

uint64_t btree_export(void * helper, struct node ** list) {
    
    struct btree* my_tree = (struct btree*)helper;
    if (my_tree->node_count == 0 || my_tree->root == NULL) {
        return 0;
    }
    struct btree_node* root = my_tree->root;

    *list = malloc(sizeof(struct node)*my_tree->node_count);
    int count = 0;
    preorder_traversal(root, my_tree, &count, *list);
    return count+1;
}

void encrypt_tea(uint32_t plain[2], uint32_t cipher[2], uint32_t key[4]) {

    uint32_t sum = 0;
    uint32_t delta = 0x9E3779B9;

    cipher[0] = plain[0];
    cipher[1] = plain[1];
    for (int i = 0; i < 1024; i++) {
        sum = (sum + delta) % (1L<<32);
        cipher[0] = (cipher[0] + ((((cipher[1] << 4) + key[0]) % (1L<<32)) ^ ((cipher[1] + sum) % (1L<<32)) ^ (((cipher[1] >> 5) + key[1]) % (1L<<32)))) % (1L<<32);
        cipher[1] = (cipher[1] + ((((cipher[0] << 4) + key[2]) % (1L<<32)) ^ ((cipher[0] + sum) % (1L<<32)) ^ (((cipher[0] >> 5) + key[3]) % (1L<<32)))) % (1L<<32);
    }
    return;
}

void decrypt_tea(uint32_t cipher[2], uint32_t plain[2], uint32_t key[4]) {

    uint32_t sum = 0xDDE6E400;
    uint32_t delta = 0x9E3779B9;

    for (int i = 0; i < 1024; i++) { 
        cipher[1] = (cipher[1] - ((((cipher[0] << 4) + key[2]) % (1L<<32)) ^ ((cipher[0] + sum) % (1L<<32)) ^ (((cipher[0] >> 5) + key[3]) % (1L<<32)))) % (1L<<32);
        cipher[0] = (cipher[0] - ((((cipher[1] << 4) + key[0]) % (1L<<32)) ^ ((cipher[1] + sum) % (1L<<32)) ^ (((cipher[1] >> 5) + key[1]) % (1L<<32)))) % (1L<<32);
        sum = (sum - delta) % (1L<<32);
    }
    plain[0] = cipher[0];
    plain[1] = cipher[1];
    return;
}

void* thread_encrypt(void* arg) {

    struct arguments* flag = (struct arguments*)arg;

    int i = flag->start;
    int n = flag->end;

    for (; i < n; i++) {
        uint64_t tmp1 = i ^ flag->nonce;

        uint32_t tmp3[2];
        tmp3[0] = ((int32_t *) &tmp1)[0]; 
        tmp3[1] = ((int32_t *) &tmp1)[1];

        encrypt_tea(tmp3, tmp3, flag->key);
        flag->tmp2[i] = *(uint64_t*)tmp3;
        flag->cipher[i] = flag->plain[i] ^ *(uint64_t*)tmp3;
    }
    free(flag);

    return NULL;
}

void btree_encrpyt(uint64_t * plain, uint32_t key[4], uint64_t nonce, uint64_t * cipher, uint32_t num_blocks, void * helper, uint64_t* tmp2) {

    struct btree* my_tree = (struct btree*)helper;

    int block_num;
    if (num_blocks < my_tree->n_processors) {
        block_num = ((num_blocks + (my_tree->n_processors-1))/my_tree->n_processors);
    } else {
        block_num = (num_blocks/my_tree->n_processors);
    }
    int start = 0;
    int end = block_num;

    int i = 0;
    for (; i < my_tree->n_processors; i++) {

        struct arguments* args = malloc(sizeof(struct arguments));
        args->plain = plain;
        args->nonce = nonce;
        args->cipher = cipher;
        args->start = start;
        args->end = end;
        args->tmp2 = tmp2;
        memmove(args->key, key, sizeof(uint32_t)*4);

        if (end > num_blocks) {
            free(args);
            break;
        }
        pthread_create(&my_tree->pool[i], NULL, &thread_encrypt, args);

        start += block_num;
        end += block_num;
        if ((i == my_tree->n_processors-1) && end < num_blocks) {
            end += 1;
        }
    }   

    for (int j = 0; j < i; j++) {
        pthread_join(my_tree->pool[j], NULL);
    }   
    return;
}

void* thread_decrypt(void* arg) {

    struct arguments* flag = (struct arguments*)arg;

    int i = flag->start;
    int n = flag->end;

    for (; i < n; i++) {
        uint64_t tmp1 = i ^ flag->nonce;
  
        uint32_t tmp3[2];
        tmp3[0] = ((int32_t *) &tmp1)[0]; 
        tmp3[1] = ((int32_t *) &tmp1)[1];

        encrypt_tea(tmp3, tmp3, flag->key);
        flag->plain[i] = flag->cipher[i] ^ *(uint64_t*)tmp3;
    }
    free(flag);
    return NULL;
}

void btree_decryption(uint64_t * cipher, uint32_t key[4], uint64_t nonce, uint64_t * plain, uint32_t num_blocks, void* helper) {

    struct btree* my_tree = (struct btree*)helper;

    int block_num;
    if (num_blocks < my_tree->n_processors) {
        block_num = ((num_blocks + (my_tree->n_processors-1))/my_tree->n_processors);
    } else {
        block_num = (num_blocks/my_tree->n_processors);
    }
    int start = 0;
    int end = block_num;

    int i = 0;
    for (; i < my_tree->n_processors; i++) {

        struct arguments* args = malloc(sizeof(struct arguments));
        args->plain = plain;
        args->nonce = nonce;
        args->cipher = cipher;
        args->start = start;
        args->end = end;
        memmove(args->key, key, sizeof(uint32_t)*4);

        if (end > num_blocks) {
            free(args);
            break;
        }
        pthread_create(&my_tree->pool[i], NULL, &thread_decrypt, args);

        start += block_num;
        end += block_num;
        if ((i == my_tree->n_processors-1) && end < num_blocks) {
            end += 1;
        }
    }   
    for (int j = 0; j < i; j++) {
        pthread_join(my_tree->pool[j], NULL);
    }   
    return;
}

void my_tea_ctr(uint64_t * plain, uint32_t key[4], uint64_t nonce, uint64_t * cipher, uint32_t num_blocks, uint64_t* tmp2) {

    for (int i = 0; i < num_blocks; i++) {
        uint64_t tmp1 = i ^ nonce;

        uint32_t tmp3[2];
        tmp3[0] = ((int32_t *) &tmp1)[0]; 
        tmp3[1] = ((int32_t *) &tmp1)[1];

        encrypt_tea(tmp3, tmp3, key);
        tmp2[i] = *(uint64_t*)tmp3;
        cipher[i] = plain[i] ^ *(uint64_t*)tmp3;
    }
    return;
}

void encrypt_tea_ctr(uint64_t * plain, uint32_t key[4], uint64_t nonce, uint64_t * cipher, uint32_t num_blocks) {

    for (int i = 0; i < num_blocks; i++) {
        uint64_t tmp1 = i ^ nonce;

        uint32_t tmp3[2];
        tmp3[0] = ((int32_t *) &tmp1)[0]; 
        tmp3[1] = ((int32_t *) &tmp1)[1];

        encrypt_tea(tmp3, tmp3, key);
        cipher[i] = plain[i] ^ *(uint64_t*)tmp3;
    }
    return;
}

void decrypt_tea_ctr(uint64_t * cipher, uint32_t key[4], uint64_t nonce, uint64_t * plain, uint32_t num_blocks) {
    
    for (int i = 0; i < num_blocks; i++) {
        uint64_t tmp1 = i ^ nonce;

        uint32_t tmp3[2];
        tmp3[0] = ((int32_t *) &tmp1)[0]; 
        tmp3[1] = ((int32_t *) &tmp1)[1];

        encrypt_tea(tmp3, tmp3, key);
        plain[i] = cipher[i] ^ *(uint64_t*)tmp3;
    }
    return;
}
