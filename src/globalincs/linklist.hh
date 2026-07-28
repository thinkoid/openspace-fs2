/*
 * Copyright (C) Volition, Inc. 1999.  All rights reserved.
 *
 * All source code herein is the property of Volition, Inc. You may not sell
 * or otherwise commercially exploit the source or things you created based on the
 * source.
 *
*/

#ifndef _LINKLIST_H
#define _LINKLIST_H

#include <cstddef>
#include <type_traits>

// Intrusive circular doubly-linked list with a thin sentinel.
//
// Nodes carry their own links by inheriting list_links_t<T>; a head is a
// list_t<T> -- the two links and nothing else.  The ring runs through the
// sentinel: head->next is the first node, head->prev the last, and empty is
// the sentinel self-looped.  Every mutator is branch-free, and removal
// needs only the element.
//
// The links are typed T*, so the sentinel wears a T* uniform (sentinel()).
// The cast is safe because the links sit at offset 0 of head and node alike
// (list_links_t is the first base, nothing virtual), and only next/prev are
// ever touched through a sentinel pointer -- never node payload.  Walking
// past END_OF_LIST() therefore runs off a 16-byte object, which a sanitizer
// can catch; the old fat sentinel (a full node as head) made that bug
// silently readable.

// Deliberately trivial (no constructors, no member initializers): the
// engine memsets nodes and the structs that embed heads, then list_init()s
// every head before use -- retail's contract, kept.  A head is not valid
// until list_init() runs.
template< class T >
struct list_links_t
{
    T *next;
    T *prev;
};

template< class T >
struct list_t : list_links_t< T >
{
    static_assert(!std::is_polymorphic_v< T >,
                  "a vtable would displace the links from offset 0");

    T *sentinel()
    {
        static_assert(sizeof(list_t) == 2 * sizeof(void *),
                      "the sentinel is the links and nothing else");
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Winvalid-offsetof"
        static_assert(offsetof(T, next) == 0,
                      "list_links_t must be the first base, ahead of everything");
#pragma GCC diagnostic pop
        return reinterpret_cast< T * >(this);
    }
};

// Reinitializes a list to zero elements
template< class T >
inline void
list_init(list_t< T > *head)
{
    head->next = head->prev = head->sentinel();
}

// Inserts element onto the front of the list
template< class T >
inline void
list_insert(list_t< T > *head, T *elem)
{
    elem->next = head->next;
    elem->prev = head->sentinel();
    head->next->prev = elem;
    head->next = elem;
}

// Inserts new_elem before elem
template< class T >
inline void
list_insert_before(T *elem, T *new_elem)
{
    new_elem->prev = elem->prev;
    new_elem->next = elem;
    elem->prev->next = new_elem;
    elem->prev = new_elem;
}

// Appends an element on to the tail of the list
template< class T >
inline void
list_append(list_t< T > *head, T *elem)
{
    elem->prev = head->prev;
    elem->next = head->sentinel();
    head->prev->next = elem;
    head->prev = elem;
}

// Removes an element from the list it's in
template< class T >
inline void
list_remove(T *elem)
{
    elem->prev->next = elem->next;
    elem->next->prev = elem->prev;
    elem->next = nullptr;
    elem->prev = nullptr;
}

template< class T >
inline T *
GET_FIRST(list_t< T > *head)
{
    return head->next;
}

template< class T >
inline T *
GET_LAST(list_t< T > *head)
{
    return head->prev;
}

template< class T >
inline T *
GET_NEXT(T *elem)
{
    return elem->next;
}

template< class T >
inline T *
GET_PREV(T *elem)
{
    return elem->prev;
}

template< class T >
inline T *
END_OF_LIST(list_t< T > *head)
{
    return head->sentinel();
}

template< class T >
inline bool
NOT_EMPTY(list_t< T > *head)
{
    return head->next != head->sentinel();
}

template< class T >
inline bool
EMPTY(list_t< T > *head)
{
    return head->next == head->sentinel();
}

#endif
