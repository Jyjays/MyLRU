#include "lru_cache.h"

namespace myLru {


LRUCACHE_TEMPLATE_ARGUMENTS
LRUCACHE::LRUCache(size_t size)
    : max_size_(size), cur_size_(0) {
    head_ = new LRUNode();
    tail_ = new LRUNode();
    head_->next_ = tail_;
    tail_->prev_ = head_;
}

LRUCACHE_TEMPLATE_ARGUMENTS
bool LRUCACHE::Find(const Key& key, Value& value) {
    LRUNode* cur_node;
    if (!hash_table_.Get(key, cur_node)) {
        return false;
    }
    value = cur_node->value_;
    remove_node(cur_node);
    push_node(cur_node);
    return true;
}

LRUCACHE_TEMPLATE_ARGUMENTS
bool LRUCACHE::Insert(const Key& key, Value value) {
    LRUNode *node;
    if (hash_table_.Get(key, node)) {
        LRUNode* new_node = new LRUNode(key, value);
        Remove(key);    // Remove the old node
        push_node(new_node);
        if (!hash_table_.Insert(key, new_node)) {
            delete new_node;
            return false;
        }
        cur_size_++;
        return true;
    }
    LRUNode* new_node = new LRUNode(key,value);
    if (!hash_table_.Insert(key, new_node)) {
        delete new_node;
        return false;
    }
    cur_size_++;
    push_node(new_node);
    if (cur_size_ > max_size_) {
        evict();
    }
    return true;
}

LRUCACHE_TEMPLATE_ARGUMENTS
bool LRUCACHE::Remove(const Key& key) {
    LRUNode* cur_node;
    if (!hash_table_.Get(key, cur_node)) {
        return false;
    }
    remove_node(cur_node);
    hash_table_.Remove(key);
    delete cur_node;
    cur_size_--;
    return true;
}

LRUCACHE_TEMPLATE_ARGUMENTS 
size_t LRUCACHE::Size() {
    return cur_size_;
}

LRUCACHE_TEMPLATE_ARGUMENTS
void LRUCACHE::Clear() {
    LRUNode* cur_node = head_->next_;
    while (cur_node != tail_) {
        LRUNode* next_node = cur_node->next_;
        delete cur_node;
        cur_node = next_node;
    }
    head_->next_ = tail_;
    tail_->prev_ = head_;
    hash_table_.Clear();
    cur_size_ = 0;
}

LRUCACHE_TEMPLATE_ARGUMENTS
void LRUCACHE::Resize(size_t size) {
    if (size < max_size_) {
        while (cur_size_ > size) {
            evict();
        }
    }
    max_size_ = size;
}

LRUCACHE_TEMPLATE_ARGUMENTS
void LRUCACHE::evict(){
    // Single thread version
    LRUNode* last_node = tail_->prev_;
    if (last_node == head_) {
        return;
    }
    remove_node(last_node);
    if (!hash_table_.Remove(last_node->key_)) {
        LRU_ERR("Failed to remove key from hash table");
    }
    cur_size_--;
    delete last_node;
}

LRUCACHE_TEMPLATE_ARGUMENTS
void LRUCACHE::push_node(LRUNode* node) {
    LRU_ASSERT(node != head_ && node != tail_ && node != nullptr, "Invalid node");

    LRUNode* ori_first = head_->next_;
    ori_first->prev_ = node;
    node->next_ = ori_first;
    node->prev_ = head_;
    head_->next_ = node;
}

LRUCACHE_TEMPLATE_ARGUMENTS 
void LRUCACHE::remove_node(LRUNode* node) {
    assert(node != head_ && node != tail_ && node !=nullptr);
    LRUNode* ori_next = node->next_;
    LRUNode* ori_prev = node->prev_;
    ori_next->prev_ = ori_prev;
    ori_prev->next_ = ori_next;
    node->next_ = nullptr;
    node->prev_ = nullptr;
}

template class LRUCache<KeyType, ValueType, HashType, KeyEqualType>;


};  // namespace myLru