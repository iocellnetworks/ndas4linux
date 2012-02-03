/*
 -------------------------------------------------------------------------
 Copyright (c) 2002-2005, XIMETA, Inc., IRVINE, CA, USA.
 All rights reserved.

 LICENSE TERMS

 The free distribution and use of this software in both source and binary 
 form is allowed (with or without changes) provided that:

   1. distributions of this source code include the above copyright 
      notice, this list of conditions and the following disclaimer;

   2. distributions in binary form include the above copyright
      notice, this list of conditions and the following disclaimer
      in the documentation and/or other associated materials;

   3. the copyright holder's name is not used to endorse products 
      built using this software without specific written permission. 
      
 ALTERNATIVELY, provided that this notice is retained in full, this product
 may be distributed under the terms of the GNU General Public License (GPL),
 in which case the provisions of the GPL apply INSTEAD OF those given above.
 
 DISCLAIMER

 This software is provided 'as is' with no explcit or implied warranties
 in respect of any properties, including, but not limited to, correctness 
 and fitness for purpose.
 -------------------------------------------------------------------------
*/
#ifndef _SAL_SYS_LIBC_H_ 
#define _SAL_SYS_LIBC_H_

#include "sal/sal.h"

/* String functions from newlib */

/* 32bit machine */
#define SAL_LONG_MAX 2147483647L
#undef PREFER_SIZE_OVER_SPEED
#define _DEFUN(name, arglist, args)     name(args)
#define _AND ,
#define _CONST const
#define _PTR void*
typedef unsigned int sal_size_t;


/* String functions from newlib */

#define UNALIGNED1(X)   ((long)X & (LBLOCKSIZE - 1))
/* Nonzero if either X or Y is not aligned on a "long" boundary.  */
#define UNALIGNED2(X, Y) \
  (((long)X & (sizeof (long) - 1)) | ((long)Y & (sizeof (long) - 1)))

/* DETECTNULL returns nonzero if (long)X contains a NULL byte. */
#if SAL_LONG_MAX == 2147483647L
#define DETECTNULL(X) (((X) - 0x01010101) & ~(X) & 0x80808080)
#else
#if SAL_LONG_MAX == 9223372036854775807L
#define DETECTNULL(X) (((X) - 0x0101010101010101) & ~(X) & 0x8080808080808080)
#else
#error long int is not a 32bit or 64bit type.
#endif
#endif

#ifndef DETECTNULL
#error long int is not a 32bit or 64bit byte
#endif

#define LBLOCKSIZE (sizeof(long))
#define TOO_SMALL(LEN) ((LEN) < LBLOCKSIZE)
/* How many bytes are copied each iteration of the 4X unrolled loop.  */
#define BIGBLOCKSIZE    (sizeof (long) << 2)

/* How many bytes are copied each iteration of the word copy loop.  */
#define LITTLEBLOCKSIZE (sizeof (long))


/*
FUNCTION
    <<strcmp>>---character string compare
    
INDEX
    strcmp

ANSI_SYNOPSIS
    #include <string.h>
    int strcmp(const char *<[a]>, const char *<[b]>);

TRAD_SYNOPSIS
    #include <string.h>
    int strcmp(<[a]>, <[b]>)
    char *<[a]>;
    char *<[b]>;

DESCRIPTION
    <<strcmp>> compares the string at <[a]> to
    the string at <[b]>.

RETURNS
    If <<*<[a]>>> sorts lexicographically after <<*<[b]>>>,
    <<strcmp>> returns a number greater than zero.  If the two
    strings match, <<strcmp>> returns zero.  If <<*<[a]>>>
    sorts lexicographically before <<*<[b]>>>, <<strcmp>> returns a
    number less than zero.

PORTABILITY
<<strcmp>> is ANSI C.

<<strcmp>> requires no supporting OS subroutines.

QUICKREF
    strcmp ansi pure
*/


static inline int
_DEFUN (sal_strcmp, (s1, s2),
    _CONST char *s1 _AND
    _CONST char *s2)
{ 
#if defined(PREFER_SIZE_OVER_SPEED) || defined(__OPTIMIZE_SIZE__)
  while (*s1 != '\0' && *s1 == *s2)
    {
      s1++;
      s2++;
    }

  return (*(unsigned char *) s1) - (*(unsigned char *) s2);
#else
  unsigned long *a1;
  unsigned long *a2;

  /* If s1 or s2 are unaligned, then compare bytes. */
  if (!UNALIGNED2 (s1, s2))
    {  
      /* If s1 and s2 are word-aligned, compare them a word at a time. */
      a1 = (unsigned long*)s1;
      a2 = (unsigned long*)s2;
      while (*a1 == *a2)
        {
          /* To get here, *a1 == *a2, thus if we find a null in *a1,
         then the strings must be equal, so return zero.  */
          if (DETECTNULL (*a1))
        return 0;

          a1++;
          a2++;
        }

      /* A difference was detected in last few bytes of s1, so search bytewise */
      s1 = (char*)a1;
      s2 = (char*)a2;
    }

  while (*s1 != '\0' && *s1 == *s2)
    {
      s1++;
      s2++;
    }
  return (*(unsigned char *) s1) - (*(unsigned char *) s2);
#endif /* not PREFER_SIZE_OVER_SPEED */
}

/*
FUNCTION
    <<strncmp>>---character string compare
    
INDEX
    strncmp

ANSI_SYNOPSIS
    #include <string.h>
    int strncmp(const char *<[a]>, const char * <[b]>, sal_size_t <[length]>);

TRAD_SYNOPSIS
    #include <string.h>
    int strncmp(<[a]>, <[b]>, <[length]>)
    char *<[a]>;
    char *<[b]>;
    sal_size_t <[length]>

DESCRIPTION
    <<strncmp>> compares up to <[length]> characters
    from the string at <[a]> to the string at <[b]>.

RETURNS
    If <<*<[a]>>> sorts lexicographically after <<*<[b]>>>,
    <<strncmp>> returns a number greater than zero.  If the two
    strings are equivalent, <<strncmp>> returns zero.  If <<*<[a]>>>
    sorts lexicographically before <<*<[b]>>>, <<strncmp>> returns a
    number less than zero.

PORTABILITY
<<strncmp>> is ANSI C.

<<strncmp>> requires no supporting OS subroutines.

QUICKREF
    strncmp ansi pure
*/

static inline int 
_DEFUN (sal_strncmp, (s1, s2, n),
    _CONST char *s1 _AND
    _CONST char *s2 _AND
    sal_size_t n)
{
#if defined(PREFER_SIZE_OVER_SPEED) || defined(__OPTIMIZE_SIZE__)
  if (n == 0)
    return 0;

  while (n-- != 0 && *s1 == *s2)
    {
      if (n == 0 || *s1 == '\0')
    break;
      s1++;
      s2++;
    }

  return (*(unsigned char *) s1) - (*(unsigned char *) s2);
#else
  unsigned long *a1;
  unsigned long *a2;

  if (n == 0)
    return 0;

  /* If s1 or s2 are unaligned, then compare bytes. */
  if (!UNALIGNED2 (s1, s2))
    {
      /* If s1 and s2 are word-aligned, compare them a word at a time. */
      a1 = (unsigned long*)s1;
      a2 = (unsigned long*)s2;
      while (n >= sizeof (long) && *a1 == *a2)
        {
          n -= sizeof (long);

          /* If we've run out of bytes or hit a null, return zero
         since we already know *a1 == *a2.  */
          if (n == 0 || DETECTNULL (*a1))
        return 0;

          a1++;
          a2++;
        }

      /* A difference was detected in last few bytes of s1, so search bytewise */
      s1 = (char*)a1;
      s2 = (char*)a2;
    }

  while (n-- > 0 && *s1 == *s2)
    {
      /* If we've run out of bytes or hit a null, return zero
     since we already know *s1 == *s2.  */
      if (n == 0 || *s1 == '\0')
    return 0;
      s1++;
      s2++;
    }
  return (*(unsigned char *) s1) - (*(unsigned char *) s2);
#endif /* not PREFER_SIZE_OVER_SPEED */
}

/*
FUNCTION
    <<strcpy>>---copy string

INDEX
    strcpy

ANSI_SYNOPSIS
    #include <string.h>
    char *strcpy(char *<[dst]>, const char *<[src]>);

TRAD_SYNOPSIS
    #include <string.h>
    char *strcpy(<[dst]>, <[src]>)
    char *<[dst]>;
    char *<[src]>;

DESCRIPTION
    <<strcpy>> copies the string pointed to by <[src]>
    (including the terminating null character) to the array
    pointed to by <[dst]>.

RETURNS
    This function returns the initial value of <[dst]>.

PORTABILITY
<<strcpy>> is ANSI C.

<<strcpy>> requires no supporting OS subroutines.

QUICKREF
    strcpy ansi pure
*/

static inline char*
_DEFUN (sal_strcpy, (dst0, src0),
    char *dst0 _AND
    _CONST char *src0)
{
#if defined(PREFER_SIZE_OVER_SPEED) || defined(__OPTIMIZE_SIZE__)
  char *s = dst0;

  while ((*dst0++ = *src0++))
    ;

  return s;
#else
  char *dst = dst0;
  _CONST char *src = src0;
  long *aligned_dst;
  _CONST long *aligned_src;

  /* If SRC or DEST is unaligned, then copy bytes.  */
  if (!UNALIGNED2 (src, dst))
    {
      aligned_dst = (long*)dst;
      aligned_src = (long*)src;

      /* SRC and DEST are both "long int" aligned, try to do "long int"
         sized copies.  */
      while (!DETECTNULL(*aligned_src))
        {
          *aligned_dst++ = *aligned_src++;
        }

      dst = (char*)aligned_dst;
      src = (char*)aligned_src;
    }

  while ((*dst++ = *src++))
    ;
  return dst0;
#endif /* not PREFER_SIZE_OVER_SPEED */
}

/*
FUNCTION
    <<strncpy>>---counted copy string

INDEX
    strncpy

ANSI_SYNOPSIS
    #include <string.h>
    char *strncpy(char *<[dst]>, const char *<[src]>, sal_size_t <[length]>);

TRAD_SYNOPSIS
    #include <string.h>
    char *strncpy(<[dst]>, <[src]>, <[length]>)
    char *<[dst]>;
    char *<[src]>;
    sal_size_t <[length]>;

DESCRIPTION
    <<strncpy>> copies not more than <[length]> characters from the
    the string pointed to by <[src]> (including the terminating
    null character) to the array pointed to by <[dst]>.  If the
    string pointed to by <[src]> is shorter than <[length]>
    characters, null characters are appended to the destination
    array until a total of <[length]> characters have been
    written.

RETURNS
    This function returns the initial value of <[dst]>.

PORTABILITY
<<strncpy>> is ANSI C.

<<strncpy>> requires no supporting OS subroutines.

QUICKREF
    strncpy ansi pure
*/

static inline char *
_DEFUN (sal_strncpy, (dst0, src0),
    char *dst0 _AND
    _CONST char *src0 _AND
    sal_size_t count)
{
#if defined(PREFER_SIZE_OVER_SPEED) || defined(__OPTIMIZE_SIZE__)
  char *dscan;
  _CONST char *sscan;

  dscan = dst0;
  sscan = src0;
  while (count > 0)
    {
      --count;
      if ((*dscan++ = *sscan++) == '\0')
    break;
    }
  while (count-- > 0)
    *dscan++ = '\0';

  return dst0;
#else
  char *dst = dst0;
  _CONST char *src = src0;
  long *aligned_dst;
  _CONST long *aligned_src;

  /* If SRC and DEST is aligned and count large enough, then copy words.  */
  if (!UNALIGNED2 (src, dst) && !TOO_SMALL (count))
    {
      aligned_dst = (long*)dst;
      aligned_src = (long*)src;

      /* SRC and DEST are both "long int" aligned, try to do "long int"
     sized copies.  */
      while (count >= sizeof (long int) && !DETECTNULL(*aligned_src))
    {
      count -= sizeof (long int);
      *aligned_dst++ = *aligned_src++;
    }

      dst = (char*)aligned_dst;
      src = (char*)aligned_src;
    }

  while (count > 0)
    {
      --count;
      if ((*dst++ = *src++) == '\0')
    break;
    }

  while (count-- > 0)
    *dst++ = '\0';

  return dst0;
#endif /* not PREFER_SIZE_OVER_SPEED */
}

/*
FUNCTION
    <<strcat>>---concatenate strings

INDEX
    strcat

ANSI_SYNOPSIS
    #include <string.h>
    char *strcat(char *<[dst]>, const char *<[src]>);

TRAD_SYNOPSIS
    #include <string.h>
    char *strcat(<[dst]>, <[src]>)
    char *<[dst]>;
    char *<[src]>;

DESCRIPTION
    <<strcat>> appends a copy of the string pointed to by <[src]>
    (including the terminating null character) to the end of the
    string pointed to by <[dst]>.  The initial character of
    <[src]> overwrites the null character at the end of <[dst]>.

RETURNS
    This function returns the initial value of <[dst]>

PORTABILITY
<<strcat>> is ANSI C.

<<strcat>> requires no supporting OS subroutines.

QUICKREF
    strcat ansi pure
*/

/* Nonzero if X is aligned on a "long" boundary.  */
#define ALIGNED(X) \
  (((long)X & (sizeof (long) - 1)) == 0)


static inline char *
_DEFUN (sal_strcat, (s1, s2),
    char *s1 _AND
    _CONST char *s2)
{
#if defined(PREFER_SIZE_OVER_SPEED) || defined(__OPTIMIZE_SIZE__)
  char *s = s1;

  while (*s1)
    s1++;

  while ((*s1++ = *s2++))
    ;
  return s;
#else
  char *s = s1;


  /* Skip over the data in s1 as quickly as possible.  */
  if (ALIGNED (s1))
    {
      unsigned long *aligned_s1 = (unsigned long *)s1;
      while (!DETECTNULL (*aligned_s1))
    aligned_s1++;

      s1 = (char *)aligned_s1;
    }

  while (*s1)
    s1++;

  /* s1 now points to the its trailing null character, we can
     just use strcpy to do the work for us now.

     ?!? We might want to just include strcpy here.
     Also, this will cause many more unaligned string copies because
     s1 is much less likely to be aligned.  I don't know if its worth
     tweaking strcpy to handle this better.  */
  sal_strcpy (s1, s2);
    
  return s;
#endif /* not PREFER_SIZE_OVER_SPEED */
}


/* 
FUNCTION
    <<strlen>>---character string length
    
INDEX
    strlen

ANSI_SYNOPSIS
    #include <string.h>
    sal_size_t strlen(const char *<[str]>);

TRAD_SYNOPSIS
    #include <string.h>
    sal_size_t strlen(<[str]>)
    char *<[src]>;

DESCRIPTION
    The <<strlen>> function works out the length of the string
    starting at <<*<[str]>>> by counting chararacters until it
    reaches a <<NULL>> character.

RETURNS
    <<strlen>> returns the character count.

PORTABILITY
<<strlen>> is ANSI C.

<<strlen>> requires no supporting OS subroutines.

QUICKREF
    strlen ansi pure
*/

static inline sal_size_t
_DEFUN (sal_strlen, (str),
    _CONST char *str)
{
#if defined(PREFER_SIZE_OVER_SPEED) || defined(__OPTIMIZE_SIZE__)
  _CONST char *start = str;

  while (*str)
    str++;

  return str - start;
#else
  _CONST char *start = str;
  unsigned long *aligned_addr;

  if (!UNALIGNED1 (str))
    {
      /* If the string is word-aligned, we can check for the presence of 
         a null in each word-sized block.  */
      aligned_addr = (unsigned long*)str;
      while (!DETECTNULL (*aligned_addr))
        aligned_addr++;

      /* Once a null is detected, we check each byte in that block for a
         precise position of the null.  */
      str = (char*)aligned_addr;
    }
 
  while (*str)
    str++;
  return str - start;
#endif /* not PREFER_SIZE_OVER_SPEED */
}
/* 
FUNCTION
        <<strnlen>>---character string length
        
INDEX
        strnlen

ANSI_SYNOPSIS
        #include <string.h>
        sal_size_t strnlen(const char *<[str]>, sal_size_t <[n]>);

TRAD_SYNOPSIS
        #include <string.h>
        sal_size_t strnlen(<[str]>, <[n]>)
        char *<[src]>;
        sal_size_t <[n]>;

DESCRIPTION
        The <<strnlen>> function works out the length of the string
        starting at <<*<[str]>>> by counting chararacters until it
        reaches a NUL character or the maximum: <[n]> number of
        characters have been inspected.

RETURNS
        <<strnlen>> returns the character count or <[n]>.

PORTABILITY
<<strnlen>> is a Gnu extension.

<<strnlen>> requires no supporting OS subroutines.

*/

static inline sal_size_t
_DEFUN (sal_strnlen, (str, n),
        _CONST char *str _AND
        sal_size_t n)
{
  _CONST char *start = str;

  while (*str && n-- > 0)
    str++;

  return str - start;
}

/*
FUNCTION
    <<memset>>---set an area of memory

INDEX
    memset

ANSI_SYNOPSIS
    #include <string.h>
    void *memset(const void *<[dst]>, int <[c]>, sal_size_t <[length]>);

TRAD_SYNOPSIS
    #include <string.h>
    void *memset(<[dst]>, <[c]>, <[length]>)
    void *<[dst]>;
    int <[c]>;
    sal_size_t <[length]>;

DESCRIPTION
    This function converts the argument <[c]> into an unsigned
    char and fills the first <[length]> characters of the array
    pointed to by <[dst]> to the value.

RETURNS
    <<memset>> returns the value of <[m]>.

PORTABILITY
<<memset>> is ANSI C.

    <<memset>> requires no supporting OS subroutines.

QUICKREF
    memset ansi pure
*/



static inline _PTR 
_DEFUN (sal_memset, (m, c, n),
    _PTR m _AND
    int c _AND
    sal_size_t n)
{
#if defined(PREFER_SIZE_OVER_SPEED) || defined(__OPTIMIZE_SIZE__)
  char *s = (char *) m;

  while (n-- != 0)
    {
      *s++ = (char) c;
    }

  return m;
#else
  char *s = (char *) m;
  unsigned int i;
  unsigned long buffer;
  unsigned long *aligned_addr;
  unsigned int d = c & 0xff;    /* To avoid sign extension, copy C to an
                   unsigned variable.  */

  if (!TOO_SMALL (n) && !UNALIGNED1 (m))
    {
      /* If we get this far, we know that n is large and m is word-aligned. */
      aligned_addr = (unsigned long*)m;

      /* Store D into each char sized location in BUFFER so that
         we can set large blocks quickly.  */
      if (LBLOCKSIZE == 4)
        {
          buffer = (d << 8) | d;
          buffer |= (buffer << 16);
        }
      else
        {
          buffer = 0;
          for (i = 0; i < LBLOCKSIZE; i++)
        buffer = (buffer << 8) | d;
        }

      while (n >= LBLOCKSIZE*4)
        {
          *aligned_addr++ = buffer;
          *aligned_addr++ = buffer;
          *aligned_addr++ = buffer;
          *aligned_addr++ = buffer;
          n -= 4*LBLOCKSIZE;
        }

      while (n >= LBLOCKSIZE)
        {
          *aligned_addr++ = buffer;
          n -= LBLOCKSIZE;
        }
      /* Pick up the remainder with a bytewise loop.  */
      s = (char*)aligned_addr;
    }

  while (n--)
    {
      *s++ = (char)d;
    }

  return m;
#endif /* not PREFER_SIZE_OVER_SPEED */
}

/*
FUNCTION
        <<memcpy>>---copy memory regions

ANSI_SYNOPSIS
        #include <string.h>
        void* memcpy(void *<[out]>, const void *<[in]>, sal_size_t <[n]>);

TRAD_SYNOPSIS
        void *memcpy(<[out]>, <[in]>, <[n]>
        void *<[out]>;
        void *<[in]>;
        sal_size_t <[n]>;

DESCRIPTION
        This function copies <[n]> bytes from the memory region
        pointed to by <[in]> to the memory region pointed to by
        <[out]>.

        If the regions overlap, the behavior is undefined.

RETURNS
        <<memcpy>> returns a pointer to the first byte of the <[out]>
        region.

PORTABILITY
<<memcpy>> is ANSI C.

<<memcpy>> requires no supporting OS subroutines.

QUICKREF
        memcpy ansi pure
        */

static inline _PTR
_DEFUN (sal_memcpy, (dst0, src0, len0),
        _PTR dst0 _AND
        _CONST _PTR src0 _AND
        sal_size_t len0)
{
#if defined(PREFER_SIZE_OVER_SPEED) || defined(__OPTIMIZE_SIZE__)
  char *dst = (char *) dst0;
  char *src = (char *) src0;

  _PTR save = dst0;

  while (len0--)
    {
      *dst++ = *src++;
    }

  return save;
#else
  char *dst = dst0;
  _CONST char *src = src0;
  long *aligned_dst;
  _CONST long *aligned_src;
  unsigned int   len =  len0;

  /* If the size is small, or either SRC or DST is unaligned,
     then punt into the byte copy loop.  This should be rare.  */
  if (!TOO_SMALL(len) && !UNALIGNED2 (src, dst))
    {
      aligned_dst = (long*)dst;
      aligned_src = (long*)src;

      /* Copy 4X long words at a time if possible.  */
      while (len >= BIGBLOCKSIZE)
        {
          *aligned_dst++ = *aligned_src++;
          *aligned_dst++ = *aligned_src++;
          *aligned_dst++ = *aligned_src++;
          *aligned_dst++ = *aligned_src++;
          len -= BIGBLOCKSIZE;
        }

      /* Copy one long word at a time if possible.  */
      while (len >= LITTLEBLOCKSIZE)
        {
          *aligned_dst++ = *aligned_src++;
          len -= LITTLEBLOCKSIZE;
        }

       /* Pick up any residual with a byte copier.  */
      dst = (char*)aligned_dst;
      src = (char*)aligned_src;
    }

  while (len--)
    *dst++ = *src++;

  return dst0;
#endif /* not PREFER_SIZE_OVER_SPEED */
}
/*
FUNCTION
        <<memcmp>>---compare two memory areas

INDEX
        memcmp

ANSI_SYNOPSIS
        #include <string.h>
        int memcmp(const void *<[s1]>, const void *<[s2]>, sal_size_t <[n]>);

TRAD_SYNOPSIS
        #include <string.h>
        int memcmp(<[s1]>, <[s2]>, <[n]>)
        void *<[s1]>;
        void *<[s2]>;
        sal_size_t <[n]>;

DESCRIPTION
        This function compares not more than <[n]> characters of the
        object pointed to by <[s1]> with the object pointed to by <[s2]>.


RETURNS
        The function returns an integer greater than, equal to or
        less than zero         according to whether the object pointed to by
        <[s1]> is greater than, equal to or less than the object
        pointed to by <[s2]>.

PORTABILITY
<<memcmp>> is ANSI C.

<<memcmp>> requires no supporting OS subroutines.

QUICKREF
        memcmp ansi pure
*/

static inline int
_DEFUN (sal_memcmp, (m1, m2, n),
        _CONST _PTR m1 _AND
        _CONST _PTR m2 _AND
        sal_size_t n)
{
#if defined(PREFER_SIZE_OVER_SPEED) || defined(__OPTIMIZE_SIZE__)
  unsigned char *s1 = (unsigned char *) m1;
  unsigned char *s2 = (unsigned char *) m2;

  while (n--)
    {
      if (*s1 != *s2)
        {
          return *s1 - *s2;
        }
      s1++;
      s2++;
    }
  return 0;
#else  
  unsigned char *s1 = (unsigned char *) m1;
  unsigned char *s2 = (unsigned char *) m2;
  unsigned long *a1;
  unsigned long *a2;

  /* If the size is too small, or either pointer is unaligned,
     then we punt to the byte compare loop.  Hopefully this will
     not turn up in inner loops.  */
  if (!TOO_SMALL(n) && !UNALIGNED2(s1,s2))
    {
      /* Otherwise, load and compare the blocks of memory one 
         word at a time.  */
      a1 = (unsigned long*) s1;
      a2 = (unsigned long*) s2;
      while (n >= LBLOCKSIZE)
        {
          if (*a1 != *a2) 
            break;
          a1++;
          a2++;
          n -= LBLOCKSIZE;
        }

      /* check m mod LBLOCKSIZE remaining characters */

      s1 = (unsigned char*)a1;
      s2 = (unsigned char*)a2;
    }

  while (n--)
    {
      if (*s1 != *s2)
        return *s1 - *s2;
      s1++;
      s2++;
    }

  return 0;
#endif /* not PREFER_SIZE_OVER_SPEED */
}

NDAS_SAL_API extern int
_DEFUN(sal_snprintf, (str, size, fmt),
       char *str   _AND
       sal_size_t size _AND
       _CONST char *fmt _AND ...)
       __attribute__ ((format (printf, 3, 4)));

#endif // _SAL_SYS_LIBC_H_

