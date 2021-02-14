#include "SimpleLRU.h"

namespace Afina {
namespace Backend {

SimpleLRU::~SimpleLRU() {
    _lru_index.clear();
    while(_lru_head != nullptr) {
        auto tmp = std::move(_lru_head->next);
        _lru_head.reset();
        std::swap(_lru_head, tmp);
    }
}

bool SimpleLRU::DeleteLruTail() {
    if (_lru_tail == nullptr) {
        return false;
    }
    return Delete(_lru_tail->key);
}

bool SimpleLRU::ProvideSpace(size_t space_required) {
    while (_max_size - _current_size < space_required) {
        if (!DeleteLruTail()) {
            return false;
        }
    }
    return true;
}

bool SimpleLRU::MoveNodeToHead(const std::string& key) {
    auto node = _lru_index.find(std::ref(key))->second;
    if (node.get().prev == nullptr) {
        return true;
    }
    if (node.get().next == nullptr) {
        auto prev = node.get().prev;
        _lru_head->prev = &node.get();
        _lru_tail = prev;
        node.get().next = std::move(_lru_head);
        _lru_head = std::move(_lru_tail->next);
        _lru_tail->next = nullptr;
        node.get().prev = nullptr;
        return true;
    }
    auto prev = node.get().prev;
    node.get().next->prev = prev;
    std::swap(_lru_head, prev->next);
    std::swap(prev->next, node.get().next);
    node.get().prev = nullptr;
    node.get().next->prev = &(node.get());
    return true;
}

// See MapBasedGlobalLockImpl.h
bool SimpleLRU::Put(const std::string &key, const std::string &value) {
    auto wrapped_key = std::ref(key);
    auto map_it = _lru_index.find(wrapped_key);
    if (map_it != _lru_index.end()) {
        std::size_t node_size = key.size() + value.size();
        if (node_size > _max_size) {
            return false;
        }

        MoveNodeToHead(key);

        if (_lru_head->value.size() < value.size()) {
            ProvideSpace(value.size() - _lru_head->value.size());
        }
        _current_size -= _lru_head->value.size() - value.size();
        _lru_head->value = value;
        return true;
    }
    return PutIfAbsent(key, value);
}

// See MapBasedGlobalLockImpl.h
bool SimpleLRU::PutIfAbsent(const std::string &key, const std::string &value) {

    if (_lru_index.find(std::ref(key)) != _lru_index.end()) {
        return false;
    }
    std::size_t node_size = key.size() + value.size();
    if (node_size > _max_size) {
        return false;
    }
    if (_max_size - _current_size < node_size) {
        ProvideSpace(node_size);
    }

    auto node = new lru_node{key, value, nullptr, std::move(_lru_head)};
    _current_size += node_size;
    _lru_index.emplace(std::make_pair(std::ref(node->key), std::ref(*node)));
    _lru_head = std::unique_ptr<lru_node>(node);
    if (node->next == nullptr) {
        _lru_tail = node;
    } else {
        node->next->prev = _lru_head.get();
    }
    return true;
}

// See MapBasedGlobalLockImpl.h
bool SimpleLRU::Set(const std::string &key, const std::string &value) {
    if (_lru_index.find(std::ref(key)) == _lru_index.end()) {
        return false;
    }
    if (value.size() > _max_size) {
        return false;
    }

    MoveNodeToHead(key);
    if (_lru_head->value.size() < value.size()) {
        ProvideSpace(value.size() - _lru_head->value.size());
    }
    _current_size -= _lru_head->value.size() - value.size();
    _lru_head->value = value;
    return true;
}

// See MapBasedGlobalLockImpl.h
bool SimpleLRU::Delete(const std::string &key) {
    auto pos = _lru_index.find(std::ref(key));
    if (pos == _lru_index.end()) {
        return false;
    }
    auto node = pos->second;
    _lru_index.erase(pos);
    _current_size -= node.get().value.size() + node.get().key.size();
    
    // if there are only one node in the list
    if (_lru_head->next.get() == nullptr) {
        _lru_head = nullptr;
        _lru_tail = nullptr;
        return true;
    }

    // Node is in the head
    if (node.get().prev == nullptr) {
        std::unique_ptr<lru_node> new_head;
        new_head = std::move(_lru_head->next);
        new_head->prev = nullptr;
        _lru_head->next = nullptr;
        std::swap(_lru_head, new_head);
        return true;
    }

    // Node is in the tail
    if (node.get().next == nullptr) {
        lru_node* new_tail = _lru_tail->prev;
        new_tail->next = nullptr;
        _lru_tail = new_tail;
        return true;
    }

    auto prev = node.get().prev;
    node.get().next->prev = prev;
    auto tmp_ptr = std::move(node.get().next);
    node.get().next = nullptr;
    prev->next = std::move(tmp_ptr);
    return true;
}

// See MapBasedGlobalLockImpl.h
bool SimpleLRU::Get(const std::string &key, std::string &value) {
    auto pos = _lru_index.find(std::ref(key));
    if (pos == _lru_index.end()) {
        return false;
    }

    MoveNodeToHead(key);
    value = _lru_head->value;
    return true;
}

} // namespace Backend
} // namespace Afina
