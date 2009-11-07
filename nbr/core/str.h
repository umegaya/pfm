/***************************************************************
 * str.h : basic string operation
 * 2008/07/23 iyatomi : create
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
 ****************************************************************/
#if !defined(__STR_H__)
#define __STR_H__

NBR_INLINE int 
nbr_str_cmp(const char *a, size_t al, const char *b, size_t bl)
{
	const char *wa = a, *wb = b;
	while (*wa && *wb) {
		if (*wa > *wb) {
			return 1;
		}
		else if (*wa < *wb) {
			return -1;
		}
		wa++; wb++;
		if ((wa - a) > al) {
			if ((wb - b) > bl) {
				return 0;
			}
			return -1;
		}
		else {
			if ((wb - b) > bl) {
				return 1;
			}
		}
	}
	if (*wa == *wb) {
		return 0;
	}
	else if (!(*wb)) {
		return 1;
	}
	else if (!(*wa)) {
		return -1;
	}
	return 0;
}

NBR_INLINE int 
nbr_str_copy(char *a, size_t al, const char *b, size_t bl)
{
	char *wa = a;
	const char *wb = b;
	while (*wb) {
		*wa++ = *wb++;
		if ((wa - a) >= al) {
			a[al - 1] = '\0';
			return al;
		}
		if ((wb - b) >= bl) {
			*wa = '\0';
			return (wa - a);
		}
	}
	*wa = '\0';
	return (wa - a);
}

NBR_INLINE int
nbr_str_length(const char *str, size_t max)
{
	const char *w = str;
	while(*w) {
		if ((w - str) > max) {
			ASSERT(FALSE);
			return max;
		}
	}
	return (w - str);
}

#define nbr_str_printf(__p, __l, __fmt, ...)	\
	snprintf(__p, __l, __fmt, __VA_ARGS__);


#endif//__STR_H__
