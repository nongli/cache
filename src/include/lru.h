#pragma once
/*
 * Implements a LRU cache, which in turn is necessary when building ARC.
 */
#include <cassert>
#include <memory>
#include <unordered_map>

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
  LRULink<K, V> *prev;
  LRULink<K, V> *next;

  // Adding a constructor for convenience.
  LRULink(K k, std::shared_ptr<V> v)
      : key{k}, value{std::move(v)}, prev{NULL}, next{NULL} {}
  // No copy constructor.
  LRULink(const LRULink &) = delete;
  // No assignment.[
  LRULink operator=(const LRULink &) = delete;
};

// The actual LRU link list. Elements enter by being inserted at the head
// and age out as they fall to the tail. Elements are removed from the tail.
template <typename K, typename V> class LRUList {
public:
  LRUList() : _head{NULL}, _tail{NULL}, _length{0} {}

  LRULink<K, V> *peek_head() const { return _head; }

  LRULink<K, V> *peek_tail() const { return _tail; }

  size_t size() const { return _length; }

  // FIXME: Worry about concurrency
  void insert_head(LRULink<K, V> *entry) {
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

  LRULink<K, V> *remove_tail() {
    LRULink<K, V> *ret = _tail;
    if (ret) {
      _tail = ret->prev;
      if (_tail) {
        _tail->next = NULL;
      } else {
        assert(_length == 1);
        _head = NULL;
      }
      ret->next = ret->prev = NULL;
      _length--;
    }
    return ret;
  }

  // When accessing an element move it to the head to allow it to survive.
  // ASSUMES elt is in list.
  void move_to_head(LRULink<K, V> *elt) {
    assert(_head && _tail && _length > 0); // Cannot be an empty list.
    if (elt->prev && elt->next) {
      // Middle of the list
      assert(_length > 1 && _head != elt && _tail != elt);
      elt->next->prev = elt->prev;
      elt->prev->next = elt->next;
      _head->prev = elt;
      elt->next = _head;
      elt->prev = NULL;

      _head = elt;
    } else if (elt->prev) {
      // Tail of list, but not head
      assert(_tail == elt && _head != elt);
      _tail = elt->prev;
      elt->prev->next = NULL;
      _head->prev = elt;
      elt->next = _head;
      elt->prev = NULL;
      _head = elt;
    } else {
      // elt is already head.
      assert(_head == elt);
    }
  }

  LRUList(const LRUList &) = delete;
  LRUList operator=(const LRUList &) = delete;

private:
  LRULink<K, V> *_head;
  LRULink<K, V> *_tail;
  size_t _length;
};
} // namespace cache
