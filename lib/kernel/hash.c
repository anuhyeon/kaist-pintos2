/* Hash table.

   This data structure is thoroughly documented in the Tour of
   Pintos for Project 3.

   See hash.h for basic information. */

#include "hash.h"
#include "../debug.h"
#include "threads/malloc.h"

#define list_elem_to_hash_elem(LIST_ELEM)                       \
	list_entry(LIST_ELEM, struct hash_elem, list_elem)

static struct list *find_bucket (struct hash *, struct hash_elem *);
static struct hash_elem *find_elem (struct hash *, struct list *,
		struct hash_elem *);
static void insert_elem (struct hash *, struct list *, struct hash_elem *);
static void remove_elem (struct hash *, struct hash_elem *);
static void rehash (struct hash *);

/* Initializes hash table H to compute hash values using HASH and
   compare hash elements using LESS, given auxiliary data AUX. 
   여기서 hash_init()은 해시 테이블을 초기화하는 함수이다. 
   해시 함수 page_hash_func를 이용하여 해시 값을 계산하고 
   비교 함수 page_less_func를 이용해 해시 테이블 내 요소를 비교하는 테이블을 만든다.
   spt init() 함수는 페이지 테이블이 처음 만들어질 때 함께 만들어져야 한다. 
   페이지 테이블은 언제 만들어질까? 
   1. 처음에 process가 생성될 때 
   2. fork로 만들어질 때 
   위 두 가지 경우에 해당한다. 
   따라서 initd()와 __do_fork()에서 초기화를 추가해준다. (이는 이미 적용되어 있음)
   */
bool
hash_init (struct hash *h,
		hash_hash_func *hash, hash_less_func *less, void *aux) {
	h->elem_cnt = 0;
	h->bucket_cnt = 4;
	h->buckets = malloc (sizeof *h->buckets * h->bucket_cnt);
	h->hash = hash;
	h->less = less;
	h->aux = aux;

	if (h->buckets != NULL) {
		hash_clear (h, NULL);
		return true;
	} else
		return false;
}

/* Removes all the elements from H.

   If DESTRUCTOR is non-null, then it is called for each element
   in the hash.  DESTRUCTOR may, if appropriate, deallocate the
   memory used by the hash element.  However, modifying hash
   table H while hash_clear() is running, using any of the
   functions hash_clear(), hash_destroy(), hash_insert(),
   hash_replace(), or hash_delete(), yields undefined behavior,
   whether done in DESTRUCTOR or elsewhere. */
void
hash_clear (struct hash *h, hash_action_func *destructor) {
	size_t i;

	for (i = 0; i < h->bucket_cnt; i++) {
		struct list *bucket = &h->buckets[i];

		if (destructor != NULL)
			while (!list_empty (bucket)) {
				struct list_elem *list_elem = list_pop_front (bucket);
				struct hash_elem *hash_elem = list_elem_to_hash_elem (list_elem);
				destructor (hash_elem, h->aux);
			}

		list_init (bucket);
	}

	h->elem_cnt = 0;
}

/* Destroys hash table H.

   If DESTRUCTOR is non-null, then it is first called for each
   element in the hash.  DESTRUCTOR may, if appropriate,
   deallocate the memory used by the hash element.  However,
   modifying hash table H while hash_clear() is running, using
   any of the functions hash_clear(), hash_destroy(),
   hash_insert(), hash_replace(), or hash_delete(), yields
   undefined behavior, whether done in DESTRUCTOR or
   elsewhere. */
void
hash_destroy (struct hash *h, hash_action_func *destructor) {
	if (destructor != NULL)
		hash_clear (h, destructor);
	free (h->buckets);
}

/* Inserts NEW into hash table H and returns a null pointer, if
   no equal element is already in the table.
   If an equal element is already in the table, returns it
   without inserting NEW. 
   새 요소 NEW를 해시 테이블 H에 삽입하고, 이미 테이블에 동일한 요소가 없다면 NULL 포인터를 반환.
   만약 테이블에 동일한 요소가 이미 존재한다면, 그 요소를 반환하고 새 요소는 삽입하지 않는다.
   */
struct hash_elem *
hash_insert (struct hash *h, struct hash_elem *new) { // 해시테이블에 새로운 요소를 삽입하는 기능을 수행하는 함수-> 해시테이블 h에 새로운 요소 new를 추가하려고함.
	 // 새 요소 'new'를 넣을 버킷을 찾음 
	struct list *bucket = find_bucket (h, new); // new의 해시 값을 사용해서 넣을 버킷을 찾음
	struct hash_elem *old = find_elem (h, bucket, new); // 버킷안에 연결되어있는 요소들을 다 탐색하여 new랑 같은 요소가 있는지 확인 탐해당 버킷안에 new랑 같은 동일한 요소가 있는지 확인 

	if (old == NULL)  // new랑 같은 요소가 없으면 new를 삽입
		insert_elem (h, bucket, new); // 해당 버킷에 new라는 요소를 삽입

	rehash (h); // 필요에 따라 재해싱을 수행하여 해시테이블 성능 유지

	return old; // 삽입을 방해하는 기존 요소를 반환하거나, 'new'가 삽입된 경우 NULL을 반환
}

/* Inserts NEW into hash table H, replacing any equal element
   already in the table, which is returned. */
struct hash_elem *
hash_replace (struct hash *h, struct hash_elem *new) {
	struct list *bucket = find_bucket (h, new);
	struct hash_elem *old = find_elem (h, bucket, new);

	if (old != NULL)
		remove_elem (h, old);
	insert_elem (h, bucket, new);

	rehash (h);

	return old;
}

/* Finds and returns an element equal to E in hash table H, or a
   null pointer if no equal element exists in the table. */
struct hash_elem *
hash_find (struct hash *h, struct hash_elem *e) {
	return find_elem (h, find_bucket (h, e), e); // e라는 요소를 통해 해시값을 구하여 해당 인덱스를 가진 버킷을 찾아 해당 버킷에서 e와 동일한 요소를 찾음 찾으면 해당 요소를 반환, 찾지 못하면 NULL을 반환
}

/* Finds, removes, and returns an element equal to E in hash
   table H.  Returns a null pointer if no equal element existed
   in the table.

   If the elements of the hash table are dynamically allocated,
   or own resources that are, then it is the caller's
   responsibility to deallocate them. 
   
   해시 테이블 H에서 E와 동등한 요소를 찾아 제거하고 반환한다.
   동등한 요소가 테이블에 없다면 NULL 포인터를 반환힌다.

   해시 테이블의 요소들이 동적으로 할당되었거나, 관련 자원을 소유하고 있다면,
   이 요소들을 해제하는 책임은 호출자에게 있다..
   */
struct hash_elem *
hash_delete (struct hash *h, struct hash_elem *e) { // 인자로 받은 e라는 요소를 찾아 해시테이블에서 제거하는 함수
	struct hash_elem *found = find_elem (h, find_bucket (h, e), e); // e라는 요소를 찾기 위해 해당 요소가 위치할 버킷을 찾고, 그 안에서 요소를 검색.  find_bucket()함수는 요소의 해시 값을 가지고 해시테이블 인덱스를 구하여 버킷을 찾는 것.
	if (found != NULL) { // 요소가 해시테이블에 존재 하면
		remove_elem (h, found); // 해당 요소를 해시테이블에서 제거
		rehash (h); // 재해싱
	}
	return found; // 제거된 요소를 반환하거나, 요소가 없었으면 NULL을 반환(해시테이블 안에 삭제할 요소가 존재하지 않았다면 NULL을 반환)
}

/* Calls ACTION for each element in hash table H in arbitrary
   order.
   Modifying hash table H while hash_apply() is running, using
   any of the functions hash_clear(), hash_destroy(),
   hash_insert(), hash_replace(), or hash_delete(), yields
   undefined behavior, whether done from ACTION or elsewhere. */
void
hash_apply (struct hash *h, hash_action_func *action) {
	size_t i;

	ASSERT (action != NULL);

	for (i = 0; i < h->bucket_cnt; i++) {
		struct list *bucket = &h->buckets[i];
		struct list_elem *elem, *next;

		for (elem = list_begin (bucket); elem != list_end (bucket); elem = next) {
			next = list_next (elem);
			action (list_elem_to_hash_elem (elem), h->aux);
		}
	}
}

/* Initializes I for iterating hash table H.

   Iteration idiom:

   struct hash_iterator i;

   hash_first (&i, h);
   while (hash_next (&i))
   {
   struct foo *f = hash_entry (hash_cur (&i), struct foo, elem);
   ...do something with f...
   }

   Modifying hash table H during iteration, using any of the
   functions hash_clear(), hash_destroy(), hash_insert(),
   hash_replace(), or hash_delete(), invalidates all
   iterators. */
void
hash_first (struct hash_iterator *i, struct hash *h) {
	ASSERT (i != NULL);
	ASSERT (h != NULL);

	i->hash = h;
	i->bucket = i->hash->buckets;
	i->elem = list_elem_to_hash_elem (list_head (i->bucket));
}

/* Advances I to the next element in the hash table and returns
   it.  Returns a null pointer if no elements are left.  Elements
   are returned in arbitrary order.

   Modifying a hash table H during iteration, using any of the
   functions hash_clear(), hash_destroy(), hash_insert(),
   hash_replace(), or hash_delete(), invalidates all
   iterators. */
struct hash_elem *
hash_next (struct hash_iterator *i) {
	ASSERT (i != NULL);

	i->elem = list_elem_to_hash_elem (list_next (&i->elem->list_elem));
	while (i->elem == list_elem_to_hash_elem (list_end (i->bucket))) {
		if (++i->bucket >= i->hash->buckets + i->hash->bucket_cnt) {
			i->elem = NULL;
			break;
		}
		i->elem = list_elem_to_hash_elem (list_begin (i->bucket));
	}

	return i->elem;
}

/* Returns the current element in the hash table iteration, or a
   null pointer at the end of the table.  Undefined behavior
   after calling hash_first() but before hash_next(). */
struct hash_elem *
hash_cur (struct hash_iterator *i) {
	return i->elem;
}

/* Returns the number of elements in H. */
size_t
hash_size (struct hash *h) {
	return h->elem_cnt;
}

/* Returns true if H contains no elements, false otherwise. */
bool
hash_empty (struct hash *h) {
	return h->elem_cnt == 0;
}

/* Fowler-Noll-Vo hash constants, for 32-bit word sizes. */
#define FNV_64_PRIME 0x00000100000001B3UL
#define FNV_64_BASIS 0xcbf29ce484222325UL

/* Returns a hash of the SIZE bytes in BUF. */
uint64_t
hash_bytes (const void *buf_, size_t size) { //hash_bytes()함수는 해시 함수를 이용해 buf 메모리 공간에 있는 정수를 size 만큼 암호화 시킨후 해당 암호화된 해시값(해시값 % 버킷 수 =해시테이블 인덱스 값)을 반환
	/* Fowler-Noll-Vo 32-bit hash, for bytes. Fowler-Noll-Vo (FNV) 해시 알고리즘의 변형 */
	const unsigned char *buf = buf_;
	uint64_t hash;

	ASSERT (buf != NULL);

	hash = FNV_64_BASIS;
	while (size-- > 0)
		hash = (hash * FNV_64_PRIME) ^ *buf++;

	return hash;
}

/* Returns a hash of string S. */
uint64_t
hash_string (const char *s_) {
	const unsigned char *s = (const unsigned char *) s_;
	uint64_t hash;

	ASSERT (s != NULL);

	hash = FNV_64_BASIS;
	while (*s != '\0')
		hash = (hash * FNV_64_PRIME) ^ *s++;

	return hash;
}

/* Returns a hash of integer I. */
uint64_t
hash_int (int i) {
	return hash_bytes (&i, sizeof i);
}

/* Returns the bucket in H that E belongs in. H 해시 테이블에서 E 해시 요소가 속해야 할 버킷을 반환*/
static struct list *
find_bucket (struct hash *h, struct hash_elem *e) {
	size_t bucket_idx = h->hash (e, h->aux) & (h->bucket_cnt - 1); // 요소 E에 대한 해시 값을 계산하고, 해시 테이블 H의 버킷 수에 따라 인덱스를 정규화
	return &h->buckets[bucket_idx]; // 계산된 인덱스에 해당하는 버킷의 주소를 반환
}

/* Searches BUCKET in H for a hash element equal to E.  Returns
   it if found or a null pointer otherwise.
   H의 BUCKET에서 E와 동등한 해시 요소를 검색한다. 
   찾은 경우 해당 요소를 반환하고, 찾지 못한 경우 NULL 포인터를 반환
    */
static struct hash_elem *
find_elem (struct hash *h, struct list *bucket, struct hash_elem *e) {
	struct list_elem *i; // // 리스트를 순회하기 위한 반복자
	// 버킷의 시작부터 끝까지 순회
	for (i = list_begin (bucket); i != list_end (bucket); i = list_next (i)) {
		struct hash_elem *hi = list_elem_to_hash_elem (i); // 리스트 요소를 해시 요소로 변환
		// h->less 함수를 사용하여 E와 hi가 동일한지 검사.
		// 두 요소가 서로에게 'less'가 아닌 경우 동등함을 의미.
		if (!h->less (hi, e, h->aux) && !h->less (e, hi, h->aux)) // e랑 hi가 동일하면
			return hi; // 해당 hash_elem을 반환
	}
	return NULL; // 동일한 요소를 찾지 못하면 NULL 반환
}

/* Returns X with its lowest-order bit set to 1 turned off. */
static inline size_t
turn_off_least_1bit (size_t x) {
	return x & (x - 1);
}

/* Returns true if X is a power of 2, otherwise false. */
static inline size_t
is_power_of_2 (size_t x) {
	return x != 0 && turn_off_least_1bit (x) == 0;
}

/* Element per bucket ratios. */
#define MIN_ELEMS_PER_BUCKET  1 /* Elems/bucket < 1: reduce # of buckets. */
#define BEST_ELEMS_PER_BUCKET 2 /* Ideal elems/bucket. */
#define MAX_ELEMS_PER_BUCKET  4 /* Elems/bucket > 4: increase # of buckets. */

/* Changes the number of buckets in hash table H to match the
   ideal.  This function can fail because of an out-of-memory
   condition, but that'll just make hash accesses less efficient;
   we can still continue. */
static void
rehash (struct hash *h) {
	size_t old_bucket_cnt, new_bucket_cnt;
	struct list *new_buckets, *old_buckets;
	size_t i;

	ASSERT (h != NULL);

	/* Save old bucket info for later use. */
	old_buckets = h->buckets;
	old_bucket_cnt = h->bucket_cnt;

	/* Calculate the number of buckets to use now.
	   We want one bucket for about every BEST_ELEMS_PER_BUCKET.
	   We must have at least four buckets, and the number of
	   buckets must be a power of 2. */
	new_bucket_cnt = h->elem_cnt / BEST_ELEMS_PER_BUCKET;
	if (new_bucket_cnt < 4)
		new_bucket_cnt = 4;
	while (!is_power_of_2 (new_bucket_cnt))
		new_bucket_cnt = turn_off_least_1bit (new_bucket_cnt);

	/* Don't do anything if the bucket count wouldn't change. */
	if (new_bucket_cnt == old_bucket_cnt)
		return;

	/* Allocate new buckets and initialize them as empty. */
	new_buckets = malloc (sizeof *new_buckets * new_bucket_cnt);
	if (new_buckets == NULL) {
		/* Allocation failed.  This means that use of the hash table will
		   be less efficient.  However, it is still usable, so
		   there's no reason for it to be an error. */
		return;
	}
	for (i = 0; i < new_bucket_cnt; i++)
		list_init (&new_buckets[i]);

	/* Install new bucket info. */
	h->buckets = new_buckets;
	h->bucket_cnt = new_bucket_cnt;

	/* Move each old element into the appropriate new bucket. */
	for (i = 0; i < old_bucket_cnt; i++) {
		struct list *old_bucket;
		struct list_elem *elem, *next;

		old_bucket = &old_buckets[i];
		for (elem = list_begin (old_bucket);
				elem != list_end (old_bucket); elem = next) {
			struct list *new_bucket
				= find_bucket (h, list_elem_to_hash_elem (elem));
			next = list_next (elem);
			list_remove (elem);
			list_push_front (new_bucket, elem);
		}
	}

	free (old_buckets);
}

/* Inserts E into BUCKET (in hash table H). */
static void
insert_elem (struct hash *h, struct list *bucket, struct hash_elem *e) {
	h->elem_cnt++;
	list_push_front (bucket, &e->list_elem);
}

/* Removes E from hash table H. */
static void
remove_elem (struct hash *h, struct hash_elem *e) {
	h->elem_cnt--;
	list_remove (&e->list_elem);
}

