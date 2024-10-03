#include "hashmap.hpp"
#include <stdio.h>
#include <stdlib.h>


HashMap::HashMap(int sz) {
    size = sz;
    table = new HashNode*[sz];
    for (int i = 0; i < sz; i++) table[i] = NULL;
}

int HashMap::get(ll key, int vsign) {
    int pos = 1ll * key % size;
    HashNode *pointer = table[pos];
    while (pointer != NULL) {
        if (pointer->data.key == key)
            return pointer->data.val;
        else
            pointer = pointer->next;
    }
    return vsign;
}

void HashMap::erase(ll key) {
    int pos = 1ll * key % size;
    HashNode *pointer = table[pos];
    if (pointer == NULL) return;
    if (pointer->data.key == key) {
        table[pos] = pointer->next;
        delete pointer;
        return;
    }
    HashNode *prev = pointer;
    pointer = pointer->next;
    while (pointer != NULL) {
        if (pointer->data.key == key) {
            prev->next = pointer->next;
            delete pointer;
            return;
        }
        prev = pointer;
        pointer = pointer->next;    
    };
}

void HashMap::insert(ll key, int value) {
    HashNode *hnode = new HashNode();
    hnode->data.key = key;
    hnode->data.val = value;
    hnode->next = NULL;

    int pos = 1ll * key % size;
    if (table[pos] == NULL) {
        table[pos] = hnode;
        return;
    }
    HashNode *pointer = table[pos];
    HashNode *prev = pointer;
    while (pointer != NULL) {
        if (pointer->data.key == key) {
            pointer->data.val = value;
            delete hnode;
            return;
        } 
        prev = pointer;
        pointer = pointer->next; 
    } 
    prev->next = hnode;
}

HashMap::~HashMap(){
    for (int i = 0; i < size; i++) {
        HashNode *pointer = table[i];
        HashNode *prev = pointer;
        while (pointer != NULL){    
            prev = pointer;
            pointer = pointer->next;
            delete prev;
        }
    }
    delete []table;
}