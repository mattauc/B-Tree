#include "btreestore.h"

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

struct args {

    void* helper;
    int key;
};

void read_export(struct node* list, int count) {

    for (int i = 0; i < count; i++) {
        for (int j = 0; j < list[i].num_keys; j++) {
            printf("%d ",list[i].keys[j]);
        }
        free(list[i].keys);
        printf("\n");
    }
    free(list);
}

void read_retrieve(struct info* output) {

    printf("SIZE: %d\n", output->size);
    printf("NONCE: %d\n", output->nonce);
    printf("KEY: ");
    for(int i = 0; i < 4; i++) {
        printf("%d ",output->key[i]);
    }
    printf("\n");
    printf("DATA: ");
    for (int i = 0; i < output->size; i++) {
        printf("%d ", output->data + i);
    }
    printf("\n");
}

/*
* Basic insert, export and close_store functionality test from keys 0->50
*/
void insert1() {

    void * helper = init_store(4, 4);

    uint64_t plaintext[10];
    uint32_t enc_key[4];
    struct node* list = NULL;

    for (int i = 0; i < 50; i++) {
        btree_insert(i, plaintext, 10, enc_key, 5, helper);
    }

    int count = btree_export(helper,&list);
    read_export(list, count);
    close_store(helper);

}

/*
* Basic insert, export and close_store functionality test from keys 50->0
*/
void insert2() {

    void * helper = init_store(4, 4);

    uint64_t plaintext[10];
    uint32_t enc_key[4];
    struct node* list = NULL;

    for (int i = 50; i > 0; i--) {
        btree_insert(i, plaintext, 10, enc_key, 5, helper);
    }

    int count = btree_export(helper,&list);
    read_export(list, count);
    close_store(helper);

}

/*
* Basic insert, export and close_store functionality test from keys 0->15 with b = 5
*/
void insert3() {

    void * helper = init_store(5, 4);

    uint64_t plaintext[10];
    uint32_t enc_key[4];
    struct node* list = NULL;

    for (int i = 0; i < 15; i++) {
        btree_insert(i, plaintext, 10, enc_key, 5, helper);
    }

    int count = btree_export(helper,&list);
    read_export(list, count);
    close_store(helper);

}

/*
* Basic insert, export and close_store functionality test from keys 0->15 with b = 6
*/
void insert4() {

    void * helper = init_store(6, 4);

    uint64_t plaintext[10];
    uint32_t enc_key[4];
    struct node* list = NULL;

    for (int i = 0; i < 15; i++) {
        btree_insert(i, plaintext, 10, enc_key, 5, helper);
    }

    int count = btree_export(helper,&list);
    read_export(list, count);
    close_store(helper);

}

/*
* Basic insert, export and close_store functionality test from keys 0->15 with b = 3
*/
void insert5() {

    void * helper = init_store(3, 4);

    uint64_t plaintext[10];
    uint32_t enc_key[4];
    struct node* list = NULL;

    for (int i = 0; i < 15; i++) {
        btree_insert(i, plaintext, 10, enc_key, 5, helper);
    }

    int count = btree_export(helper,&list);
    read_export(list, count);
    close_store(helper);

}

/*
* Basic insert, export and close_store functionality test from keys 0->15 with b = 7
*/
void insert6() {

    void * helper = init_store(7, 4);

    uint64_t plaintext[10];
    uint32_t enc_key[4];
    struct node* list = NULL;

    for (int i = 0; i < 50; i++) {
        btree_insert(i, plaintext, 10, enc_key, 5, helper);
    }

    int count = btree_export(helper,&list);
    read_export(list, count);
    close_store(helper);

}

/*
* Randomly generated keys; this tests for potential errors with random, unordered numbers
*/
void insert_random1() {

    void * helper = init_store(4, 4);

    uint64_t plaintext[10];
    uint32_t enc_key[4];
    struct node* list = NULL;

    int i = 0;
    for(; i < 500; i++) {
        btree_insert(rand(), plaintext, 10, enc_key, 5, helper);
    }

    if (i != 500) {
        printf("Insert was interrupted!\n");
    }
    close_store(helper);
}

/*
* Very large insert and delete functionality. Checks that btree can handle large data
*/
void large_insert1() {

    void * helper = init_store(4, 4);

    uint64_t plaintext[10];
    uint32_t enc_key[4];
    struct node* list = NULL;

    for (int i = 0; i < 400000; i++) {
        btree_insert(i, plaintext, 10, enc_key, 5, helper);
    }

    for (int i = 0; i < 400000; i++) {
        btree_delete(i, helper);
    }
    long count = btree_export(helper, &list);
    if (count != 0 ) {
        printf("Incorrect node/key count!\n");
    }
    close_store(helper);
}

/*
* Tests whether you can insert, then delete and then re-insert keys into the tree 
*/
void insert_delete1() {

    void * helper = init_store(4, 4);

    uint64_t plaintext[10];
    uint32_t enc_key[4];
    struct node* list1 = NULL;
    struct node* list2 = NULL;

    for (int i = 0; i < 20; i++) {
        btree_insert(i, plaintext, 10, enc_key, 5, helper);
    }
    int count1 = btree_export(helper,&list1);
    read_export(list1, count1);

    printf("DELETING:\n");
    
    btree_delete(17, helper);
    btree_delete(4, helper);
    btree_delete(2, helper);
    btree_delete(7, helper);

    btree_insert(2, plaintext, 10, enc_key, 5, helper);
    btree_insert(7, plaintext, 10, enc_key, 5, helper);
    

    int count2 = btree_export(helper,&list2);
    read_export(list2, count2);
    close_store(helper);
}

/*
* Checks all potential insert errors
*/
void insert_error1() {

    void * helper = init_store(4, 4);
    uint64_t plaintext[10];
    uint32_t enc_key[4];

    int ret1 = btree_insert(6, plaintext, 10, enc_key, 5, helper);
    if (ret1 != 0) {
        printf("Return value: %d\n",ret1);
    }
    int ret2 = btree_insert(6, plaintext, 10, enc_key, 5, helper);
    if (ret2 != 0) {
        printf("Return value: %d\n",ret2);
    }
    close_store(helper);
}

/*
* Checks basic receive output
*/
void retrieve1() {

    void * helper = init_store(4, 4);

    uint64_t plaintext[10];
    for (int i = 0; i < 10; i++) {
        plaintext[i] = i;
    }
    uint32_t enc_key[4];
    for (int i = 0; i < 4; i++) {
        enc_key[i] = i*2;
    }

    for (int i = 0; i < 15; i++) {
        btree_insert(i, plaintext, 10, enc_key, 5, helper);
    }
    struct info* output = malloc(sizeof(struct info));
    int ret = btree_retrieve(6, output, helper);
    if (ret != 0) {
        printf("Return value: %d\n", ret);
    } else {
        read_retrieve(output);
    }
    free(output);

    close_store(helper);
}

/*
* Checks all potential receive errors
*/
void retrieve_error1() {

    void * helper = init_store(4, 4);

    struct info* output = malloc(sizeof(struct info));
    int ret = btree_retrieve(6, output, helper);
    if (ret != 0) {
        printf("Return value: %d\n", ret);
    } else {
        read_retrieve(output);
    }
    free(output);
    close_store(helper);
}

/*
* Checks basic decrypt output
*/
void decrypt1() {

    void * helper = init_store(4, 4);

    char plaintext[20];
    for (int i = 0; i < 20; i++) {
        plaintext[i] = i;
    }
    uint32_t enc_key[4];
    for (int i = 0; i < 4; i++) {
        enc_key[i] = i*2;
    }

    for (int i = 0; i < 15; i++) {
        btree_insert(i, plaintext, 20, enc_key, 5, helper);
    }
    struct info* output1 = malloc(sizeof(struct info));
    int ret1 = btree_retrieve(6, output1, helper);
    if (ret1 != 0) {
        printf("Return value: %d\n", ret1);
    } else {
        read_retrieve(output1);
    }
    free(output1);
    char output2[20];
    int ret2 = btree_decrypt(6, output2, helper);
    if (ret2 != 0) {
        printf("Return value: %d\n", ret2);
    } else {
        printf("DECRYPTED: ");
        for (int i = 0; i < 20; i++) {
            printf("%d ", output2[i]);
        }
        printf("\n");
    }
    close_store(helper);
}

/*
* This tests the multithreading aspect of encryption with a very large plaintext
*/
void large_plaintext1() {

    void * helper = init_store(4, 4);
    char plaintext[100000];
    for (int i = 0; i < 100000; i++) {
        plaintext[i] = i;
    }
    uint32_t enc_key[4];
    for (int i = 0; i < 4; i++) {
        enc_key[i] = i*2;
    }
    for (int i = 0; i < 15; i++) {
        btree_insert(i, plaintext, 100000, enc_key, 5, helper);
    }
    struct info* output1 = malloc(sizeof(struct info));
    int ret1 = btree_retrieve(6, output1, helper);
    if (ret1 != 0) {
        printf("Return value: %d\n", ret1);
    } 
    free(output1);
    char output2[100000];
    int ret2 = btree_decrypt(6, output2, helper);
    if (ret2 != 0) {
        printf("Return value: %d\n", ret2);
    } 
    close_store(helper);
}

/*
* Checks all potential decrypt errors
*/
void decrypt_error1() {

    void * helper = init_store(3, 4);
    char plaintext[10];
    for (int i = 0; i < 10; i++) {
        plaintext[i] = i;
    }
    uint32_t enc_key[4];
    for (int i = 0; i < 4; i++) {
        enc_key[i] = i*2;
    }
    char output2[10];
    int ret2 = btree_decrypt(6, output2, helper);
    if (ret2 != 0) {
        printf("Return value: %d\n", ret2);
    } 
    close_store(helper);
}

/*
* Checks basic delete functionality with keys from 0->100 and b = 8
*/
void delete1() {

    void * helper = init_store(8, 4);

    uint64_t plaintext[10];
    uint32_t enc_key[4];
    struct node* list = NULL;

    for (int i = 0; i < 100; i++) {
        btree_insert(i, plaintext, 10, enc_key, 5, helper);
    }

    for (int i = 0; i < 100; i++) {
        btree_delete(i, helper);
    }

    int count = btree_export(helper,&list);
    if (count != 0) {
        printf("Return value: %d\n", count);
    }
    close_store(helper);

}

/*
* Checks basic delete functionality with keys from 100->0 and b = 9
*/
void delete2() {

    void * helper = init_store(9, 4);

    uint64_t plaintext[10];
    uint32_t enc_key[4];
    struct node* list = NULL;

    for (int i = 100; i > 0; i--) {
        btree_insert(i, plaintext, 10, enc_key, 5, helper);
    }

    for (int i = 100; i > 0; i--) {
        btree_delete(i, helper);
    }

    int count = btree_export(helper,&list);
    if (count != 0) {
        printf("Return value: %d\n", count);
    }
    close_store(helper);

}

/*
* Checks for all potential delete errors
*/
void delete_error1() {

    close_store(NULL);
    void * helper = init_store(4, 4);
    int ret = btree_delete(5, helper);
    if (ret != 0) {
        printf("Return value: %d\n", ret);
    }
    uint64_t plaintext[10];
    uint32_t enc_key[4];

    btree_insert(5, plaintext, 10, enc_key, 5, helper);
    int ret1 = btree_delete(6, helper);
    if (ret1 != 0) {
        printf("Return value: %d\n", ret1);
    }
    close_store(helper);
}

/*
* Multithread inserter helper
*/
void* thread_inserter(void* args) {

    struct args* flag = (struct args*)args;

    char plaintext[10];
    uint32_t enc_key[4];
    btree_insert(flag->key, plaintext, 10, enc_key, 5, flag->helper);

    free(args);
    return NULL;
}

/*
* Multithread deleter helper
*/
void* thread_deleter(void* args) {

    struct args* flag = (struct args*)args;

    btree_delete(flag->key, flag->helper);
    free(args);
    return NULL;
}

/*
* Multithread retrieve and decrypt helper
*/
void* thread_reader(void* args) {

    struct args* flag = (struct args*)args;

    struct info* output = malloc(sizeof(struct info));
    int ret1 = btree_retrieve(flag->key, output, flag->helper);
    if (ret1 != 0) {
        printf("Return value: %d\n",ret1);
    }
    free(output);

    char output1[10];
    int ret2 = btree_decrypt(flag->key, output1, flag->helper);
    if (ret2 != 0) {
        printf("Return value: %d\n",ret2);
    }
    free(args);
    return NULL;
}

/*
* Tests for thread safety with retrieve/decrypt/insert/delete
*/
void multithread1() {

    pthread_t th[5000];

    void * helper = init_store(4, 4);
    
    for (int i = 0; i < 5000; i++) {
        struct args* num = malloc(sizeof(struct args));
        num->key = i;
        num->helper = helper;
        pthread_create(&th[i], NULL, &thread_inserter, num);
    }
    for (int i = 0; i < 5000; i++) {
        pthread_join(th[i], NULL);
    }

    for (int i = 0; i < 1000; i++) {
        struct args* num = malloc(sizeof(struct args));
        num->key = i;
        num->helper = helper;
        pthread_create(&th[i], NULL, &thread_reader, num);
    }
    for (int i = 0; i < 1000; i++) {
        pthread_join(th[i], NULL);
    }

    for (int i = 0; i < 5000; i++) {
        struct args* num = malloc(sizeof(struct args));
        num->key = i;
        num->helper = helper;
        pthread_create(&th[i], NULL, &thread_inserter, num);
    }
    for (int i = 0; i < 5000; i++) {
        pthread_join(th[i], NULL);
    }
    close_store(helper);
}


int main(int argc, char* argv[]) {

    if (argc == 1) {
        fprintf(stderr, "No arguments!\n");
        exit(1);
    }

    if (argv[1][0] == '1') {
        large_insert1();
    } else if (argv[1][0] == '2') {
        insert1();
    } else if (argv[1][0] == '3') {
        insert2();
    } else if (argv[1][0] == '4') {
        insert_delete1();
    } else if (argv[1][0] == '5') {
        insert3();
    } else if (argv[1][0] == '6') {
        insert4();
    } else if (argv[1][0] == '7') {
        insert5();
    } else if (argv[1][0] == '8') {
        insert6();
    } else if (argv[1][0] == '9') {
        insert_random1();
    } else if (argv[1][0] == 'a') {
        retrieve1();
    } else if (argv[1][0] == 'b') {
        retrieve_error1();
    } else if (argv[1][0] == 'c') {
        decrypt1();
    } else if (argv[1][0] == 'd') {
        decrypt_error1();
    } else if (argv[1][0] == 'e') {
        insert_error1();
    } else if (argv[1][0] == 'f') {
        large_plaintext1();
    } else if (argv[1][0] == 'g') {
        delete1();
    } else if (argv[1][0] == 'h') {
        delete2();
    } else if (argv[1][0] == 'i') {
        delete_error1();
    } else if (argv[1][0] == 'j') {
        multithread1();
    } 
    return 0;
}

