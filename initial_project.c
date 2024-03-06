#include <stdlib.h> /* for malloc/free/etc */
#include <stdio.h>
#include <unistd.h> /* for getpagesize() */
#include <assert.h>
#include <inttypes.h> /* for PRI___ macros (printf) */
#include <omp.h>
#include <stdbool.h>

#if defined(__STDC__)
#if defined(__STDC_VERSION__)
#if (__STDC_VERSION__ >= 199901L)
#define C99
#endif
#endif
#endif
#ifndef C99
#error | This code requires C99 support (for gcc, use -std=c99) |
#endif

// external
typedef const void *ptr_key_t;

// internal
typedef uint64_t key_t;
typedef uint64_t so_key_t;
typedef uintptr_t marked_ptr_t;
typedef void (*hash_callback_fn)(const ptr_key_t, void *, void *);

typedef struct hash_entry_s
{
    so_key_t key;
    void *value;
    marked_ptr_t next;
} hash_entry;

typedef struct hash_s
{
    marked_ptr_t *B; // Buckets
    volatile size_t count;
    volatile size_t size;
} * hash;

// prototypes
static void *list_find(marked_ptr_t *head,
                       so_key_t key,
                       marked_ptr_t **prev,
                       marked_ptr_t *cur,
                       marked_ptr_t *next);
static void initialize_bucket(hash h,
                              size_t bucket);

#define MAX_LOAD 4

#define MARK_OF(x) ((x)&1)
#define PTR_MASK(x) ((x) & ~(marked_ptr_t)1)
#define PTR_OF(x) ((hash_entry *)PTR_MASK(x))
#define CONSTRUCT(mark, ptr) (PTR_MASK((uintptr_t)ptr) | (mark))
#define UNINITIALIZED ((marked_ptr_t)0)

#define MSB (((uint64_t)1) << 63)

#define HASH_KEY(key) (((key >> 16) ^ key) * 0x45d9f3b)

// REVERSE_BYTE(x) function taken from the internet
#define REVERSE_BYTE(x) ((so_key_t)((((((uint32_t)(x)) * 0x0802LU & 0x22110LU) | (((uint32_t)(x)) * 0x8020LU & 0x88440LU)) * 0x10101LU >> 16) & 0xff))

#define REVERSE(x) ((REVERSE_BYTE((((so_key_t)(x))) & 0xff) << 56) |       \
                    (REVERSE_BYTE((((so_key_t)(x)) >> 8) & 0xff) << 48) |  \
                    (REVERSE_BYTE((((so_key_t)(x)) >> 16) & 0xff) << 40) | \
                    (REVERSE_BYTE((((so_key_t)(x)) >> 24) & 0xff) << 32) | \
                    (REVERSE_BYTE((((so_key_t)(x)) >> 32) & 0xff) << 24) | \
                    (REVERSE_BYTE((((so_key_t)(x)) >> 40) & 0xff) << 16) | \
                    (REVERSE_BYTE((((so_key_t)(x)) >> 48) & 0xff) << 8) |  \
                    (REVERSE_BYTE((((so_key_t)(x)) >> 56) & 0xff) << 0))

size_t hard_max_buckets = 0;

// TODO: CAS ; FETCHANDADD
#define CAS(ADDR, OLDV, NEWV) __sync_val_compare_and_swap((ADDR), (OLDV), (NEWV))
#define INCR(ADDR, INCVAL) __sync_fetch_and_add((ADDR), (INCVAL))

static inline so_key_t so_regularkey(const key_t key)
{
    return REVERSE(key | MSB);
}

static inline so_key_t so_dummykey(const key_t key)
{
    return REVERSE(key);
}

static inline size_t GET_PARENT(uint64_t bucket)
{
    uint64_t t = bucket;

    t |= t >> 1;
    t |= t >> 2;
    t |= t >> 4;
    t |= t >> 8;
    t |= t >> 16;
    t |= t >> 32; // creates a mask
    return bucket & (t >> 1);
}

static int list_insert(marked_ptr_t *head,
                       hash_entry *node,
                       marked_ptr_t *ocur)
{
    so_key_t key = node->key;

    while (1)
    {
        marked_ptr_t *lprev;
        marked_ptr_t cur;

        if (list_find(head, key, &lprev, &cur, NULL) != NULL)
        { // needs to set cur/prev
            if (ocur)
            {
                *ocur = cur;
            }
            return 0;
        }
        node->next = CONSTRUCT(0, cur);
        if (CAS(lprev, node->next, CONSTRUCT(0, node)) == CONSTRUCT(0, cur))
        {
            if (ocur)
            {
                *ocur = cur;
            }
            return 1;
        }
    }
}

static void *list_find(marked_ptr_t *head,
                       so_key_t key,
                       marked_ptr_t **oprev,
                       marked_ptr_t *ocur,
                       marked_ptr_t *onext)
{
    so_key_t ckey;
    void *cval;
    marked_ptr_t *prev = NULL;
    marked_ptr_t cur = UNINITIALIZED;
    marked_ptr_t next = UNINITIALIZED;

    while (1)
    {
        prev = head;
        cur = *prev;
        while (1)
        {
            if (PTR_OF(cur) == NULL)
            {
                if (oprev)
                {
                    *oprev = prev;
                }
                if (ocur)
                {
                    *ocur = cur;
                }
                if (onext)
                {
                    *onext = next;
                }
                return 0;
            }
            next = PTR_OF(cur)->next;
            ckey = PTR_OF(cur)->key;
            cval = PTR_OF(cur)->value;
            if (*prev != CONSTRUCT(0, cur))
            {
                break; // this means someone mucked with the list; start over
            }
            if (!MARK_OF(next))
            { // if next pointer is not marked
                if (ckey >= key)
                { // if current key > key, the key isn't in the list; if current key == key, the key IS in the list
                    if (oprev)
                    {
                        *oprev = prev;
                    }
                    if (ocur)
                    {
                        *ocur = cur;
                    }
                    if (onext)
                    {
                        *onext = next;
                    }
                    return (ckey == key) ? cval : NULL;
                }
                // but if current key < key, the we don't know yet, keep looking
                prev = &(PTR_OF(cur)->next);
            }
            else
            {
                if (CAS(prev, CONSTRUCT(0, cur), CONSTRUCT(0, next)) == CONSTRUCT(0, cur))
                {
                    free(PTR_OF(cur));
                }
                else
                {
                    break;
                }
            }
            cur = next;
        }
    }
}

static int list_delete(marked_ptr_t *head,
                       so_key_t key)
{
    while (1)
    {
        marked_ptr_t *lprev;
        marked_ptr_t lcur;
        marked_ptr_t lnext;

        if (list_find(head, key, &lprev, &lcur, &lnext) == NULL)
        {
            return 0;
        }
        if (CAS(&PTR_OF(lcur)->next, CONSTRUCT(0, lnext), CONSTRUCT(1, lnext)) != CONSTRUCT(0, lnext))
        {
            continue;
        }
        if (CAS(lprev, CONSTRUCT(0, lcur), CONSTRUCT(0, lnext)) == CONSTRUCT(0, lcur))
        {
            free(PTR_OF(lcur));
        }
        else
        {
            list_find(head, key, NULL, NULL, NULL); // needs to set cur/prev/next
        }
        return 1;
    }
}

// INITIALIZE_BUCKET
static void initialize_bucket(hash h,
                              size_t bucket)
{
    size_t parent = GET_PARENT(bucket);
    marked_ptr_t cur;

    if (h->B[parent] == UNINITIALIZED)
    {
        initialize_bucket(h, parent);
    }
    hash_entry *dummy = malloc(sizeof(hash_entry)); // XXX: should pull out of a memory pool
    assert(dummy);
    dummy->key = so_dummykey(bucket);
    dummy->value = NULL;
    dummy->next = UNINITIALIZED;
    if (!list_insert(&(h->B[parent]), dummy, &cur))
    {
        free(dummy);
        dummy = PTR_OF(cur);
        while (h->B[bucket] != CONSTRUCT(0, dummy))
            ;
    }
    else
    {
        h->B[bucket] = CONSTRUCT(0, dummy);
    }
}

hash hash_create(int needSync)
{
    hash tmp = malloc(sizeof(struct hash_s));

    assert(tmp);
    if (hard_max_buckets == 0)
    {
        hard_max_buckets = getpagesize() / sizeof(marked_ptr_t);
    }
    tmp->B = calloc(hard_max_buckets, sizeof(marked_ptr_t));
    assert(tmp->B);
    tmp->size = 2;
    tmp->count = 0;
    {
        hash_entry *dummy = calloc(1, sizeof(hash_entry)); // XXX: should pull out of a memory pool
        assert(dummy);
        tmp->B[0] = CONSTRUCT(0, dummy);
    }
    return tmp;
}

int hash_put(hash h,
             ptr_key_t key,
             void *value)
{
    hash_entry *node = malloc(sizeof(hash_entry)); // XXX: should pull out of a memory pool
    size_t bucket;
    uint64_t lkey = (uint64_t)(uintptr_t)key;

    HASH_KEY(lkey);
    bucket = lkey % h->size;

    assert(node);
    assert((lkey & MSB) == 0);
    node->key = so_regularkey(lkey);
    node->value = value;
    node->next = UNINITIALIZED;

    if (h->B[bucket] == UNINITIALIZED)
    {
        initialize_bucket(h, bucket);
    }
    if (!list_insert(&(h->B[bucket]), node, NULL))
    {
        free(node);
        return 0;
    }
    size_t csize = h->size;
    if (INCR(&h->count, 1) / csize > MAX_LOAD)
    {
        if (2 * csize <= hard_max_buckets)
        { // MAX size of the hash
            CAS(&h->size, csize, 2 * csize);
        }
    }
    return 1;
}

void *hash_get(hash h,
               const ptr_key_t key)
{
    size_t bucket;
    uint64_t lkey = (uint64_t)(uintptr_t)key;

    HASH_KEY(lkey);
    bucket = lkey % h->size;

    if (h->B[bucket] == UNINITIALIZED)
    {
        // CAN'T RETURN NULL POINTER!
        // NULL POINTER means that losing key/value pairs might be lost
        // hence, falsely report them as missing when hash table resizes
        initialize_bucket(h, bucket);
    }
    return list_find(&(h->B[bucket]), so_regularkey(lkey), NULL, NULL, NULL);
}

int hash_remove(hash h,
                const ptr_key_t key)
{
    size_t bucket;
    uint64_t lkey = (uint64_t)(uintptr_t)key;

    HASH_KEY(lkey);
    bucket = lkey % h->size;

    if (h->B[bucket] == UNINITIALIZED)
    {
        initialize_bucket(h, bucket);
    }
    if (!list_delete(&(h->B[bucket]), so_regularkey(lkey)))
    {
        return 0;
    }
    INCR(&h->count, -1);
    return 1;
}

void hash_destroy(hash h)
{
    marked_ptr_t cursor;

    assert(h);
    assert(h->B);
    cursor = h->B[0];
    while (PTR_OF(cursor) != NULL)
    {
        marked_ptr_t tmp = cursor;
        assert(MARK_OF(tmp) == 0);
        cursor = PTR_OF(cursor)->next;
        free(PTR_OF(tmp));
    }
    free(h->B);
    free(h);
}

size_t hash_count(hash h)
{
    assert(h);
    return h->size;
}

void call_hash_callback_fn(hash h,
                           hash_callback_fn f,
                           void *arg)
{
    marked_ptr_t cursor;

    assert(h);
    assert(h->B);
    cursor = h->B[0];
    while (PTR_OF(cursor) != NULL)
    {
        marked_ptr_t tmp = cursor;
        so_key_t key = PTR_OF(cursor)->key;
        assert(MARK_OF(tmp) == 0);
        if (key & 1)
        {
            f((ptr_key_t)(uintptr_t)REVERSE(key ^ 1), PTR_OF(cursor)->value, arg);
        }
        else
        {
            // f((key_t)REVERSE(key), PTR_OF(cursor)->value, (void *)1);
        }
        cursor = PTR_OF(cursor)->next;
    }
}

void print_all(const ptr_key_t k,
               void *v,
               void *a)
{
    if (a)
    {
        printf("\t{ %6" PRIuPTR ",   ,\t%p }\n", (uintptr_t)k, /*(uintptr_t)v,*/ a);
    }
    else
    {
        printf("\t{ %6" PRIuPTR ", %2" PRIuPTR ",\t    }\n", (uintptr_t)k, (uintptr_t)v);
    }
}

int main()
{
    intptr_t i = 1;
    hash H = hash_create(0);
    uint64_t n_per_thread = 50000;

    double threadtime[8];
    bool failure = false;

    call_hash_callback_fn(H, print_all, NULL);

    #pragma omp parallel
    {
        int t = omp_get_thread_num();
		double start, stop;

        #pragma omp barrier
		for (uint64_t k = 0; k < n_per_thread / 2; k++) {
			if ((t % 2) == 0) {
                hash_put(H, (void *)(k + t * n_per_thread), (void *)i++);
			} else {
                hash_put(H, (void *)(k + (t-1) * n_per_thread/2), (void *)i++);
			}
		}
        printf("H->count = %lu, H->size = %lu\n", H->count, H->size);

        call_hash_callback_fn(H, print_all, NULL);

        #pragma omp barrier
		start = omp_get_wtime();
		for (uint64_t k = 0; k < n_per_thread; k++) {
			uint64_t random = (uint64_t)rand();
            printf("%s %llu\n", hash_get(H, (void *)random) ? "found" : "not found", random);
			if ((t % 2) == 0) {
                hash_remove(H, (void *)(k + t * n_per_thread));
			} else if ((t % 2) == 1) {
                hash_put(H, (void *)(k + t * n_per_thread), (void *)i++);
			}
		}
        stop = omp_get_wtime();
		threadtime[t] = stop - start;

        call_hash_callback_fn(H, print_all, NULL);

        #pragma omp barrier
		for (uint64_t k = 0; k < n_per_thread; k++) {
            bool contains = hash_get(H, (void *)(k + t * n_per_thread )) ? true : false;
			if ((t % 2) == 0 && contains) {
                printf("Value: %llu should not be in table, but is!", (k + t * n_per_thread));
				failure |= true;
			} else if ((t % 2) == 1 && !contains) {
                printf("Value: %llu should be in table, but isn't!", (k + t * n_per_thread));
				failure |= true;
			}
		}
	}
    double total_time = 0;
	for (int k = 0; k < 8; k++) {
		total_time += threadtime[k];
	}
    hash_destroy(H);

    printf("Total elapsed time: %fs.\n", total_time);

	if (failure) {
		printf("Error in the datastructure detected!\n");
		return -1;
	} else {
		printf("No error occured!\n");
	}
    
    return 0;
}