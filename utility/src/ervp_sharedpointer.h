#ifndef __ERVP_SHAREDPOINTER_H__
#define __ERVP_SHAREDPOINTER_H__

typedef struct
{
    void *data;
    int count;
    void (*deleter)(void *); // WITHOUT refcount_t
} refcount_t;

refcount_t *refcount_alloc(void *data, void (*deleter)(void *));
void sharedpointer_free(void *a, refcount_t *refcount);

#define sharedpointer_increase_refcount(a, free_except_refcount) \
    do { \
        assert(a); \
        if (((a)->refcount) != NULL) \
        { \
            ((a)->refcount)->count++; \
        } \
        else \
        { \
            refcount_t *refcount = refcount_alloc(a, free_except_refcount); \
            refcount->count = 2; \
            (a)->refcount = refcount; \
        } \
    } while (0)

#endif