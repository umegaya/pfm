/****************************************************************
 * str.c
 * 2008/09/21 iyatomi : create
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
#include "str.h"
#include <ctype.h>


/*-------------------------------------------------------------*/
/* macro													   */
/*-------------------------------------------------------------*/
#define STR_ERROUT		NBR_ERROUT



/*-------------------------------------------------------------*/
/* external methods											   */
/*-------------------------------------------------------------*/
NBR_API int
nbr_str_atobn(const char* str, S64 *i, int max)
{
	const char *_s = str;
	int minus = 0;
	*i = 0LL;
	if ('-' == *_s) {
		minus = 1;
		_s++;
	}
	while(*_s) {
		if ('0' <= *_s && *_s <= '9') {
			(*i) = (*i) * 10LL + (unsigned long long)((*_s) - '0');
		}
		else {
			STR_ERROUT(ERROR,FORMAT,"invalid fmt(%s)\n",str);
			return LASTERR;
		}
		_s++;
		if (_s - str >= max) {
			STR_ERROUT(ERROR,LENGTH,
				"invalid length (more than %d)\n", max);
			return LASTERR;
		}
	}
	if (minus) {
		(*i) = -1LL * (*i);
	}

	return NBR_OK;
}

NBR_API int
nbr_str_atoi(const char* str, int *i, int max)
{
	const char *_s = str;
	int minus = 0;
	*i = 0;
	if ('-' == *_s) {
		minus = 1;
		_s++;
	}
	while(*_s) {
		if ('0' <= *_s && *_s <= '9') {
			(*i) = (*i) * 10 + (int)((*_s) - '0');
		}
		else {
			STR_ERROUT(ERROR,FORMAT,"invalid fmt(%s)\n", str );
			ASSERT(FALSE);
			return LASTERR;
		}
		_s++;
		if (_s - str >= max) {
			STR_ERROUT(ERROR,LENGTH,
				"invalid length (more than %d)\n", max);
			return LASTERR;
		}
	}

	if (minus) {
		(*i) = -1 * (*i);
	}

	return NBR_OK;
}

NBR_API int
nbr_str_htoi(const char* str, int *i, int max)
{
	const char *_s = str;
	int minus = 0;
	*i = 0;
	if ('-' == *_s) {
		minus = 1;
		_s++;
	}
	while(*_s) {
		int num = -1;
		if ('0' <= *_s && *_s <= '9') {
			num = (int)((*_s) - '0');
		}
		if ('a' <= *_s && *_s <= 'f') {
			num = (int)(((*_s) - 'a') + 10);
		}
		if ('A' <= *_s && *_s <= 'F') {
			num = (int)(((*_s) - 'A') + 10);
		}
		if (num < 0) {
			STR_ERROUT(ERROR,FORMAT,"invalid fmt(%s)\n", str );
			return LASTERR;
		}
		(*i) = (*i) * 16 + num;
		_s++;
		if (_s - str >= max) {
			STR_ERROUT(ERROR,LENGTH,
				"invalid length (more than %d)\n", max);
			return LASTERR;
		}
	}

	if (minus) {
		(*i) = -1 * (*i);
	}

	return NBR_OK;
}

NBR_API int
nbr_str_htobn(const char* str, S64 *i, int max)
{
	const char *_s = str;
	int minus = 0;
	*i = 0;
	if ('-' == *_s) {
		minus = 1;
		_s++;
	}
	while(*_s) {
		long long num = -1;
		if ('0' <= *_s && *_s <= '9') {
			num = (S64)((*_s) - '0');
		}
		if ('a' <= *_s && *_s <= 'f') {
			num = (S64)(((*_s) - 'a') + 10);
		}
		if ('A' <= *_s && *_s <= 'F') {
			num = (S64)(((*_s) - 'A') + 10);
		}
		if (num < 0) {
			STR_ERROUT(ERROR,FORMAT,"invalid fmt(%s)\n", str );
			return LASTERR;
		}
		(*i) = (*i) * 16 + num;
		_s++;
		if (_s - str >= max) {
			STR_ERROUT(ERROR,LENGTH,
				"invalid length (more than %d)\n", max);
			return LASTERR;
		}
	}

	if (minus) {
		(*i) = -1 * (*i);
	}
	return NBR_OK;
}

NBR_INLINE char *
get_oneword_utf8(char *ibuf, int *ilen, char *obuf, int *olen)
{
	/* ported from UnicodeConvert.cpp */
	if (ibuf == NULL || obuf == NULL || *ilen <= 0 || *olen <= 0) {
		return 0;
	}

	U8	ch = (U8)ibuf[0];
	if (ch <= 0x7F) {
		*obuf++ = *ibuf++;
		*olen = 1;
		*ilen -= 1;
		return ibuf;
	}
	else if ((ch & 0xE0) == 0xE0) {
		// ３バイトUTF-8文字の１バイト目
		if (*ilen >= 3 && *olen >= 3) {
			*obuf++	= *ibuf++;
			*obuf++	= *ibuf++;
			*obuf++	= *ibuf++;
			*olen = 3;
			*ilen -= 3;
			return ibuf;
		}
	}
	else if ((ch & 0xC0) == 0xC0) {
		// ２バイトUTF-8文字の１バイト目
		if (*ilen >= 2 && *olen >= 2) {
			*obuf++	= *ibuf++;
			*obuf++	= *ibuf++;
			*olen = 2;
			*ilen -= 2;
			return ibuf;
		}
	}
	else {
		ASSERT(FALSE);
	}

	*olen = 0;
	return NULL;
}

NBR_API size_t
nbr_str_utf8_copy(char *dst, int dlen, const char *src, int smax, int len)
{
	char *s_now = (char *)src, *d_now = dst;
	int dwork, swork;
	while(*s_now) {
		dwork = dlen, swork = smax;
		s_now = get_oneword_utf8(s_now, &swork, d_now, &dwork);
		d_now += dwork;
		dlen -= dwork;
		smax = swork;
		len--;
		if (len <= 0 || dlen <= 1 || smax <= 0) {
			break;
		}
	}
	*d_now = '\0';
	return (src - s_now);
}

NBR_API const char *
nbr_str_divide_tag_and_val(char sep, const char *line, char *tag, int taglen)
{
	const char *w = line;
	while(*w) {
		if (*w == sep) {
			*tag = '\0';
			return w + 1;
		}
		*tag++ = *w++;
		if ((int)(w - line) > taglen) {
			ASSERT(FALSE);
			return NULL;
		}
	}

	*tag = '\0';
	ASSERT(FALSE);
	return NULL;
}

NBR_API const char *
nbr_str_divide(const char *sep, const char *line, char *tag, int *tlen)
{
	const char *w = line, *tmp = tag;
	const char *s;
	int maxlen = *tlen;
	while(*w) {
		s = sep;
		while (*s) {
			if (*w == *s) {
				*tag = '\0';
				return w + 1;
			}
			s++;
		}
		*tag++ = *w++;
		if ((int)(w - line) > maxlen) {
			ASSERT(FALSE);
			return NULL;
		}
	}

	*tag = '\0';
	*tlen = (w - line);
	return tmp;
}


NBR_API int
nbr_str_cmp_nocase(const char *a, const char *b, int len)
{
	const char *wa = a, *wb = b;
	while(1) {
		if (*wa == 0) {
			if (*wb == 0) {
				return 0;
			}
			else {
				return -1;
			}
		}
		else {
			if (tolower(*wa) != tolower(*wb)) {
				if (tolower(*wa) > tolower(*wb)) {
					return 1;
				}
				else {
					return -1;
				}
			}
		}
		wa++; wb++;
		if (wa - a > len) {
			return 0;
		}
	}
}

NBR_API int
nbr_str_cmp_tail(const char *a, const char *b, int len, int max)
{
	const char *wa = a, *wb = b;
	while (*wa) {
		wa++;
		if ((wa - a) > max) {
			return 1;
		}
	}
	while (*wb) {
		wb++;
		if ((wb - b) > max) {
			return -1;
		}
	}
	if ((wb - b) < len || (wa - a) < len) {
		return nbr_str_cmp(a, (wa - a), b, (wb - b));
	}
	wb -= len;
	wa -= len;
	int cnt = 0;
	while (cnt < len) {
		if (wb[cnt] != wa[cnt]) {
			if (wb[cnt] > wa[cnt]) {
				return -1;
			}
			else {
				return 1;
			}
		}
		cnt++;
	}
	return 0;
}

NBR_API int
nbr_str_parse_url(const char *in, int max, char *host, U16 *port, char *url)
{
	const char *w = in;
	int i = 0;
	char tok[65536];
	while(*w) {
		switch(i) {
			case 0:	/* host */
				{
					char *t = host;
					*port = 80;
					while(*w) {
						*t++ = *w++;
						if (*w == ':') { i = 1; *t = '\0'; w++; break; }
						if (*w == '/') { i = 2; *t = '\0'; break; }
					}
					/* reach to null terminator */
					if (*w == '\0') { return NBR_OK; }
				}
				break;
			case 1:	/* port */
				{
					char *t = tok;
					while(*w != '/') {
						*t++ = *w++;
						/* reach to null terminator */
						if (*w == '\0') { break; }
					}
					*t = '\0';
					int _port;
					if (nbr_str_atoi(tok, &_port, 256) < 0) {
						STR_ERROUT(ERROR,FORMAT,"invalid fmt(%s)\n", in );
						return LASTERR;
					}
					*port = (U16)_port;
					i++;
					if (*w == '\0') { return NBR_OK; }
				}
				break;
			case 2:	/* URL */
				{
					char *t = url;
					while(*w) { *t++ = *w++; }
					*t = '\0';
					if (*(t - 1) == '\n') { *(t - 1) = '\0'; }
				}
				break;
		}
		if ((w - in) > max) {
			STR_ERROUT(ERROR,FORMAT,"invalid fmt(%s)\n", in );
			return LASTERR;
		}
	}
	return NBR_OK;
}

NBR_API char *
nbr_str_chop(char *buffer)
{
	char *w = buffer;
	while(*w) {
		w++;
		if ((w - buffer) > (int)(0x7FFFFFFF)) {
			return NULL;
		}
	}
	if ((w - buffer) > 0 && *(w - 1) == '\n') {
		*(w - 1) = '\0';
		return buffer;
	}
	else if ((w - buffer) > 1 && *(w - 1) == '\n' && *(w - 2) == '\r') {
		*(w - 1) = *(w - 2) = '\0';
		return buffer;
	}
	return buffer;
}

NBR_API const char*
nbr_str_rchr(const char *in, char sep, int max)
{
	const char *w = in, *p = NULL;
	while(*w) {
		if (*w == sep) {
			p = w;
		}
		if ((w - in) > max) {
			return NULL;
		}
		w++;
	}
	return p;
}

static const char SEP_EQUAL = '=';
static const char SEP_AMP = '&';

NBR_API int
nbr_parse_http_req_str(const char *req, const char *tag, char *buf, int buflen)
{
	const char *w = req, *prev = w, *mark = NULL;
	while (*w) {
		if (*w == SEP_EQUAL && mark == NULL) {
			mark = w;
		}
		if (*w == SEP_AMP) {
			if (mark == NULL) {
				continue;
			}
			if (nbr_mem_cmp(prev, tag, (mark - prev)) == 0) {
				if (1 < (w - mark) && (w - mark) <= buflen) {
					nbr_mem_copy(buf, mark + 1, (w - mark - 1));
					buf[(w - mark - 1)] = '\0';
					return (w - mark - 1);
				}
				else {
					return NBR_ENOTFOUND;
				}
			}
			prev = (w + 1);
			mark = NULL;
		}
		w++;
		if ((w - req) > 65536) {
			return NBR_ENOTFOUND;
		}
	}
	if (mark != NULL && mark != req) {	/* <tag=value> pattern */
		if (nbr_mem_cmp(prev, tag, (mark - prev)) == 0) {
			if (1 < (w - mark) && (w - mark) <= buflen) {
				nbr_mem_copy(buf, mark + 1, (w - mark - 1));
				buf[(w - mark - 1)] = '\0';
				return (w - mark - 1);
			}
			else {
				return NBR_ENOTFOUND;
			}
		}
	}
	return NBR_ENOTFOUND;
}

NBR_API int
nbr_parse_http_req_int(const char *req, const char *tag, int *buf)
{
	int r;
	char buffer[65536];
	if ((r = nbr_parse_http_req_str(req, tag, buffer, sizeof(buffer))) < 0) {
		return r;
	}
	if ((r = nbr_str_atoi(buffer, buf, sizeof(buffer))) < 0) {
		return r;
	}
	return NBR_OK;
}

NBR_API int
nbr_parse_http_req_bigint(const char *req, const char *tag, long long *buf)
{
	int r;
	char buffer[65536];
	if ((r = nbr_parse_http_req_str(req, tag, buffer, sizeof(buffer))) < 0) {
		return r;
	}
	if ((r = nbr_str_atobn(buffer, buf, sizeof(buffer))) < 0) {
		return r;
	}
	return NBR_OK;
}
