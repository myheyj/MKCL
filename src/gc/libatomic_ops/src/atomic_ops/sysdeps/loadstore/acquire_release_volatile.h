/*
 * Copyright (c) 2003-2004 Hewlett-Packard Development Company, L.P.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

/* This file adds definitions appropriate for environments in which     */
/* volatile load of a given type has acquire semantics, and volatile    */
/* store of a given type has release semantics.  This is arguably       */
/* supposed to be true with the standard Itanium software conventions.  */
/* Empirically gcc/ia64 does some reordering of ordinary operations     */
/* around volatiles even when we think it should not.  GCC v3.3 and     */
/* earlier could reorder a volatile store with another store.  As of    */
/* March 2005, gcc pre-4 reuses some previously computed common         */
/* subexpressions across a volatile load; hence, we now add compiler    */
/* barriers for gcc.                                                    */

#ifndef MK_AO_GCC_BARRIER
  /* TODO: Check GCC version (if workaround not needed for modern GCC). */
# if defined(__GNUC__)
#   define MK_AO_GCC_BARRIER() MK_AO_compiler_barrier()
# else
#   define MK_AO_GCC_BARRIER() (void)0
# endif
#endif

MK_AO_INLINE MK_AO_t
MK_AO_load_acquire(const volatile MK_AO_t *addr)
{
  MK_AO_t result = *addr;

  /* A normal volatile load generates an ld.acq (on IA-64).     */
  MK_AO_GCC_BARRIER();
  return result;
}
#define MK_AO_HAVE_load_acquire

MK_AO_INLINE void
MK_AO_store_release(volatile MK_AO_t *addr, MK_AO_t new_val)
{
  MK_AO_GCC_BARRIER();
  /* A normal volatile store generates an st.rel (on IA-64).    */
  *addr = new_val;
}
#define MK_AO_HAVE_store_release
