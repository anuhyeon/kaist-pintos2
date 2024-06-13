#ifndef __LIB_KERNEL_HASH_H
#define __LIB_KERNEL_HASH_H

/* Hash table.
 *
 * This data structure is thoroughly documented in the Tour of
 * Pintos for Project 3.
 *
 * This is a standard hash table with chaining.  To locate an
 * element in the table, we compute a hash function over the
 * element's data and use that as an index into an array of
 * doubly linked lists, then linearly search the list.
 * 
 * 이 해시 테이블은 체이닝 방식의 표준 해시 테이블이다. 
 * 테이블 내에 원소를 위치시키기 위해, 
 * 해시 함수로 원소의 값을 계산해 변환하고, 
 * 이를 인덱스로 사용해 이중 연결리스트의 배열에 삽입한다. 
 * 이후, 리스트를 선형으로 검색한다. 
 *
 * The chain lists do not use dynamic allocation.  Instead, each
 * structure that can potentially be in a hash must embed a
 * struct hash_elem member.  All of the hash functions operate on
 * these `struct hash_elem's.  The hash_entry macro allows
 * conversion from a struct hash_elem back to a structure object
 * that contains it.  This is the same technique used in the
 * linked list implementation.  Refer to lib/kernel/list.h for a
 * detailed explanation. 
 * 
 * 이 체인 리스트는 동적 할당을 사용하지 않는다. 
 * 대신, 해시 테이블에 value로 들어갈 여지가 있는 page 구조체는 반드시 hash_elem을 멤버로 가져야만 한다. 
 * 모든 hash function은 struct hash_elem 위에서 동작한다. 
 * hash_entry 매크로는 hash_elem 구조체로부터 해당 elem을 갖고 있는 구조체(page)로 전환한다. (hash table이 아님)
 * */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "list.h"

/* Hash element. */
struct hash_elem {
	struct list_elem list_elem;
};

/* Converts pointer to hash element HASH_ELEM into a pointer to
 * the structure that HASH_ELEM is embedded inside.  Supply the
 * name of the outer structure STRUCT and the member name MEMBER
 * of the hash element.  See the big comment at the top of the
 * file for an example. */
#define hash_entry(HASH_ELEM, STRUCT, MEMBER)                   \
	((STRUCT *) ((uint8_t *) &(HASH_ELEM)->list_elem        \
		- offsetof (STRUCT, MEMBER.list_elem)))

/* Computes and returns the hash value for hash element E, given
 * auxiliary data AUX. */
typedef uint64_t hash_hash_func (const struct hash_elem *e, void *aux); // hashed index를 만드는 함수 포인터에 대한 타입 정의
// //아래는 함수포인터 사용 예제 + 함수 구현
// uint64_t my_hash_function(const struct hash_elem *e, void *aux) {
//     // 여기에 해시 함수의 구현 내용 작성
// }
// // 함수 포인터 선언 및 초기화
// hash_hash_func *my_func_pointer = my_hash_function;
// // 함수 포인터를 사용한 함수 호출
// uint64_t hash_value = my_func_pointer(some_elem, some_aux);
/* Compares the value of two hash elements A and B, given
 * auxiliary data AUX.  Returns true if A is less than B, or
 * false if A is greater than or equal to B. */
typedef bool hash_less_func (const struct hash_elem *a,  
		const struct hash_elem *b,
		void *aux); // hash 요소 간에 비교를 하는 함수

/* Performs some operation on hash element E, given auxiliary
 * data AUX. */
typedef void hash_action_func (struct hash_elem *e, void *aux);

/* Hash table. 
  가상 주소 va를 hash function을 통해서 hash table의 인덱스로 변경을 하면 
  해당 인덱스에 매핑되는 bucket에는 hash_elem이 달려있어 
  이 hash_elem을 통해 해당 페이지를 찾을 수 있는 구조임.
*/
struct hash {
	size_t elem_cnt;            /* Number of elements in table. */
	size_t bucket_cnt;          /* Number of buckets, a power of 2. */
	struct list *buckets;       /* Array of `bucket_cnt' lists. 버킷 또한 초기화 해주어야함 -> malloc을 통해 동적할당으로 배열을 받아옴. */
	hash_hash_func *hash;       /* Hash function. */
	hash_less_func *less;       /* Comparison function. */
	void *aux;                  /* Auxiliary data for `hash' and `less'. */
};

/* A hash table iterator. */
struct hash_iterator {
	struct hash *hash;          /* The hash table. */
	struct list *bucket;        /* Current bucket. */ 
	struct hash_elem *elem;     /* Current hash element in current bucket. */
};

/* Basic life cycle. */
bool hash_init (struct hash *, hash_hash_func *, hash_less_func *, void *aux);
void hash_clear (struct hash *, hash_action_func *);
void hash_destroy (struct hash *, hash_action_func *);

/* Search, insertion, deletion. */
struct hash_elem *hash_insert (struct hash *, struct hash_elem *);
struct hash_elem *hash_replace (struct hash *, struct hash_elem *);
struct hash_elem *hash_find (struct hash *, struct hash_elem *);
struct hash_elem *hash_delete (struct hash *, struct hash_elem *);

/* Iteration. */
void hash_apply (struct hash *, hash_action_func *);
void hash_first (struct hash_iterator *, struct hash *);
struct hash_elem *hash_next (struct hash_iterator *);
struct hash_elem *hash_cur (struct hash_iterator *);

/* Information. */
size_t hash_size (struct hash *);
bool hash_empty (struct hash *);

/* Sample hash functions. */
uint64_t hash_bytes (const void *, size_t);
uint64_t hash_string (const char *);
uint64_t hash_int (int);

#endif /* lib/kernel/hash.h */
