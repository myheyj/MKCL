/*
 * Copyright (c) 2000-2005 by Hewlett-Packard Company.  All rights reserved.
 *
 * THIS MATERIAL IS PROVIDED AS IS, WITH ABSOLUTELY NO WARRANTY EXPRESSED
 * OR IMPLIED.  ANY USE IS AT YOUR OWN RISK.
 *
 * Permission is hereby granted to use or copy this program
 * for any purpose,  provided the above notices are retained on all copies.
 * Permission to modify the code and to distribute modified code is granted,
 * provided the above notices are retained, and a notice that the code was
 * modified is included with the above copyright notice.
 */

#include "private/gc_priv.h"

#if defined(THREAD_LOCAL_ALLOC)

#ifndef THREADS
# error "invalid config - THREAD_LOCAL_ALLOC requires MK_GC_THREADS"
#endif

#include "private/thread_local_alloc.h"

#include <stdlib.h>

#if defined(USE_COMPILER_TLS)
  __thread
#elif defined(USE_WIN32_COMPILER_TLS)
  __declspec(thread)
#endif
MK_GC_key_t MK_GC_thread_key;

static MK_GC_bool keys_initialized;

#ifdef ENABLE_DISCLAIM
  MK_GC_INNER ptr_t * MK_GC_finalized_objfreelist = NULL;
        /* This variable is declared here to prevent linking of         */
        /* fnlz_mlc module unless the client uses the latter one.       */
#endif

/* Return a single nonempty freelist fl to the global one pointed to    */
/* by gfl.                                                              */

static void return_single_freelist(void *fl, void **gfl)
{
    void *q, **qptr;

    if (*gfl == 0) {
      *gfl = fl;
    } else {
      MK_GC_ASSERT(MK_GC_size(fl) == MK_GC_size(*gfl));
      /* Concatenate: */
        qptr = &(obj_link(fl));
        while ((word)(q = *qptr) >= HBLKSIZE)
          qptr = &(obj_link(q));
        MK_GC_ASSERT(0 == q);
        *qptr = *gfl;
        *gfl = fl;
    }
}

/* Recover the contents of the freelist array fl into the global one gfl.*/
/* We hold the allocator lock.                                          */
static void return_freelists(void **fl, void **gfl)
{
    int i;

    for (i = 1; i < TINY_FREELISTS; ++i) {
        if ((word)(fl[i]) >= HBLKSIZE) {
          return_single_freelist(fl[i], gfl+i);
        }
        /* Clear fl[i], since the thread structure may hang around.     */
        /* Do it in a way that is likely to trap if we access it.       */
        fl[i] = (ptr_t)HBLKSIZE;
    }
    /* The 0 granule freelist really contains 1 granule objects.        */
#   ifdef MK_GC_GCJ_SUPPORT
      if (fl[0] == ERROR_FL) return;
#   endif
    if ((word)(fl[0]) >= HBLKSIZE) {
        return_single_freelist(fl[0], gfl+1);
    }
}

/* Each thread structure must be initialized.   */
/* This call must be made from the new thread.  */
MK_GC_INNER void MK_GC_init_thread_local(MK_GC_tlfs p)
{
    int i;

    MK_GC_ASSERT(I_HOLD_LOCK());
    if (!EXPECT(keys_initialized, TRUE)) {
        MK_GC_ASSERT((word)&MK_GC_thread_key % sizeof(word) == 0);
        if (0 != MK_GC_key_create(&MK_GC_thread_key, 0)) {
            ABORT("Failed to create key for local allocator");
        }
        keys_initialized = TRUE;
    }
    if (0 != MK_GC_setspecific(MK_GC_thread_key, p)) {
        ABORT("Failed to set thread specific allocation pointers");
    }
    for (i = 1; i < TINY_FREELISTS; ++i) {
        p -> ptrfree_freelists[i] = (void *)(word)1;
        p -> normal_freelists[i] = (void *)(word)1;
#       ifdef MK_GC_GCJ_SUPPORT
          p -> gcj_freelists[i] = (void *)(word)1;
#       endif
#       ifdef ENABLE_DISCLAIM
          p -> finalized_freelists[i] = (void *)(word)1;
#       endif
    }
    /* Set up the size 0 free lists.    */
    /* We now handle most of them like regular free lists, to ensure    */
    /* That explicit deallocation works.  However, allocation of a      */
    /* size 0 "gcj" object is always an error.                          */
    p -> ptrfree_freelists[0] = (void *)(word)1;
    p -> normal_freelists[0] = (void *)(word)1;
#   ifdef MK_GC_GCJ_SUPPORT
        p -> gcj_freelists[0] = ERROR_FL;
#   endif
#   ifdef ENABLE_DISCLAIM
        p -> finalized_freelists[0] = (void *)(word)1;
#   endif
}

/* We hold the allocator lock.  */
MK_GC_INNER void MK_GC_destroy_thread_local(MK_GC_tlfs p)
{
    /* We currently only do this from the thread itself or from */
    /* the fork handler for a child process.                    */
    return_freelists(p -> ptrfree_freelists, MK_GC_aobjfreelist);
    return_freelists(p -> normal_freelists, MK_GC_objfreelist);
#   ifdef MK_GC_GCJ_SUPPORT
        return_freelists(p -> gcj_freelists, (void **)MK_GC_gcjobjfreelist);
#   endif
#   ifdef ENABLE_DISCLAIM
        return_freelists(p -> finalized_freelists,
                         (void **)MK_GC_finalized_objfreelist);
#   endif
}

#ifdef MK_GC_ASSERTIONS
  /* Defined in pthread_support.c or win32_threads.c. */
  MK_GC_bool MK_GC_is_thread_tsd_valid(void *tsd);
#endif

MK_GC_API void * MK_GC_CALL MK_GC_malloc(size_t bytes)
{
    size_t granules = ROUNDED_UP_GRANULES(bytes);
    void *tsd;
    void *result;
    void **tiny_fl;

#   if !defined(USE_PTHREAD_SPECIFIC) && !defined(USE_WIN32_SPECIFIC)
      MK_GC_key_t k = MK_GC_thread_key;
      if (EXPECT(0 == k, FALSE)) {
        /* We haven't yet run MK_GC_init_parallel.  That means     */
        /* we also aren't locking, so this is fairly cheap.     */
        return MK_GC_core_malloc(bytes);
      }
      tsd = MK_GC_getspecific(k);
#   else
      tsd = MK_GC_getspecific(MK_GC_thread_key);
#   endif
#   if !defined(USE_COMPILER_TLS) && !defined(USE_WIN32_COMPILER_TLS)
      if (EXPECT(0 == tsd, FALSE)) {
        return MK_GC_core_malloc(bytes);
      }
#   endif
    MK_GC_ASSERT(MK_GC_is_initialized);

    MK_GC_ASSERT(MK_GC_is_thread_tsd_valid(tsd));

    tiny_fl = ((MK_GC_tlfs)tsd) -> normal_freelists;
    MK_GC_FAST_MALLOC_GRANS(result, granules, tiny_fl, DIRECT_GRANULES,
                         NORMAL, MK_GC_core_malloc(bytes), obj_link(result)=0);
#   ifdef LOG_ALLOCS
      MK_GC_log_printf("MK_GC_malloc(%lu) returned %p, recent GC #%lu\n",
                    (unsigned long)bytes, result, (unsigned long)MK_GC_gc_no);
#   endif
    return result;
}

MK_GC_API void * MK_GC_CALL MK_GC_malloc_atomic(size_t bytes)
{
    size_t granules = ROUNDED_UP_GRANULES(bytes);
    void *tsd;
    void *result;
    void **tiny_fl;

#   if !defined(USE_PTHREAD_SPECIFIC) && !defined(USE_WIN32_SPECIFIC)
      MK_GC_key_t k = MK_GC_thread_key;
      if (EXPECT(0 == k, FALSE)) {
        /* We haven't yet run MK_GC_init_parallel.  That means     */
        /* we also aren't locking, so this is fairly cheap.     */
        return MK_GC_core_malloc_atomic(bytes);
      }
      tsd = MK_GC_getspecific(k);
#   else
      tsd = MK_GC_getspecific(MK_GC_thread_key);
#   endif
#   if !defined(USE_COMPILER_TLS) && !defined(USE_WIN32_COMPILER_TLS)
      if (EXPECT(0 == tsd, FALSE)) {
        return MK_GC_core_malloc_atomic(bytes);
      }
#   endif
    MK_GC_ASSERT(MK_GC_is_initialized);
    tiny_fl = ((MK_GC_tlfs)tsd) -> ptrfree_freelists;
    MK_GC_FAST_MALLOC_GRANS(result, granules, tiny_fl, DIRECT_GRANULES, PTRFREE,
                         MK_GC_core_malloc_atomic(bytes), (void)0 /* no init */);
    return result;
}

#ifdef MK_GC_GCJ_SUPPORT

# include "atomic_ops.h" /* for MK_AO_compiler_barrier() */

# include "include/gc_gcj.h"

/* Gcj-style allocation without locks is extremely tricky.  The         */
/* fundamental issue is that we may end up marking a free list, which   */
/* has freelist links instead of "vtable" pointers.  That is usually    */
/* OK, since the next object on the free list will be cleared, and      */
/* will thus be interpreted as containing a zero descriptor.  That's    */
/* fine if the object has not yet been initialized.  But there are      */
/* interesting potential races.                                         */
/* In the case of incremental collection, this seems hopeless, since    */
/* the marker may run asynchronously, and may pick up the pointer to    */
/* the next freelist entry (which it thinks is a vtable pointer), get   */
/* suspended for a while, and then see an allocated object instead      */
/* of the vtable.  This may be avoidable with either a handshake with   */
/* the collector or, probably more easily, by moving the free list      */
/* links to the second word of each object.  The latter isn't a         */
/* universal win, since on architecture like Itanium, nonzero offsets   */
/* are not necessarily free.  And there may be cache fill order issues. */
/* For now, we punt with incremental GC.  This probably means that      */
/* incremental GC should be enabled before we fork a second thread.     */
/* Unlike the other thread local allocation calls, we assume that the   */
/* collector has been explicitly initialized.                           */
MK_GC_API void * MK_GC_CALL MK_GC_gcj_malloc(size_t bytes,
                                    void * ptr_to_struct_containing_descr)
{
  if (EXPECT(MK_GC_incremental, FALSE)) {
    return MK_GC_core_gcj_malloc(bytes, ptr_to_struct_containing_descr);
  } else {
    size_t granules = ROUNDED_UP_GRANULES(bytes);
    void *result;
    void **tiny_fl = ((MK_GC_tlfs)MK_GC_getspecific(MK_GC_thread_key))
                                        -> gcj_freelists;
    MK_GC_ASSERT(MK_GC_gcj_malloc_initialized);
    MK_GC_FAST_MALLOC_GRANS(result, granules, tiny_fl, DIRECT_GRANULES,
                         MK_GC_gcj_kind,
                         MK_GC_core_gcj_malloc(bytes,
                                            ptr_to_struct_containing_descr),
                         {MK_AO_compiler_barrier();
                          *(void **)result = ptr_to_struct_containing_descr;});
        /* This forces the initialization of the "method ptr".          */
        /* This is necessary to ensure some very subtle properties      */
        /* required if a GC is run in the middle of such an allocation. */
        /* Here we implicitly also assume atomicity for the free list.  */
        /* and method pointer assignments.                              */
        /* We must update the freelist before we store the pointer.     */
        /* Otherwise a GC at this point would see a corrupted           */
        /* free list.                                                   */
        /* A real memory barrier is not needed, since the               */
        /* action of stopping this thread will cause prior writes       */
        /* to complete.                                                 */
        /* We assert that any concurrent marker will stop us.           */
        /* Thus it is impossible for a mark procedure to see the        */
        /* allocation of the next object, but to see this object        */
        /* still containing a free list pointer.  Otherwise the         */
        /* marker, by misinterpreting the freelist link as a vtable     */
        /* pointer, might find a random "mark descriptor" in the next   */
        /* object.                                                      */
    return result;
  }
}

#endif /* MK_GC_GCJ_SUPPORT */

/* The thread support layer must arrange to mark thread-local   */
/* free lists explicitly, since the link field is often         */
/* invisible to the marker.  It knows how to find all threads;  */
/* we take care of an individual thread freelist structure.     */
MK_GC_INNER void MK_GC_mark_thread_local_fls_for(MK_GC_tlfs p)
{
    ptr_t q;
    int j;

    for (j = 0; j < TINY_FREELISTS; ++j) {
      q = p -> ptrfree_freelists[j];
      if ((word)q > HBLKSIZE) MK_GC_set_fl_marks(q);
      q = p -> normal_freelists[j];
      if ((word)q > HBLKSIZE) MK_GC_set_fl_marks(q);
#     ifdef MK_GC_GCJ_SUPPORT
        if (j > 0) {
          q = p -> gcj_freelists[j];
          if ((word)q > HBLKSIZE) MK_GC_set_fl_marks(q);
        }
#     endif /* MK_GC_GCJ_SUPPORT */
#     ifdef ENABLE_DISCLAIM
        q = p -> finalized_freelists[j];
        if ((word)q > HBLKSIZE)
          MK_GC_set_fl_marks(q);
#     endif
    }
}

#if defined(MK_GC_ASSERTIONS)
    /* Check that all thread-local free-lists in p are completely marked. */
    void MK_GC_check_tls_for(MK_GC_tlfs p)
    {
        int j;

        for (j = 1; j < TINY_FREELISTS; ++j) {
          MK_GC_check_fl_marks(&p->ptrfree_freelists[j]);
          MK_GC_check_fl_marks(&p->normal_freelists[j]);
#         ifdef MK_GC_GCJ_SUPPORT
            MK_GC_check_fl_marks(&p->gcj_freelists[j]);
#         endif
#         ifdef ENABLE_DISCLAIM
            MK_GC_check_fl_marks(&p->finalized_freelists[j]);
#         endif
        }
    }
#endif /* MK_GC_ASSERTIONS */

#endif /* THREAD_LOCAL_ALLOC */
