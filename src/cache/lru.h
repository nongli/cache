#pragma once

/*
 * Implements a LRU cache, which in turn is necessary when building ARC.
 */
#include <cassert>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <tuple>
#include <unordered_map>

#include "cache/cache.h"

// FIXME: Maybe move to different namespace?
namespace cache {

// The LRU itself is a linked list of entries also present in HashMap. The
// hashmap allows quick lookup, the linked list allows tracking. However, we
// need some linked list operations that std::list does not seem to offer,
// hence building it in here.
template <typename K, typename V> struct LRULink {
  K key;
  // FIXME: Figure out what memory management semantics we
  // prefer. It is important to be able to
  // (a) Have a link without a valid value: this is necessary for ghost
  // lists.
  // (b) Hand out values, and then invalidate without causing someone with
  // a reference from failing.
  //
  // Given this shared_ptr seemed good, but there are obvious issues with
  // this.
  std::shared_ptr<V> value;
  LRULink<K, V>* prev;
  LRULink<K, V>* next;

  // Adding a constructor for convenience.
  LRULink(K k, std::shared_ptr<V> v)
      : key{k}, value{std::move(v)}, prev{nullptr}, next{nullptr} {}
  // No copy constructor.
  LRULink(const LRULink&) = delete;
  // No assignment.
  LRULink operator=(const LRULink&) = delete;
  // Yes move
  LRULink(LRULink&&) = default;
};

// The actual LRU link list. Elements enter by being inserted at the head
// and age out as they fall to the tail. Elements are removed from the tail.
template <typename K, typename V> class LRUList {
public:
  LRUList() : _head{nullptr}, _tail{nullptr}, _length{0} {}

  LRULink<K, V>* peek_head() const { return _head; }

  LRULink<K, V>* peek_tail() const { return _tail; }

  int64_t size() const { return _length; }

  // FIXME: Worry about concurrency

  // Insert entry into head.
  inline void insert_head(LRULink<K, V>* entry) {
    entry->next = _head;
    if (_head) {
      assert(!_head->prev);
      _head->prev = entry;
    }

    _head = entry;

    if (!_tail) {
      assert(_length == 0);
      _tail = entry;
    }

    _length++;
  }

  // Remove the tail entry, this is essentially aging out
  // an entry.
  LRULink<K, V>* remove_tail() {
    LRULink<K, V>* ret = _tail;
    if (ret) {
      _tail = ret->prev;
      if (_tail) {
        _tail->next = nullptr;
      } else {
        assert(_length == 1);
        _head = nullptr;
      }
      ret->next = ret->prev = nullptr;
      _length--;
    }
    return ret;
  }

  // Remove an arbitrary entry.
  // ASSUMES: entry in list.
  inline void remove(LRULink<K, V>* entry) {
    if (entry->prev) {
      entry->prev->next = entry->next;
    } else {
      assert(_head == entry);
      _head = entry->next;
    }

    if (entry->next) {
      entry->next->prev = entry->prev;
    } else {
      assert(_tail == entry);
      _tail = entry->prev;
    }
    entry->next = entry->prev = nullptr;
    _length--;
  }

  // When accessing an element move it to the head to allow it to survive.
  // ASSUMES elt is in list.
  void move_to_head(LRULink<K, V>* elt) {
    assert(_head && _tail && _length > 0); // Cannot be an empty list.
    if (elt != _head) {
      remove(elt);
      insert_head(elt);
    }
  }

  void clear() {
    _head = nullptr;
    _tail = nullptr;
    _length = 0;
  }

  LRUList(const LRUList&) = delete;
  LRUList operator=(const LRUList&) = delete;

private:
  LRULink<K, V>* _head;
  LRULink<K, V>* _tail;
  int64_t _length;
};

// An LRU cache of fixed size.
template <typename K, typename V, typename Lock = NopLock,
          typename Sizer = ElementCount<V>>
class LRUCache : public Cache<K, V> {
public:
  LRUCache(int64_t size)
      : _max_size{size}, _current_size{0}, _access_list{},
        _access_map{}, _sizer{} {}

  inline int64_t max_size() const { return _max_size; }
  inline int64_t size() const { return _current_size; }
  inline int64_t num_entries() const { return _access_list.size(); }
  const Stats& stats() const { return _stats; }
  inline int64_t p() const { return 0; }
  inline int64_t max_p() const { return 0; }
  inline int64_t filter_size() const { return 0; }

  // Get value from the cache. If found bumps the element up in the LRU list.
  std::shared_ptr<V> get(const K& key) {
    std::lock_guard<Lock> l(_lock);
    auto elt = _access_map.find(key);
    if (elt != _access_map.end()) {
      ++_stats.num_hits;
      _stats.bytes_hit += _sizer(elt->second.value.get());
      _access_list.move_to_head(&elt->second);
      return elt->second.value;
    } else {
      ++_stats.num_misses;
      return nullptr;
    }
  }

  // Check if value is in the cache. This is useful for things like ghost caches
  // where we don't have real values. Note we do bump the page up for contains,
  // this matches what ARC states in Figure 4.
  inline bool contains(const K& key) {
    std::lock_guard<Lock> l(_lock);
    auto elt = _access_map.find(key);
    if (elt != _access_map.end()) {
      _access_list.move_to_head(&elt->second);
      return true;
    } else {
      return false;
    }
  }

  // Insert element into cache without eviction.
  // If the same key is used then we replace the value.
  inline void add_to_cache_no_evict(const K& key, std::shared_ptr<V> value) {
    std::lock_guard<Lock> l(_lock);
    add_to_cache_no_evict_impl(key, value);
  }

  // Evict an entry and return the evicted entry's key.
  inline std::optional<K> evict_entry(size_t& evicted_size) {
    std::lock_guard<Lock> l(_lock);
    return evict_entry_impl(evicted_size);
  }

  // Evict an entry and return the evicted entry's key.
  inline std::optional<K> evict_entry() {
    std::lock_guard<Lock> l(_lock);
    size_t r;
    return evict_entry_impl(r);
  }

  // Insert element into the cache. Might evict a cache element if necessary.
  // If the same key is used then we replace the value.
  // Returns size of EVicted entries.
  int64_t add_to_cache(const K& key, std::shared_ptr<V> value) {
    // FIXME: Should input be shared_ptr? Not so sure. Revisit.
    // FIXME: Need to notify on eviction, this is something that the ghost
    // lists need. Alternately the no_evict form is enough?
    std::lock_guard<Lock> l(_lock);
    add_to_cache_no_evict_impl(key, value);
    assert(_current_size == _access_map.size());
    int64_t before = _current_size;
    while (_current_size > _max_size) {
      size_t e;
      evict_entry_impl(e);
    }
    // FIXME: Is this ever useful?
    return before - _current_size;
  }

  // Update a cached element if it exists, do nothing otherwise. Boolean returns
  // whether or not value was updated.
  bool update_cache(const K& key, std::shared_ptr<V> value) {
    std::lock_guard<Lock> l(_lock);
    auto elt = _access_map.find(key);
    if (elt != _access_map.end()) {
      _access_list.move_to_head(&elt->second);
      size_t old = _sizer(elt.first->second.value.get());
      size_t nsz = _sizer(value.get());
      elt.first->second.value = value;
      _current_size += (nsz - old);
      return true;
    } else {
      return false;
    }
  }

  // Remove element from cache, return value.
  std::shared_ptr<V> remove_from_cache(const K& key) {
    std::lock_guard<Lock> l(_lock);
    auto elt = _access_map.find(key);
    if (elt != _access_map.end()) {
      _access_list.remove(&elt->second);
      _current_size -= _sizer(elt->second.value.get());
      auto val = std::move(elt->second.value);
      _access_map.erase(elt);
      return val;
    }
    return nullptr;
  }

  // Increase the maximum cache size.
  void increase_size(int64_t delta) { _max_size += delta; }

  // Decrease the maximum cache size.
  void decrease_size(int64_t delta) { _max_size -= delta; }

  void reset() {
    std::lock_guard<Lock> l(_lock);
    _current_size = 0;
    _access_map.clear();
    _access_list.clear();
  }

  void clear() {
    _stats.clear();
    reset();
  }

  // FIXME: Do we want a default size?
  LRUCache() = delete;
  LRUCache(const LRUCache&) = delete;
  LRUCache operator=(const LRUCache&) = delete;

private:
  Lock _lock;
  int64_t _max_size;
  int64_t _current_size;
  LRUList<K, V> _access_list;
  std::unordered_map<K, LRULink<K, V>> _access_map;
  Sizer _sizer;
  Stats _stats;

  // FIXME: We return a key rather than a k,v pair since ARC does not need a
  // value, but is this a good design.
  inline std::optional<K> evict_entry_impl(std::size_t& evicted_size) {
    if (_current_size == 0) {
      return std::nullopt;
    }
    LRULink<K, V>* remove = _access_list.remove_tail();
    std::string key = remove->key;
    evicted_size = _sizer(remove->value.get());
    _current_size -= evicted_size;
    int64_t removed = _access_map.erase(remove->key);
    // We should have no more than one element with the key.
    assert(removed == 1);
    ++_stats.num_evicted;
    _stats.bytes_evicted += evicted_size;
    return key;
  }

  // Insert element into cache without eviction.
  // If the same key is used then we replace the value.
  inline void add_to_cache_no_evict_impl(const K& key,
                                         std::shared_ptr<V> value) {
    // FIXME Maybe move to C++17 where structured binding makes this more
    // pleasant.
    int64_t val = _sizer(value.get());
    auto emplaced = _access_map.emplace(
        std::make_pair(key, std::move(LRULink<K, V>(key, value))));
    if (emplaced.second) {
      _access_list.insert_head(&emplaced.first->second);
      _current_size += val;
    } else {
      _access_list.move_to_head(&emplaced.first->second);
      _current_size -= _sizer(emplaced.first->second.value.get());
      emplaced.first->second.value = value;
      _current_size += val;
    }
  }
};

} // namespace cache
