#ifndef BTREESTORE_H
#define BTREESTORE_H

#include <stdint.h>
#include <stddef.h>
#include <pthread.h>
#include <stdlib.h>

struct info {

    uint32_t size;
    uint32_t key[4];
    uint64_t nonce;
    void * data;
};

struct node {

    uint16_t num_keys;
    uint32_t * keys;
};

struct dict {

    size_t size; //Size of stored data in bytes
    uint32_t encrypt_key[4]; //Encryption key
    uint64_t nonce; //Nonce data
    void * data; //Encrypted stored data
    uint64_t * tmp2;
    
    uint32_t key;
};

struct btree_node {

    int child_count;
    int link_count;
    
    char leaf;

    struct btree_node** children;
    struct dict** key_values;
    struct btree_node* parent;


};

struct btree {

    uint16_t branching;
    uint8_t n_processors;

    pthread_mutex_t mutex;
    pthread_t* pool;
    struct btree_node* root;

    uint32_t node_count;
    uint32_t largest_key;
};

struct arguments {

    uint64_t * plain;
    uint32_t key[4];
    uint64_t nonce;
    uint64_t * cipher;
    uint64_t * tmp2;

    int start;
    int end;
};


void * init_store(uint16_t branching, uint8_t n_processors);

void close_store(void * helper);

int btree_insert(uint32_t key, void * plaintext, size_t count, uint32_t encryption_key[4], uint64_t nonce, void * helper);

int btree_retrieve(uint32_t key, struct info * found, void * helper);

int btree_decrypt(uint32_t key, void * output, void * helper);

int btree_delete(uint32_t key, void * helper);

uint64_t btree_export(void * helper, struct node ** list);

void encrypt_tea(uint32_t plain[2], uint32_t cipher[2], uint32_t key[4]);

void decrypt_tea(uint32_t cipher[2], uint32_t plain[2], uint32_t key[4]);

void encrypt_tea_ctr(uint64_t * plain, uint32_t key[4], uint64_t nonce, uint64_t * cipher, uint32_t num_blocks);

void decrypt_tea_ctr(uint64_t * cipher, uint32_t key[4], uint64_t nonce, uint64_t * plain, uint32_t num_blocks);

void btree_encrpyt(uint64_t * plain, uint32_t key[4], uint64_t nonce, uint64_t * cipher, uint32_t num_blocks, void * helper, uint64_t* tmp2);

void btree_decryption(uint64_t * cipher, uint32_t key[4], uint64_t nonce, uint64_t * plain, uint32_t num_blocks, void* helper);

void my_tea_ctr(uint64_t * plain, uint32_t key[4], uint64_t nonce, uint64_t * cipher, uint32_t num_blocks, uint64_t* tmp2);

#endif
