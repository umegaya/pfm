/****************************************************************
 * tutil.c : test utility routines
 * 2009/10/24 iyatomi : create
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
#include "common.h"
#include "tutil.h"
#include <ctype.h>

int _prime(int given)
{
	int i, j;
	unsigned char *p;

	if (given <= 3) {
		return given;
	}

	/* this may cause problem if given is too huge */
	p = (unsigned char *)malloc(given);
	if (!p) {
		return -1;
	}
	bzero(p, given);
	for (i = 2; i <= given; i++) {
		if (i >= (int)(given/i)) {
//			TRACE("_prime:break at %u,(%u)", i, (int)(given/i));
			break;
		}
		if (p[i]) {
			continue;
		}
		else {
			for(j = i; j <= given; j += i) {
				p[j] = 1;
			}
		}
	}

	for (i = (given - 1); i > 2; i--) {
		if (p[i] == 0) {
			free(p);
			return i;
		}
	}

	// no prime number!? you're kidding!!
	ASSERT(FALSE);
	free(p);
	return -1;
}

void
write_random_data(void *p, int size)
{
	int i;
	unsigned char *u = (unsigned char *)p;

	for (i = 0; i < size; i++) {
		u[i] = (unsigned char)nbr_rand32() % 256;
	}
}

void
write_random_string(void *p, int size)
{
#define STRING_ELEMENTS "ABCDEFGHIJKLMNOPQRSTUVWXYZ"	\
		"abcdefghijklmnopqrstuvwxyz"					\
		"0123456789"									\
		" +-=!@#$%^&*()<>/?\";',."
	int i;
	char *u = (char *)p;
	const char *rndstr = STRING_ELEMENTS;

	for (i = 0; i < (size - 1); i++) {
		u[i] = (char)rndstr[nbr_rand32() % (sizeof(STRING_ELEMENTS) - 1)];
	}
	u[size - 1] = '\0';
}

char *
toupper_string(const char *p, char *buf)
{
	const char *w = p;
	while(*w) {
		*buf = toupper(*w);
		w++; buf++;
	}
	*buf = '\0';
	return buf;
}
