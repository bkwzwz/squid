#ifndef _MEM_POOL_H_
#define _MEM_POOL_H_

/**
 \defgroup MemPoolsAPI  Memory Management (Memory Pool Allocator)
 \ingroup Components
 *
 *\par
 *  MemPools are a pooled memory allocator running on top of malloc(). It's
 *  purpose is to reduce memory fragmentation and provide detailed statistics
 *  on memory consumption.
 *
 \par
 *  Preferably all memory allocations in Squid should be done using MemPools
 *  or one of the types built on top of it (i.e. cbdata).
 *
 \note Usually it is better to use cbdata types as these gives you additional
 *     safeguards in references and typechecking. However, for high usage pools where
 *     the cbdata functionality of cbdata is not required directly using a MemPool
 *     might be the way to go.
 */

#include "config.h"
#include "util.h"

#include "memMeter.h"
#include "splay.h"

#if HAVE_GNUMALLOC_H
#include <gnumalloc.h>
#elif HAVE_MALLOC_H
#include <malloc.h>
#endif

#if HAVE_MEMORY_H
#include <memory.h>
#endif

#if !M_MMAP_MAX
#if USE_DLMALLOC
#define M_MMAP_MAX -4
#endif
#endif

/// \ingroup MemPoolsAPI
#define MB ((size_t)1024*1024)
/// \ingroup MemPoolsAPI
#define mem_unlimited_size 2 * 1024 * MB
/// \ingroup MemPoolsAPI
#define toMB(size) ( ((double) size) / MB )
/// \ingroup MemPoolsAPI
#define toKB(size) ( (size + 1024 - 1) / 1024 )

/// \ingroup MemPoolsAPI
#define MEM_PAGE_SIZE 4096
/// \ingroup MemPoolsAPI
#define MEM_CHUNK_SIZE 4096 * 4
/// \ingroup MemPoolsAPI
#define MEM_CHUNK_MAX_SIZE  256 * 1024	/* 2MB */
/// \ingroup MemPoolsAPI
#define MEM_MIN_FREE  32
/// \ingroup MemPoolsAPI
#define MEM_MAX_FREE  65535	/* ushort is max number of items per chunk */

class MemImplementingAllocator;
class MemPoolStats;

/// \ingroup MemPoolsAPI
/// \todo Kill this typedef for C++
typedef struct _MemPoolGlobalStats MemPoolGlobalStats;

/// \ingroup MemPoolsAPI
class MemPoolIterator
{
public:
    MemImplementingAllocator *pool;
    MemPoolIterator * next;
};

/**
 \ingroup MemPoolsAPI
 * Object to track per-pool cumulative counters
 */
class mgb_t
{
public:
    mgb_t() : count(0), bytes(0) {}
    double count;
    double bytes;
};

/**
 \ingroup MemPoolsAPI
 * Object to track per-pool memory usage (alloc = inuse+idle)
 */
class MemPoolMeter
{
public:
    MemPoolMeter();
    void flush();
    MemMeter alloc;
    MemMeter inuse;
    MemMeter idle;


    /** history Allocations */
    mgb_t gb_allocated;
    mgb_t gb_oallocated;

    /** account Saved Allocations */
    mgb_t gb_saved;

    /** account Free calls */
    mgb_t gb_freed;
};

class MemImplementingAllocator;

/// \ingroup MemPoolsAPI
class MemPools
{
public:
    static MemPools &GetInstance();
    MemPools();
    void init();
    void flushMeters();

    /**
     \param label	Name for the pool. Displayed in stats.
     \param obj_size	Size of elements in MemPool.
     */
    MemImplementingAllocator * create(const char *label, size_t obj_size);

    /**
     * Sets upper limit in bytes to amount of free ram kept in pools. This is
     * not strict upper limit, but a hint. When MemPools are over this limit,
     * totally free chunks are immediately considered for release. Otherwise
     * only chunks that have not been referenced for a long time are checked.
     */
    void setIdleLimit(size_t new_idle_limit);

    size_t idleLimit() const;

    /**
     \par
     * Main cleanup handler. For MemPools to stay within upper idle limits,
     * this function needs to be called periodically, preferrably at some
     * constant rate, eg. from Squid event. It looks through all pools and
     * chunks, cleans up internal states and checks for releasable chunks.
     *
     \par
     * Between the calls to this function objects are placed onto internal
     * cache instead of returning to their home chunks, mainly for speedup
     * purpose. During that time state of chunk is not known, it is not
     * known whether chunk is free or in use. This call returns all objects
     * to their chunks and restores consistency.
     *
     \par
     * Should be called relatively often, as it sorts chunks in suitable
     * order as to reduce free memory fragmentation and increase chunk
     * utilisation.
     * Suitable frequency for cleanup is in range of few tens of seconds to
     * few minutes, depending of memory activity.
     *
     \todo DOCS: Re-write this shorter!
     *
     \param maxage   Release all totally idle chunks that
     *               have not been referenced for maxage seconds.
     */
    void clean(time_t maxage);

    void setDefaultPoolChunking(bool const &);
    MemImplementingAllocator *pools;
    int mem_idle_limit;
    int poolCount;
    bool defaultIsChunked;
private:
    static MemPools *Instance;
};

/**
 \ingroup MemPoolsAPI
 * a pool is a [growing] space for objects of the same size
 */
class MemAllocator
{
public:
    MemAllocator (char const *aLabel);
    virtual ~MemAllocator() {}

    /**
     \param stats	Object to be filled with statistical data about pool.
     \retval		Number of objects in use, ie. allocated.
     */
    virtual int getStats(MemPoolStats * stats, int accumulate = 0) = 0;

    virtual MemPoolMeter const &getMeter() const = 0;

    /**
     * Allocate one element from the pool
     */
    virtual void *alloc() = 0;

    /**
     * Free a element allocated by MemAllocator::alloc()
     */
    virtual void free(void *) = 0;

    virtual char const *objectType() const;
    virtual size_t objectSize() const = 0;
    virtual int getInUseCount() = 0;
    void zeroOnPush(bool doIt);
    int inUseCount();

    /**
     * Allows you tune chunk size of pooling. Objects are allocated in chunks
     * instead of individually. This conserves memory, reduces fragmentation.
     * Because of that memory can be freed also only in chunks. Therefore
     * there is tradeoff between memory conservation due to chunking and free
     * memory fragmentation.
     *
     \note  As a general guideline, increase chunk size only for pools that keep
     *      very many items for relatively long time.
     */
    virtual void setChunkSize(size_t chunksize) {}

    /**
     \param minSize	Minimum size needed to be allocated.
     \retval n Smallest size divisible by sizeof(void*)
     */
    static size_t RoundedSize(size_t minSize);

protected:
    bool doZeroOnPush;

private:
    const char *label;
};

/**
 \ingroup MemPoolsAPI
 * Support late binding of pool type for allocator agnostic classes
 */
class MemAllocatorProxy
{
public:
    inline MemAllocatorProxy(char const *aLabel, size_t const &);

    /**
     * Allocate one element from the pool
     */
    void *alloc();

    /**
     * Free a element allocated by MemAllocatorProxy::alloc()
     */
    void free(void *);

    int inUseCount() const;
    size_t objectSize() const;
    MemPoolMeter const &getMeter() const;

    /**
     \param stats	Object to be filled with statistical data about pool.
     \retval		Number of objects in use, ie. allocated.
     */
    int getStats(MemPoolStats * stats);

    char const * objectType() const;
private:
    MemAllocator *getAllocator() const;
    const char *label;
    size_t size;
    mutable MemAllocator *theAllocator;
};

/* help for classes */

/**
 \ingroup MemPoolsAPI
 \hideinitializer
 *
 * This macro is intended for use within the declaration of a class.
 */
#define MEMPROXY_CLASS(CLASS) \
    inline void *operator new(size_t); \
    inline void operator delete(void *); \
    static inline MemAllocatorProxy &Pool()

/**
 \ingroup MemPoolsAPI
 \hideinitializer
 *
 * This macro is intended for use within the .h or .cci of a class as appropriate.
 */
#define MEMPROXY_CLASS_INLINE(CLASS) \
MemAllocatorProxy& CLASS::Pool() \
{ \
    static MemAllocatorProxy thePool(#CLASS, sizeof (CLASS)); \
    return thePool; \
} \
\
void * \
CLASS::operator new (size_t byteCount) \
{ \
    /* derived classes with different sizes must implement their own new */ \
    assert (byteCount == sizeof (CLASS)); \
\
    return Pool().alloc(); \
}  \
\
void \
CLASS::operator delete (void *address) \
{ \
    Pool().free(address); \
}

/// \ingroup MemPoolsAPI
class MemImplementingAllocator : public MemAllocator
{
public:
    MemImplementingAllocator(char const *aLabel, size_t aSize);
    virtual ~MemImplementingAllocator();
    virtual MemPoolMeter const &getMeter() const;
    virtual MemPoolMeter &getMeter();
    virtual void flushMetersFull();
    virtual void flushMeters();

    /**
     * Allocate one element from the pool
     */
    virtual void *alloc();

    /**
     * Free a element allocated by MemImplementingAllocator::alloc()
     */
    virtual void free(void *);

    virtual bool idleTrigger(int shift) const = 0;
    virtual void clean(time_t maxage) = 0;
    virtual size_t objectSize() const;
    virtual int getInUseCount() = 0;
protected:
    virtual void *allocate() = 0;
    virtual void deallocate(void *, bool aggressive) = 0;
    MemPoolMeter meter;
    int memPID;
public:
    MemImplementingAllocator *next;
public:
    size_t alloc_calls;
    size_t free_calls;
    size_t saved_calls;
    size_t obj_size;
};

/// \ingroup MemPoolsAPI
class MemPoolStats
{
public:
    MemAllocator *pool;
    const char *label;
    MemPoolMeter *meter;
    int obj_size;
    int chunk_capacity;
    int chunk_size;

    int chunks_alloc;
    int chunks_inuse;
    int chunks_partial;
    int chunks_free;

    int items_alloc;
    int items_inuse;
    int items_idle;

    int overhead;
};

/// \ingroup MemPoolsAPI
/// \todo Classify and add constructor/destructor to initialize properly.
struct _MemPoolGlobalStats {
    MemPoolMeter *TheMeter;

    int tot_pools_alloc;
    int tot_pools_inuse;
    int tot_pools_mempid;

    int tot_chunks_alloc;
    int tot_chunks_inuse;
    int tot_chunks_partial;
    int tot_chunks_free;

    int tot_items_alloc;
    int tot_items_inuse;
    int tot_items_idle;

    int tot_overhead;
    int mem_idle_limit;
};

/// \ingroup MemPoolsAPI
#define memPoolCreate MemPools::GetInstance().create

/* Allocator API */
/**
 \ingroup MemPoolsAPI
 * Initialise iteration through all of the pools.
 \retval  Iterator for use by memPoolIterateNext() and memPoolIterateDone()
 */
extern MemPoolIterator * memPoolIterate(void);

/**
 \ingroup MemPoolsAPI
 * Get next pool pointer, until getting NULL pointer.
 */
extern MemImplementingAllocator * memPoolIterateNext(MemPoolIterator * iter);

/**
 \ingroup MemPoolsAPI
 * Should be called after finished with iterating through all pools.
 */
extern void memPoolIterateDone(MemPoolIterator ** iter);

/**
 \ingroup MemPoolsAPI
 \todo Stats API - not sured how to refactor yet
 *
 * Fills MemPoolGlobalStats with statistical data about overall
 * usage for all pools.
 *
 \retval  Number of pools that have at least one object in use.
 *        Ie. number of dirty pools.
 */
extern int memPoolGetGlobalStats(MemPoolGlobalStats * stats);

/// \ingroup MemPoolsAPI
extern int memPoolInUseCount(MemAllocator *);
/// \ingroup MemPoolsAPI
extern int memPoolsTotalAllocated(void);

MemAllocatorProxy::MemAllocatorProxy(char const *aLabel, size_t const &aSize) : label (aLabel), size(aSize), theAllocator (NULL)
{
}


#endif /* _MEM_POOL_H_ */
