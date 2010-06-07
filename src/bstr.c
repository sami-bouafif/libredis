/* bstr.c
 *
 * Copyright (C) 2010
 *        Sami Bouafif <sami.bouafif@gmail.com>. All Rights Reserved.
 *
 * This file is part of libredis.
 *
 * libredis is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * libredis is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */


#include <string.h>
#include <printf.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

#include <bstr.h>
#include <config.h>

/**
 * SECTION:bstr
 * @short_description: Typedef and functions to support binary string.
 * @title: Binary String support
 * @section_id:
 * @include: bstr.h
 *
 * A collection of function and a structure to support strings that can contain
 * one or more <code>\0</code> characters. These functions implements basic
 * string operations similar to these applicable to standard string format.
 **/


/*
 * This struct has no meaning (it is often used as char *).
 * Its a little trick to prevent implicit conversion from bstr_t to char *.
 * This conversion must ALWAYS be explicit (A conscious act) as bstr_t may often
 * contain some '\0' . :-).
 * So don't pay attention to the members of the struct. All allocation and access
 * is done manually.
 */
struct _bstr_t
{
  size_t  size;
  char    str[];
};

static short bstr_initDone = 0;
static int PA_BSTR;

/* This function copy the argument from the va_list */
static void _bstr_va (void *mem, va_list *ap)
{
  /* get the arg */
  bstr_t bstr = va_arg(*ap, bstr_t);
  /* copy to mem */
  memcpy (mem, &bstr, sizeof(bstr_t));
}

/* This is the fuction that will display bstr */
static int _bstr_print(FILE *stream,
                       const struct printf_info *info,
                       const void *const *args)
{
  const bstr_t *arg;
  bstr_t       bstr;
  size_t       len;
  int          i;

  /* Get the bstr address */
  arg = *((const bstr_t **) (args[0]));
#if HAVE_REGISTER_PRINTF_SPECIFIER
  bstr = *arg;
#else
  bstr = arg;
#endif
  if (bstr == NULL)
  {
    fprintf(stream, "(null)");
    return 6;
  }
  /* bstr always begins sizeof(size_t) further from the beginning */
  /* bstr len is stored at the beginning of the allocated zone; so, calling
   * bstr_len is useless, we just get this value from the starting address.
   */
  len = *((size_t *)bstr - 1);

  /* Finally, print all chars to the stream */
  for (i=0; i<len; i++)
    fputc(((char *)bstr)[i], stream);

  /* Return the number of bytes printed.
   * Usually, this is the value returned by sprintf.
   */
  return len;
}

/* Populate argtype and argsize. */
#ifdef HAVE_REGISTER_PRINTF_SPECIFIER
static int _bstr_printArginfo (const struct printf_info *info,
                                size_t n,
                                int *argtypes,
                                int *size)
#else
static int _bstr_printArginfo (const struct printf_info *info,
                                size_t n,
                                int *argtypes)
#endif
{
#ifdef HAVE_REGISTER_PRINTF_SPECIFIER
  argtypes[0] = PA_BSTR;
  size[0] = sizeof(bstr_t);
#else
  argtypes[0] = PA_POINTER;
#endif
  return 1;
}

/*
 * Must be called before using bstr_t type.
 * This function contains all initializations and registration of
 * the new format specifier '%B'.
 */
void __attribute__((constructor)) _bstr_init()
{
#ifdef HAVE_REGISTER_PRINTF_SPECIFIER
  PA_BSTR = register_printf_type(_bstr_va);
  register_printf_specifier('B', _bstr_print, _bstr_printArginfo);
#else
  register_printf_function('B', _bstr_print, _bstr_printArginfo);
#endif
  bstr_initDone = 1;
}

static bstr_t _bstr_malloc(size_t size)
{
  size_t *ret;

  /* Allocating the size required plus the memory zone required
   * to stock the length of the string plus a <code>\0</code>.
   *
   * This is a rough representation of a pascal string where
   * the length of the string precede the string. No NULL char
   * is required to end the string. The \0 at the end guarantee
   * a safe use of bstr as a char*.
   */
  ret = (size_t*)malloc(sizeof(size_t) + size * sizeof(char) + sizeof(char));
  memset(ret, 0, sizeof(size_t) + size * sizeof(char) + sizeof(char));
  if (ret == NULL) return NULL;
  /* Stock the length of the allocated string at the begening of
   * the allocated zone
   */
  *ret = size;
  /* Return the real position of the string */
  return (bstr_t)(ret + 1);
}

static bstr_t _bstr_resize(bstr_t bstr, size_t size)
{
  size_t *ret;
  /* If bstr is NULL, this is equivalent to _bstr_malloc */
  if (bstr == NULL) return _bstr_malloc(size);

  ret = ((size_t *)bstr) - 1;
  /* The rest is the same as _bstr_new */
  ret = (size_t*)realloc(ret, sizeof(size_t) + size * sizeof(char) + sizeof(char));
  if (ret == NULL) return NULL;

  *ret = size;
  return (bstr_t)(ret+1);
}

/**
 * bstr_new:
 * @from: a string from which to create the bstring or NULL.
 * @size: number of characters to use in creating the bstring or -1.
 *
 * Create a new bstring of length @size from the character array @from.
 *
 * If @size is -1, the length of the resulting bstring is calculated using <code>strlen(@from)</code>
 * and the resulting bstring will be a copy of @from.<sbr/>
 * If @size is  0, the length of the resulting bstring will be 0 and @from will be ignored.<sbr/>
 * If @from is NULL, bstr_new() will create a bstring of length @size with zeroed elements.
 *<note><para>
 * If @size exceeds the length of the character array @from, the result will
 * be unpredictable.
 *</para></note>
 *
 * Returns: a newly allocated bstring or NULL on error (usually a result of
 * memory allocation error).
 * The returned value should be freed with bstr_free() when no longer needed.
 **/
bstr_t bstr_new(char *from, size_t size)
{
  bstr_t ret;
  size_t len;

  len = (size == -1) ? strlen(from)
                     : size;
  ret = _bstr_malloc(len);
  if (ret == NULL) return NULL;
  /* If size is 0, we return a 0 length string.
   * If from is NULL, we return a cleared size length string.
   */
  if (size == 0 || from == NULL) return ret;
  /*
   * If size is -1, we use strlen to get the string length and the function
   * will be equivalent to bstr_newFromCStr
   */
  memcpy(ret, from, sizeof(char) * len);
  return ret;
}

/**
 * bstr_newFromCStr:
 * @cstr: a C-style string or NULL.
 *
 * Construct a new bstring from a C string. It is equivalent to
 * <code>bstr_new(@cstr, -1)</code> (See also bstr_new()).
 *
 * If @cstr is NULL, the length of the resulting bstring will be 0.
 *
 * Returns: a newly allocated bstring copy of @cstr or NULL on error (usually a
 * result of memory allocation error). The returned value should be freed with
 * bstr_free() when no longer needed.
 */
bstr_t bstr_newFromCStr(char *cstr)
{
  bstr_t ret;

  /* If cstr is NULL we return an empty string */
  if (cstr == NULL) return _bstr_malloc(0);
  /* Allocate the string, copy it and return the newly created bstr*/
  ret = _bstr_malloc(strlen(cstr));
  memcpy(ret, cstr, strlen(cstr)*sizeof(char));
  return ret;
}

/**
 * bstr_free:
 * @bstr: the bstring to free.
 *
 * Free the memory allocated to @bstr.
 * <note><para>It is advised to free memory allocated to bstrings that are no
 * longer used with bstr_free().</para></note>
 **/
void bstr_free(bstr_t bstr)
{
  if (bstr == NULL) return;
  /* Roll back to the begenning of the allocated zone and free it*/
  free(((size_t *)bstr) - 1);

}

/**
 * bstr_toCStr:
 * @bstr: a bstring to convert.
 *
 * Convert a bstring to a C-style string.
 *
 * The resulting string will be truncated at the first <code>\0</code>. So if
 * @bstr doesn't contain any <code>\0</code> (only the <code>\0</code> at the end),
 * the resulting string should be a copy of the original.
 *
 * Note that @bstr can be used as <code>char *</code> with an explicit cast like:
 * <informalexample><programlisting>
 * str = (char *)bstr;
 * </programlisting></informalexample>
 * but there is some rules on using @str in this case:<sbr/>
 * - @str should never be freed with free().<sbr/>
 * - @str should never be manipulated with standard string functions, get a C-style
 * copy of @bstr with bstr_toCStr() instead.
 *
 * The cast to <code>char *</code> can be used for simple operation such as
 * printing :
 * <informalexample><programlisting>
 * printf("%s\n",(char *)bstr);
 * </programlisting></informalexample>
 * and even this is not needed since the <code>\%B</code> modifier can be used
 * to print a bstring.
 *
 * Returns: a C-style string. The returned string should be freed withe
 * <function>free()</function> when no longer needed.
 **/
char* bstr_toCStr(bstr_t bstr)
{
  /* Simply duplicate the string part of bstr since bstr is a valid C string.
   * But notice that this will truncate the string on the first \0 if bstr
   * is a binary string.
   */
  return strdup((char*)bstr);
}

/**
 * bstr_len:
 * @bstr: a bstring.
 *
 * Retrieve the length of a bstring.
 *
 * Returns: the length of @bstr.
 **/
size_t bstr_len(bstr_t bstr)
{
  /* Simply return the lenght part of bstr*/
  return *(((size_t*)bstr) -1);
}

/**
 * bstr_catBStr:
 * @to: the bstring to append to or NULL.
 * @from: the bstring to append or NULL.
 *
 * Concatenate 2 bstrings. The result is stored in @to.
 * 
 * If @from is NULL, @to remain unchanged and returned as it is.<sbr/>
 * If @to is NULL, this is equivalent to <code>bstr_dup(@from)</code>.
 * <note><para>
 * If @to or @from are not initialized, the result is unpredictable.
 * </para></note>
 *
 * Returns: the concatenated bstring (@to) or NULL on error (usually a result of
 * an error while expanding the memory allocated to @to). The returned value
 * should be freed with bstr_free() when no longer needed.
 **/
bstr_t bstr_catBStr(bstr_t to, bstr_t from)
{
  bstr_t ret;
  size_t tolen,
         fromlen;
  if (from == NULL) return to;
  if (to == NULL)   return bstr_dup(from);
  tolen   = bstr_len(to);
  fromlen = bstr_len(from);
  /* Resize the target string to the appropriate size */
  ret = _bstr_resize(to, tolen + fromlen);
  if (ret == NULL) return NULL;
  memcpy((char *)ret + tolen, from, fromlen * sizeof(char));
  ((char *)ret)[tolen + fromlen] = '\0';

  return ret;
}

/**
 * bstr_cat:
 * @bstr: the bstring to append to or NULL.
 * @cstr: the C-style string to append or NULL.
 * @size: the number of characters to append or -1.
 *
 * Concatenate @size characters from @cstr to @bstr.
 *
 * If @size is -1, <function>strlen()</function> is used to calculate the number
 * of characters to concatenate.<sbr/>
 * If @cstr is NULL, @bstr is return as it is.<sbr/>
 * If @bstr is NULL, this is the same as calling bstr_new() with @cstr and @size.
 *
 * Returns: the concatenated bstring (@bstr) or NULL on error. (usually a result
 * of a memory allocation error). The returned value should be freed with
 * bstr_free() when no longer needed.
 **/
bstr_t bstr_cat(bstr_t bstr, char *cstr, size_t size)
{
  /* Similar to bstr_cat */
  bstr_t ret;
  size_t tolen,
         fromlen;

  if (cstr == NULL) return bstr;
  if (bstr == NULL) return bstr_new(cstr, size);
  tolen   = bstr_len(bstr);
  fromlen = (size == -1) ? strlen(cstr)
                         : size;
  /* Resize the target string to the appropriate size */
  ret = _bstr_resize(bstr, tolen + fromlen);
  if (ret == NULL) return NULL;
  memcpy((char *)ret + tolen, cstr, fromlen * sizeof(char));
  ((char *)ret)[tolen + fromlen] = '\0';

  return ret;
}

/**
 * bstr_catCStr:
 * @bstr: the bstring to append to.
 * @cstr: the C-style string to append.
 *
 * Concatenate @cstr to @bstr. This has the same effect as
 * <code>bstr_cat(@bstr, @cstr, -1)</code> (See bstr_cat()).
 *
 * Returns: the concatenated bstring (@bstr) or NULL on error (memory allocation
 * error). The returned value should be freed with bstr_free() when no longer
 * needed.
 **/
bstr_t bstr_catCStr(bstr_t bstr, char *cstr)
{
  return bstr_cat(bstr, cstr, -1);
}

/**
 * bstr_dup:
 * @bstr: the bstring to duplicate
 * 
 * Duplicates a bstring and return the newly created one.
 *
 * Returns: a bstring copy of @bstr or NULL on error (memory allocation error).
 * The returned value should be freed with bstr_free() when no longer needed.
 **/
bstr_t bstr_dup(bstr_t bstr)
{
  /* Create a new string and copy the target string to it */
  size_t fromlen;
  bstr_t ret;

  fromlen = bstr_len(bstr);
  ret = _bstr_malloc(fromlen);
  if (ret == NULL) return NULL;

  memcpy(ret, bstr, fromlen);
  return ret;
}

/*
 * Used by printf variants witch use variadic attributes.
 * Storage is allocated automatically, so no need to preallocate it.
 */
static int _bstr_vasprintf(bstr_t *bstr, char *fmt, va_list ap)
{
  char    *buf;
  int     len;

  len = vasprintf(&buf, fmt, ap);
  if (len < 0)
  {
    free(buf);
    return -1;
  }

  *bstr = bstr_new(buf, len);
  free(buf);
  return len;
}

/*
 * Like asprintf, but prints in a bstr_t var. No nedd to preallocate storage,
 * but bstr must be freed manually.
 * Return total printed chars or -1 on error.
 */
/**
 * bstr_asprintf:
 * @bstr: the bstring to print to.
 * @fmt: the printf-style format string.
 * @...: the list of args conforming to @fmt format.
 *
 * Like asprintf, but prints in a bstr_t var. No need to preallocate storage,
 * but @bstr must be freed manually.
 * 
 * <note><para>
 * There is also the new printf modifier <code>\%B</code> that can be used
 * to print a bstring. This modifier is usable in all printf-like functions.
 * </para></note>
 *
 * Returns: the number of character writen or -1 on error. @bstr should be freed
 * with bstr_free() when no longer needed.
 **/
int bstr_asprintf(bstr_t *bstr, char *fmt, ...)
{
  va_list ap;
  int     len;
  va_start(ap, fmt);
  len = _bstr_vasprintf(bstr, fmt, ap);
  va_end(ap);  /* Not required for GNU C */
  return len;
}

/**
 * bstr_scatprintf:
 * @bstr: the bstring to append to.
 * @fmt: the printf-style format string.
 * @...: the list of args conforming to @fmt format.
 *
 * Like bstr_asprintf(), but append to the input bstr_t var. @bstr is resized
 * to fit to the concatenated string.
 *
 * <note><para>
 * There is also the new printf modifier <code>\%B</code> that can be used
 * to print a bstring. This modifier is usable in all printf-like functions.
 * </para></note>
 *
 * Returns: the number of appended characters or -1 on error. @bstr should be
 * freed with bstr_free() when no longer needed.
 **/
int     bstr_scatprintf(bstr_t *bstr, char *fmt, ...)
{
  va_list ap;
  bstr_t buf;
  int    len;

  va_start(ap, fmt);
  len = _bstr_vasprintf(&buf, fmt, ap);
  va_end(ap);
  if (len < 0)
  {
    bstr_free(buf);
    return -1;
  }

  *bstr = bstr_catBStr(*bstr, buf);
  bstr_free(buf);
  return len;

}
