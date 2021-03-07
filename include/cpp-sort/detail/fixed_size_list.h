/*
 * Copyright (c) 2020-2021 Morwenn
 * SPDX-License-Identifier: MIT
 */
#ifndef CPPSORT_DETAIL_FIXED_SIZE_LIST_H_
#define CPPSORT_DETAIL_FIXED_SIZE_LIST_H_

////////////////////////////////////////////////////////////
// Headers
////////////////////////////////////////////////////////////
#include <cstddef>
#include <iterator>
#include <memory>
#include <utility>
#include <cpp-sort/utility/as_function.h>
#include "config.h"
#include "memory.h"

namespace cppsort
{
namespace detail
{
    ////////////////////////////////////////////////////////////
    // List node
    //
    // A list node is an immovable structure which contains a
    // value and pointers to the previous and next nodes if any.
    // The value is declared in an anonymous union to ensure
    // that it can be left unconstructed as needed: the list
    // structure itself handles whether a node value should be
    // constructed or destructed.

    template<typename T>
    struct fixed_size_list_node
    {
        constexpr explicit fixed_size_list_node(fixed_size_list_node* next) noexcept:
            next(next)
        {}

        constexpr fixed_size_list_node(fixed_size_list_node* prev, fixed_size_list_node* next) noexcept:
            prev(prev),
            next(next)
        {}

        // Make list nodes immovable
        fixed_size_list_node(const fixed_size_list_node&) = delete;
        fixed_size_list_node(fixed_size_list_node&&) = delete;
        fixed_size_list_node& operator=(const fixed_size_list_node&) = delete;
        fixed_size_list_node& operator=(fixed_size_list_node&&) = delete;

        // The list takes care of managing the lifetime of value
        ~fixed_size_list_node() {}

        // Inhibit construction of the node with a value
        union { T value; };

        // Pointers to the previous and next nodes
        fixed_size_list_node* prev;
        fixed_size_list_node* next;
    };

    ////////////////////////////////////////////////////////////
    // Fixed size node pool
    //
    // This is an immovable doubly linked list node pool that can
    // be constructed with a max capacity (N thereafter): it
    // allocates a contiguous block of memory for N nodes and never
    // performs any other allocation through the course of its
    // lifetime. It is meant to be used from algorithms which know
    // how many elements will be needed in a list or collection of
    // lists, and allows to reuse the list nodes: the contiguous
    // storage and the node reuse are expected to make the code
    // using it faster than equivalent code using std::list.
    //
    // The free nodes are tracked as a singly linked list which
    // reuses a node's internal pointers: that singly linked list
    // of free nodes starts at first_free_ and ends when the "next"
    // field of a node is nullptr.
    //
    // This node pool does not check for overflow: when asking for
    // a new node, an assertion will fire if the pool is out of
    // free nodes.
    //
    // It is worth nothing that the node pool only manages the
    // lifetime of the nodes themselves, it is the responsibility
    // of the list to manage the lifetime of the stored values.

    template<typename T>
    struct fixed_size_list_node_pool
    {
        public:

            ////////////////////////////////////////////////////////////
            // Public types

            using node_type = fixed_size_list_node<T>;

            ////////////////////////////////////////////////////////////
            // Constructors

            // Make the node pool immovable
            fixed_size_list_node_pool(const fixed_size_list_node_pool&) = delete;
            fixed_size_list_node_pool(fixed_size_list_node_pool&&) = delete;
            fixed_size_list_node_pool& operator=(const fixed_size_list_node_pool&) = delete;
            fixed_size_list_node_pool& operator=(fixed_size_list_node_pool&&) = delete;

            explicit fixed_size_list_node_pool(std::ptrdiff_t capacity):
                // Allocate enough space to store N nodes
                buffer_(static_cast<node_type*>(::operator new(capacity * sizeof(node_type))),
                       operator_deleter(capacity * sizeof(node_type))),
                first_free_(buffer_.get()),
                capacity_(capacity)
            {
                // fixed_size_list_node constructors are noexcept, so we don't
                // need to handle the cleanup of list nodes that would have been
                // required if exceptions could have been thrown

                auto ptr = buffer_.get();
                for (std::ptrdiff_t n = 0 ; n < capacity - 1 ; ++n, ++ptr) {
                    // Each node is initialized with a "next" field pointing to the next
                    // node in memory: this allows to create a list of free nodes laid
                    // out contiguously in memory in one pass
                    ::new (ptr) node_type(ptr + 1);
                }
                // Initialize the last node, guard with nullptr
                ::new (ptr) node_type(nullptr);
            }

            ////////////////////////////////////////////////////////////
            // Destructor

            ~fixed_size_list_node_pool()
            {
                // Destroy the nodes
                node_type* ptr = buffer_.get();
                for (std::ptrdiff_t n = 0 ; n < capacity_ ; ++n, ++ptr) {
                    ptr->~node_type();
                }
            }

            ////////////////////////////////////////////////////////////
            // Node providing/retrieval

            auto next_free_node()
                -> node_type*
            {
                // Retrieve next free node
                CPPSORT_ASSERT(first_free_ != nullptr);
                node_type* new_node = first_free_;
                first_free_ = first_free_->next;
                return new_node;
            }

            auto retrieve_nodes(node_type* first, node_type* last)
                -> void
            {
                // Get back a range of nodes linked together, this function
                // allows to retrieve any number of nodes in O(1), assuming
                // that last is accessible repeatedly iterating over next
                // from first
                last->next = first_free_;
                first_free_ = first;
            }

            ////////////////////////////////////////////////////////////
            // Danger Zone

            // reset_nodes relinks the first n nodes of the buffer in a
            // rather brutal way: it relinks everywhere next pointer of
            // the n first elements of the buffer to the next element in
            // the buffer.
            //
            // The only real use case is when reusing the same node pool
            // with several lists: once a list is destroyed, if it used
            // no more than n elements, relink those n elements, which
            // can greatly improve the cache-friendliness of the next
            // node allocations.
            //
            // Using this function incorrectly *will* fuck everything
            // and its invariants up.

            auto reset_nodes(std::ptrdiff_t until_n)
                -> void
            {
                auto ptr = buffer_.get();
                for (std::ptrdiff_t n = 0 ; n < until_n - 1 ; ++n, ++ptr) {
                    ptr->next = ptr + 1;
                }
                if (until_n == capacity_) {
                    ptr->next = nullptr;
                } else {
                    ptr->next = ptr + 1;
                }
                first_free_ = buffer_.get();
            }

        private:

            ////////////////////////////////////////////////////////////
            // Data members

            // Backing storage
            std::unique_ptr<node_type, operator_deleter> buffer_;

            // First free node of the pool
            node_type* first_free_;

            // Number of nodes the pool holds
            std::ptrdiff_t capacity_;
    };

    ////////////////////////////////////////////////////////////
    // List iterator
    //
    // Iterator wrapping a fixed_size_list_node: it uses the
    // node's next & prev pointers to advance through the list

    template<typename T>
    struct fixed_size_list_iterator
    {
        public:

            ////////////////////////////////////////////////////////////
            // Public types

            using iterator_category = std::bidirectional_iterator_tag;
            using value_type        = T;
            using difference_type   = std::ptrdiff_t;
            using pointer           = T*;
            using reference         = T&;

            ////////////////////////////////////////////////////////////
            // Constructors

            constexpr explicit fixed_size_list_iterator(fixed_size_list_node<T>* ptr) noexcept:
                ptr_(ptr)
            {}

            ////////////////////////////////////////////////////////////
            // Members access

            constexpr auto base() const
                -> fixed_size_list_node<T>*
            {
                return ptr_;
            }

            ////////////////////////////////////////////////////////////
            // Element access

            auto operator*() const
                -> reference
            {
                return ptr_->value;
            }

            auto operator->() const
                -> pointer
            {
                return &(operator*());
            }

            ////////////////////////////////////////////////////////////
            // Increment/decrement operators

            auto operator++()
                -> fixed_size_list_iterator&
            {
                ptr_ = ptr_->next;
                return *this;
            }

            auto operator++(int)
                -> fixed_size_list_iterator
            {
                auto tmp = *this;
                operator++();
                return tmp;
            }

            auto operator--()
                -> fixed_size_list_iterator&
            {
                ptr_ = ptr_->prev;
                return *this;
            }

            auto operator--(int)
                -> fixed_size_list_iterator
            {
                auto tmp = *this;
                operator--();
                return tmp;
            }

            ////////////////////////////////////////////////////////////
            // Comparison operators

            constexpr
            friend auto operator==(const fixed_size_list_iterator& lhs, const fixed_size_list_iterator& rhs)
                -> bool
            {
                return lhs.base() == rhs.base();
            }

            constexpr
            friend auto operator!=(const fixed_size_list_iterator& lhs, const fixed_size_list_iterator& rhs)
                -> bool
            {
                return lhs.base() != rhs.base();
            }

        private:

            fixed_size_list_node<T>* ptr_;
    };

    ////////////////////////////////////////////////////////////
    // Fixed size list
    //
    // This is a non-copyable doubly linked list data structure that
    // can be constructed with a reference to a fixed-size node
    // pool. The list is handles the lifetimes of the node's values,
    // but the lifetime of the nodes themselves is handled by the
    // pool.
    //
    // The list is implemented with a sentinel node to simplify many
    // of the operations. This sentinel node is store in the list
    // itself instead of being allocated from the node pool. Its
    // next pointer points to the first element of the list, its
    // prev pointer points to the last. When the list is empty, the
    // sentinel node points to itself.
    //
    // The interface is supposed to be as close as possible from
    // that of std::list, except in the following areas:
    // - Construction from a node pool
    // - No tracking of the size
    // - No copy semantics
    // - empty -> is_empty
    // A lot of the capabilities of std::list are still missing,
    // they will be added as needed whenever new library components
    // require them

    template<typename T>
    struct fixed_size_list
    {
        public:

            ////////////////////////////////////////////////////////////
            // Public types

            using value_type = T;
            using size_type = std::size_t;
            using reference = value_type&;
            using const_reference = const value_type&;
            using pointer = value_type*;
            using const_pointer = const value_type*;
            using difference_type = std::ptrdiff_t;
            using iterator = fixed_size_list_iterator<T>;
            // TODO: const_iterator
            using node_type = fixed_size_list_node<T>;

            ////////////////////////////////////////////////////////////
            // Constructors

            // Make fixed_size_list non-copyable
            fixed_size_list(const fixed_size_list&) = delete;
            fixed_size_list& operator=(const fixed_size_list&) = delete;

            explicit constexpr fixed_size_list(fixed_size_list_node_pool<T>& node_pool) noexcept:
                node_pool_(&node_pool),
                sentinel_node_(&sentinel_node_, &sentinel_node_)
            {}

            constexpr fixed_size_list(fixed_size_list&& other) noexcept:
                node_pool_(other.node_pool_),
                sentinel_node_(std::exchange(other.sentinel_node_.prev, &other.sentinel_node_),
                               std::exchange(other.sentinel_node_.next, &other.sentinel_node_))
            {
                if (sentinel_node_.prev == &other.sentinel_node_) {
                    sentinel_node_.prev = &sentinel_node_;
                } else {
                    sentinel_node_.prev->next = &sentinel_node_;
                }
                if (sentinel_node_.next == &other.sentinel_node_) {
                    sentinel_node_.next = &sentinel_node_;
                } else {
                    sentinel_node_.next->prev = &sentinel_node_;
                }
            }

            auto operator=(fixed_size_list&& other) noexcept
                -> fixed_size_list&
            {
                node_pool_ = other.node_pool_;
                if (other.sentinel_node_.prev != &other.sentinel_node_) {
                    sentinel_node_.prev = std::exchange(other.sentinel_node_.prev, &other.sentinel_node_);
                    sentinel_node_.prev->next = &sentinel_node_;
                } else {
                    sentinel_node_.prev = &sentinel_node_;
                }
                if (other.sentinel_node_.next != &other.sentinel_node_) {
                    sentinel_node_.next = std::exchange(other.sentinel_node_.next, &other.sentinel_node_);
                    sentinel_node_.next->prev = &sentinel_node_;
                } else {
                    sentinel_node_.next = &sentinel_node_;
                }
                return *this;
            }

            ////////////////////////////////////////////////////////////
            // Destructor

            ~fixed_size_list()
            {
                node_type* ptr = sentinel_node_.next;
                if (ptr != &sentinel_node_) {
                    // Destroy the node's values
                    do {
                        auto next_ptr = ptr->next;
                        ptr->value.~T();
                        ptr = next_ptr;
                    } while (ptr != &sentinel_node_);

                    // Let the pool know that the nodes are free again
                    node_pool_->retrieve_nodes(sentinel_node_.next, sentinel_node_.prev);
                }
            }

            ////////////////////////////////////////////////////////////
            // Element access

            auto front()
                -> reference
            {
                return sentinel_node_.next->value;
            }

            auto back()
                -> reference
            {
                return sentinel_node_.prev->value;
            }

            ////////////////////////////////////////////////////////////
            // Iterators

            auto begin()
                -> iterator
            {
                return iterator(sentinel_node_.next);
            }

            auto end()
                -> iterator
            {
                return iterator(&sentinel_node_);
            }

            ////////////////////////////////////////////////////////////
            // Capacity

            [[nodiscard]] auto is_empty() const noexcept
                -> bool
            {
                return &sentinel_node_ == sentinel_node_.next;
            }

            ////////////////////////////////////////////////////////////
            // Modifiers

            auto insert(iterator pos, const T& value)
                -> iterator
            {
                return iterator(insert_node_(pos.base(), value));
            }

            auto insert(iterator pos, T&& value)
                -> iterator
            {
                return iterator(insert_node_(pos.base(), std::move(value)));
            }

            auto push_back(const T& value)
                -> void
            {
                insert_node_(&sentinel_node_, value);
            }

            auto push_back(T&& value)
                -> void
            {
                insert_node_(&sentinel_node_, std::move(value));
            }

            auto push_front(const T& value)
                -> void
            {
                insert_node_(sentinel_node_.next, value);
            }

            auto push_front(T&& value)
                -> void
            {
                insert_node_(sentinel_node_.next, std::move(value));
            }

            ////////////////////////////////////////////////////////////
            // Operations

            template<typename Compare, typename Projection>
            auto merge(fixed_size_list& other, Compare compare, Projection projection)
                -> void
            {
                auto&& comp = utility::as_function(compare);
                auto&& proj = utility::as_function(projection);


                if (is_empty()) {
                    splice(end(), other);
                    return;
                }
                if (other.is_empty()) {
                    return;
                }

                auto other_it = other.begin();
                auto other_end = other.end();
                auto this_it = begin();
                auto this_end = end();

                while (other_it != other_end) {
                    if (comp(proj(*other_it), proj(*this_it))) {
                        // The following loop finds a series of nodes to splice
                        // into the current list, which is faster than splicing
                        // elements one by one
                        auto insert_begin = other_it;
                        do {
                            ++other_it;
                        } while (other_it != other_end && comp(proj(*other_it), proj(*this_it)));
                        fast_splice_(this_it, insert_begin, other_it);
                    } else {
                        ++this_it;
                    }

                    if (this_it == this_end) {
                        fast_splice_(this_end, other_it, other_end);
                        // Reset the other list's sentinel node,
                        // fast_splice_ does no do it
                        other.sentinel_node_.next = &other.sentinel_node_;
                        other.sentinel_node_.prev = &other.sentinel_node_;
                        return;
                    }
                }

                // Reset the other list's sentinel node,
                // fast_splice_ does no do it
                other.sentinel_node_.next = &other.sentinel_node_;
                other.sentinel_node_.prev = &other.sentinel_node_;
                CPPSORT_ASSERT(other.is_empty());
            }

            auto splice(iterator pos, fixed_size_list& other)
                -> void
            {
                if (other.is_empty()) {
                    return;
                }
                splice(pos, other, other.begin(), other.end());
            }

            auto splice(iterator pos, fixed_size_list&, iterator first, iterator last)
                -> void
            {
                CPPSORT_ASSERT(first.base() != last.base());

                auto last_1 = last.base()->prev;

                // Relink nodes in the other list
                first.base()->prev->next = last.base();
                last.base()->prev = first.base()->prev;

                // Add the range of elements to this list
                pos.base()->prev->next = first.base();
                first.base()->prev = pos.base()->prev;
                pos.base()->prev = last_1;
                last_1->next = pos.base();
            }

        private:

            ////////////////////////////////////////////////////////////
            // Helper functions

            auto insert_node_(node_type* pos, const T& value)
                -> node_type*
            {
                node_type* new_node = node_pool_->next_free_node();
                ::new (&new_node->value) T(value);
                link_node_before_(new_node, pos);
                return new_node;
            }

            auto insert_node_(node_type* pos, T&& value)
                -> node_type*
            {
                node_type* new_node = node_pool_->next_free_node();
                ::new (&new_node->value) T(std::move(value));
                link_node_before_(new_node, pos);
                return new_node;
            }

            auto link_node_before_(node_type* node, node_type* pos)
                -> void
            {
                // Relink pointers to a new node
                node->prev = pos->prev;
                node->next = pos;
                node->prev->next = node;
                node->next->prev = node;
            }

            auto fast_splice_(iterator pos, iterator first, iterator last)
                -> void
            {
                CPPSORT_ASSERT(first.base() != last.base());

                // This function is like splice(), but it doesn't relink
                // nodes in the other list: when merging another list into
                // the current one, we never use the relinked pointers in
                // the other list, so merge() can just skip relinking them
                // altogether and just reset the sentinel node at the end
                // of the operation instead

                // Add the range of elements to this list
                pos.base()->prev->next = first.base();
                first.base()->prev = pos.base()->prev;
                pos.base()->prev = last.base()->prev;
                last.base()->prev->next = pos.base();
            }

            ////////////////////////////////////////////////////////////
            // Data members

            // Node pool
            fixed_size_list_node_pool<T>* node_pool_;

            // Sentinel node: its prev field points to the last element
            // of the list, its next field to the first one
            node_type sentinel_node_;
    };
}}

#endif // CPPSORT_DETAIL_FIXED_SIZE_LIST_H_
