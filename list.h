// A circular double-linked list implementation originated
// from the Linux Kernel.
#ifndef LIST_H
#define LIST_H

#define container_of(ptr, type, member)                                        \
  ((type *)((char *)(ptr) - __builtin_offsetof(type, member)))

struct list_head {
  struct list_head *prev;
  struct list_head *next;
};

static inline void list_head_init(struct list_head *head) {
  head->prev = head->next = head;
}

// __list_add inserts el between prev and next.
static inline void __list_add(struct list_head *el, struct list_head *prev,
                              struct list_head *next) {
  prev->next = el;
  el->prev = prev;
  el->next = next;
  next->prev = el;
}

// list_add adds el right after head.
static inline void list_add(struct list_head *el, struct list_head *head) {
  __list_add(el, head, head->next);
}

static inline void list_del(struct list_head *el) {
  el->prev->next = el->next;
  el->next->prev = el->prev;
}

#define list_foreach(el, head)                                                 \
  for (el = (head)->next; el != (head); el = el->next)

#define list_foreach_prev(el, head)                                            \
  for (el = (head)->prev; el != (head); el = el->prev)

#endif