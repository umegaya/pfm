/****************************************************************
 * nbr_pkt.h : macros for build binary packet protocol
 * 2008/07/14 iyatomi : create
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
#if !defined(__PKT_H__)
#define __PKT_H__

#include <memory.h>		/* for memcpy */
#include <arpa/inet.h>	/* for ntohl, htonl */

/* support routine & macros */
#if __BYTE_ORDER == __BIG_ENDIAN
static inline U64 ntohll(U64 n) { return n; }
static inline U64 htonll(U64 n) { return n; }
#else
static inline U64 ntohll(U64 n) { return (((U64)ntohl(n)) << 32) + ntohl(n >> 32); }
static inline U64 htonll(U64 n) { return (((U64)htonl(n)) << 32) + htonl(n >> 32); }
#endif

#if defined (_DEBUG)
#include	<stdio.h>
#define	__RETURN				fprintf(stderr, "PACKETMACRO ERROR : %s(%d)\n", __FILE__, __LINE__); return
#else	/* defined (_PACKETMACRO_DEBUG) */
#define	__RETURN				return
#endif	/* defined (_PACKETMACRO_DEBUG) */


/* memory access support */
#define GET_8(ptr)		(*((U8 *)(ptr)))
#define GET_16(ptr)		(*((U16 *)(ptr)))
#define GET_32(ptr)		(*((U32 *)(ptr)))
#define GET_64(ptr)		(*((U64 *)(ptr)))
#define SET_8(ptr,v)	(*((U8 *)(ptr)) = v)
#define SET_16(ptr,v)	(*((U16 *)(ptr)) = v)
#define SET_32(ptr,v)	(*((U32 *)(ptr)) = v)
#define SET_64(ptr,v)	(*((U64 *)(ptr)) = v)


/* unpack support */
#define	POP_START(data, len)	const char *_data=(data); int _len=(len); int _ofs=0;
#define POP_LEN()				_ofs
#define	POP_BUF()				_data
#define POP_REMAIN()			(_len - _ofs)
#define POP_SKIP(n)				(_ofs += n)
#define POP_HAS_REMAIN()		(_len > _ofs)
#define	POP_LENGTHCHECK(x)		if ((_ofs + (int)sizeof(x)) > _len) {__RETURN -1;}
#define	POP_8(i)				{POP_LENGTHCHECK(i); i = *_data;_data+=sizeof(i); _ofs+=sizeof(i);}
#define	POP_16(i)				{U16 __ts;  POP_LENGTHCHECK(__ts); U32 _i = 0; do{((char *)(&__ts))[_i] = _data[_i];}while(++_i < 2); i = ntohs(__ts); _data+=sizeof(__ts); _ofs+=sizeof(__ts);}
#define	POP_32(i)				{U32 __tl;  POP_LENGTHCHECK(__tl); U32 _i = 0; do{((char *)(&__tl))[_i] = _data[_i];}while(++_i < 4); i = ntohl(__tl); _data+=sizeof(__tl); _ofs+=sizeof(__tl);}
#define	POP_64(i)				{U64 __tll; POP_LENGTHCHECK(__tll);U32 _i = 0; do{((char *)(&__tll))[_i] = _data[_i];}while(++_i < 8); i = ntohll(__tll); _data+=sizeof(__tll); _ofs+=sizeof(__tll);}
#define POP_8A(ia, ialen)		{U16 __alen; POP_16(__alen); if (__alen > 0) { if ((S32)__alen>(S32)(ialen)) {__RETURN -3;} if ((_ofs + __alen) > _len) {__RETURN -1;} memcpy((ia),_data,__alen); _data+=__alen; _ofs+=__alen; ialen=__alen;}}
#define	POP_16A(ia, ialen)		{U32 __ii;U32 __max_len=ialen; POP_16(ialen); if (ialen>__max_len) {__RETURN -3;} for (__ii = 0 ; __ii < ialen ; __ii++) { POP_U16(ia[__ii]); }}
#define	POP_32A(ia, ialen)		{U32 __ii;U32 __max_len=ialen; POP_16(ialen); if (ialen>__max_len) {__RETURN -3;} for (__ii = 0 ; __ii < ialen ; __ii++) { POP_U32(ia[__ii]); }}
#define	POP_64A(ia, ialen)		{U32 __ii;U32 __max_len=ialen; POP_16(ialen); if (ialen>__max_len) {__RETURN -3;} for (__ii = 0 ; __ii < ialen ; __ii++) { POP_U64(ia[__ii]); }}
#define	POP_STR(buf, buf_len)	{char *__strt=(char *)buf; int __i=0; while (*_data!='\0') {*__strt=*_data; _data++; __strt++; _ofs++; __i++; POP_LENGTHCHECK(char); if (buf_len<=__i+1) {break;}} *__strt='\0';_ofs++;_data++;}
#define	POP_STR2(buf, buf_len,__i)	{char *__strt=(char *)buf; __i=0; while (*_data!='\0') {*__strt=*_data; _data++; __strt++; _ofs++; __i++; POP_LENGTHCHECK(char); if (buf_len<=__i+1) {break;}} *__strt='\0';_ofs++;_data++;}
#define POP_MEM(buf, buf_len)   {if ((_ofs + (int)buf_len) > _len) { __RETURN -1; } memcpy(buf, _data, buf_len); _data += buf_len; _ofs += buf_len; }

/* pack support */
#define	PUSH_START(work, max)	char *_work=(work); int _outmax=(max); int _ofs=0;
#define	PUSH_LEN()				_ofs
#define	PUSH_BUF()				_work
#define PUSH_REMAIN()			(_outmax - _ofs)
#define PUSH_SKIP(n)			(_ofs += n)
#define	PUSH_LENGTHCHECK(x)		if ((_ofs + (int)sizeof(x)) > _outmax) {__RETURN -1;}
#define PUSH_8(i)				{PUSH_LENGTHCHECK(U8); *_work = (U8)i; _work+=sizeof(U8); _ofs+=sizeof(U8);}
#define	PUSH_16(i)				{U16 __ts;	PUSH_LENGTHCHECK(__ts); __ts = htons((U16)(i));   U32 _i = 0; do{_work[_i] = ((char *)(&__ts))[_i];}while(++_i < 2); _work+=sizeof(__ts); _ofs+=sizeof(__ts);}
#define PUSH_32(i)				{U32 __tl;  PUSH_LENGTHCHECK(__tl); __tl = htonl((U32)(i)); U32 _i = 0; do{_work[_i] = ((char *)(&__tl))[_i];}while(++_i < 4); _work+=sizeof(__tl); _ofs+=sizeof(__tl);}
#define	PUSH_64(i)				{U64 __tll; PUSH_LENGTHCHECK(__tll); __tll = htonll((U64)(i));U32 _i = 0; do{_work[_i] = ((char *)(&__tll))[_i];}while(++_i < 8); _work+=sizeof(__tll); _ofs+=sizeof(__tll);}
#define	PUSH_8A(a, alen)		{PUSH_16((U16)alen); if (alen > 0) { if ((_ofs + (alen)) > _outmax) {__RETURN -1;} memcpy(_work, (a), (alen)); _work+=(long)(alen); _ofs+=(long)(alen); }}
#define	PUSH_16A(a, alen)		{U32 __ii;  PUSH_16((S16)alen); for (__ii=0 ; __ii<alen ; __ii++) { PUSH_16(a[__ii]); }}
#define	PUSH_32A(a, alen)		{U32 __ii;  PUSH_16((S16)alen); for (__ii=0 ; __ii<alen ; __ii++) { PUSH_32(a[__ii]); }}
#define	PUSH_64A(a, alen)		{U32 __ii;  PUSH_16((S16)alen); for (__ii=0 ; __ii<alen ; __ii++) { PUSH_16(a[__ii]); }}
#define	PUSH_STR(str)			{S32 __n=(S32)strlen(str); if ((_ofs+__n+1)>_outmax) {__RETURN -1;} memcpy(_work, str, __n); _work+=__n; *_work='\0'; _work++; _ofs+=(__n+1);}
#define PUSH_MEM(buf, buf_len)  {if ((_ofs + (int)buf_len) > _outmax) { __RETURN -1; } memcpy(_work, buf, buf_len); _work += buf_len; _ofs += buf_len; }


#endif/* __PKT_H__ */
