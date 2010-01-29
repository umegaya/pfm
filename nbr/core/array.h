/****************************************************************
 * array.h : common routine for handling array
 * 2008/06/25 iyatomi : create
 *                             Copyright (C) 2008-2009 Takehiro Iyatomi
 * This file is part of libnbr.
 * libnbr is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either
 * version 2.1 of the License or any later version.
 * libnbr is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU Lesser General Public License for more details.
 * You should have received a copy of
 * the GNU Lesser General Public License along with libnbr;
 * if not, write to the Free Software Foundation, Inc.,
 * 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA.
 *****************************************************************/
#if !defined(__ARRAY_H__)
#define __ARRAY_H__

int	nbr_array_init(int max);
void nbr_array_fin();

#if defined(_TEST)
extern BOOL nbr_array_test();
#endif
#if defined(_DEBUG)
#ifdef __cplusplus    /* When the user is Using C++,use C-linkage by this */
extern "C" {
#endif
extern BOOL nbr_array_sanity_check(ARRAY a);
#ifdef __cplusplus    /* When the user is Using C++,use C-linkage by this */
}
#endif
#endif

#endif	/* __ARRAY_H__ */
