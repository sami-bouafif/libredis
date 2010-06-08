/* bstr.h
 *
 * Copyright (C) 2010
 *        Sami Bouafif <sami.bouafif@gmail.com>. All Rights Reserved.
 *
 * This file is part of libredis.
 *
 * libredis library is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * libredis library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with libredis library.  If not, see <http://www.gnu.org/licenses/>.
 *
 * A copy of the GPL can be found in the file "COPYING" in this distribution.
 * A copy of the LGPL can be found in the file "COPYING.LESSER" in this distribution.
 */

#ifndef BSTR_H_
#define BSTR_H_

#include <stdarg.h>
#include <stdlib.h>

/**
 * bstr_t:
 *
 * An opaque structure to represent a bstring. It is a rough representation of
 * pascal string where the length of the string precede the string itself, so
 * <code>\0</code> characters can be embedded in the string without truncating it.
 *
 * The memory occupied by #bstr_t is represented internally as <code>size_t</code>
 * followed by a <code>char *</code> and a <code>\0</code> character, and
 * #bstr_t itself points to the <code>char *</code> portion.
 * This is set manually rather than using a <code>struct</code> to emphasis the
 * fact that the only exploitable part is the string part.
 * The length part is updated automatically by #bstr_t related functions.<sbr/>
 * <note><para>
 * Using <code>struct</code> to define <code>bstr_t</code> is just a way to
 * prevent the compiler from implicit casting <code>bstr_t</code> to
 * <code>char *</code>.
 * </para></note>
 * Since #bstr_t points to the string part of the structure, it can be used
 * anywhere <code>char *</code> is usable (with an explicit cast) but, if the
 * string has more than one <code>\0</code> character, the result will be
 * truncated on the first one.
 *
 * Note also that modifying the string part of #bstr_t directly (modifying
 * #bstr_t as a <code>char *</code>) will invalidate the length part and
 * therefore, the effect of #bstr_t related functions on the modified version
 * will be unpredictable.<sbr/>
 * It is advised to use the cast in read-only operations. For read-write ones,
 * get a copy of the string with bstr_toCStr().
 **/
typedef struct _bstr_t * bstr_t;

bstr_t  bstr_new(char *from, size_t size);
bstr_t  bstr_newFromCStr(char *cstr);
void    bstr_free(bstr_t bstr);
char*   bstr_toCStr(bstr_t bstr);
size_t  bstr_len(bstr_t bstr);
bstr_t  bstr_cat(bstr_t bstr, char *cstr, size_t size);
bstr_t  bstr_catBStr(bstr_t to, bstr_t from);
bstr_t  bstr_catCStr(bstr_t bstr, char *cstr);
bstr_t  bstr_dup(bstr_t bstr);

int     bstr_asprintf(bstr_t *bstr, char *fmt, ...);
int     bstr_scatprintf(bstr_t *bstr, char *fmt, ...);

#endif /* BSTR_H_ */
