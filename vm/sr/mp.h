#if !defined(__MP_H__)
#define __MP_H__

#include <msgpack.hpp>

namespace sfc {
namespace vm {
namespace serialize {
/* TODO: pending (first, I try to use original msgpack)  */
class __mp
{
public:
	static const U8 P_FIXNUM_START= 0x00;
	static const U8 P_FIXNUM_END 	= 0x7f;       
	static const U8 N_FIXNUM_START= 0xe0;
	static const U8 N_FIXNUM_END 	= 0xff;
	static const U8 VARIABLE_START= 0x80;        
	static const U8 NIL 		= 0xc0;
	static const U8 STR 		= 0xc1;       
	static const U8 BOOLEAN_FALSE = 0xc2;
	static const U8 BOOLEAN_TRUE 	= 0xc3;
	static const U8 RESERVED1 	= 0xc4;
	static const U8 RESERVED2 	= 0xc5;
	static const U8 RESERVED3 	= 0xc6;
	static const U8 RESERVED4 	= 0xc7;
	static const U8 RESERVED5 	= 0xc8;
	static const U8 RESERVED6 	= 0xc9;
	static const U8 FLOAT 	= 0xca;
	static const U8 DOUBLE 	= 0xcb;
	static const U8 UINT_8BIT 	= 0xcc;
	static const U8 UINT_16BIT 	= 0xcd;
	static const U8 UINT_32BIT 	= 0xce;
	static const U8 UINT_64BIT 	= 0xcf;
	static const U8 SINT_8BIT 	= 0xd0;
	static const U8 SINT_16BIT 	= 0xd1;
	static const U8 SINT_32BIT 	= 0xd2;
	static const U8 SINT_64BIT 	= 0xd3;
	static const U8 BIG_FLOAT_16 	= 0xd6;
	static const U8 BIG_FLOAT_32 	= 0xd7;
	static const U8 BIGINT_16 	= 0xd8;
	static const U8 BIGINT_32 	= 0xd9;
	static const U8 RAW16 	= 0xda;
	static const U8 RAW32 	= 0xdb;
	static const U8 ARRAY16 	= 0xdc;
	static const U8 ARRAY32 	= 0xdd;
	static const U8 MAP16 	= 0xde;
	static const U8 MAP32 	= 0xdf;
        static const U8 VARIABLE_END 	= 0xdf;
	static const U8 FIXRAW_MASK	= 0xa0;
	static const U8 FIXARRAY_MASK = 0x90;
	static const U8 FIXMAP_MASK	= 0x80;
public:
	typedef struct _pair {
		struct _data *k, *v;
	} pair;
	typedef struct _data {
		typedef union {
			U8		*p;
			struct _data	*a;
			struct _pair	*m;
		} elem;
		typedef union {
			U8		b;
			S8		c;
			U16		w;
			S16		s;
			U32		ui;
			S32		i;
			U64		ull;
			S64		ll;
			float		f;
			double		d;
			struct {
				U16	l;
				elem	e;
			} a16;
			struct {
				U32	l;
				elem	e;
			} a32;
		} udata;
		U8 type;
		udata u;
	} data;
protected:
	char *m_p;
	size_t m_s, m_c;
public:
	__mp() : m_p(NULL), m_s(0), m_c(0) {}
	void start(char *p, size_t s) { m_p = p; m_s = s; m_c = 0; }
	inline void pushnil() { push(NIL); }
	inline void push(U8 u) { m_p[m_c++] = (char)u; }
	inline void pop(U8 &u) { u = (U8)m_p[m_c++]; }
	inline void push(S8 s) { m_p[m_c++] = (char)s; }
	inline void pop(S8 &s) { s = (S8)m_p[m_c++]; }
	inline void push(U16 u) { SET_16(m_p + m_c, htons(u)); m_c += sizeof(u); }
	inline void pop(U16 &u) { u = ntohs(GET_16(m_p + m_c)); m_c += sizeof(U16); }
	inline void push(S16 s) { SET_16(m_p + m_c, htons(s)); m_c += sizeof(s); }
	inline void pop(S16 &s) { s = ntohs(GET_16(m_p + m_c)); m_c += sizeof(S16); }
	inline void push(U32 u) { SET_32(m_p + m_c, htonl(u)); m_c += sizeof(u); }
	inline void pop(U32 &u) { u = ntohl(GET_32(m_p + m_c)); m_c += sizeof(U32); }
	inline void push(S32 s) { SET_32(m_p + m_c, htonl(s)); m_c += sizeof(s); }
	inline void pop(S32 &s) { s = ntohl(GET_32(m_p + m_c)); m_c += sizeof(S32); }
	inline void push(U64 u) { SET_64(m_p + m_c, htonll(u)); m_c += sizeof(u); }
	inline void pop(U64 &u) { u = ntohll(GET_64(m_p + m_c)); m_c += sizeof(U64); }
	inline void push(S64 s) { SET_64(m_p + m_c, htonll(s)); m_c += sizeof(s); }
	inline void pop(S64 &s) { s = ntohll(GET_64(m_p + m_c)); m_c += sizeof(S64); }
#if defined(CHECK_LENGTH)
#undef CHECK_LENGTH
#endif
#define CHECK_LENGTH(len)	if ((m_c + len) >= m_s) { 	\
	TRACE("length error %s(%u)\n", __FILE__, __LINE__);	\
	ASSERT(false);										\
	return NBR_ESHORT; }
	inline int curpos() const { return m_c; }
	inline int rewind(U32 sz) {
		if (m_c < sz) { return m_c; }
		m_c -= sz;
		return m_c;
	}
	inline int skip(U32 sz) {
		CHECK_LENGTH(sz);
		m_c += sz;
		return m_c;
	}
	inline int operator << (bool f) { 
		CHECK_LENGTH(1);
		if (f) { push(BOOLEAN_TRUE); }
		else { push(BOOLEAN_FALSE); }
		return m_c; 
	}
	inline int operator >> (bool &f) {
		CHECK_LENGTH(1);
		U8 v; pop(v);
		if (v == BOOLEAN_TRUE) { f = true; return m_c; }
		if (v == BOOLEAN_FALSE) { f = false; return m_c; }
		return NBR_EINVAL;
	}
	inline int operator << (U8 u) { 
		if (u <= P_FIXNUM_END) { 
			CHECK_LENGTH(1); 
			push(u); 
		} 
		else { 
			CHECK_LENGTH(2); 
			push(UINT_8BIT); 
			push(u); 
		}
		return m_c;
	}
	inline int operator >> (U8 &u) {
		CHECK_LENGTH(1); 
		U8 v; pop(v);
		if (v <= P_FIXNUM_END) { 
			u = v;
		}
		else if (v == UINT_8BIT) {
			CHECK_LENGTH(1);
			pop(u);
		}
		return m_c;
	}
	inline int operator << (S8 s) {
		if (((U8)s) >= N_FIXNUM_START) { 
			CHECK_LENGTH(1);
			push(s); 
		}
		else { 
			CHECK_LENGTH(2);
			push(SINT_8BIT); 
			push(s); 
		}
		return m_c;
	}
	inline int operator >> (S8 &s) {
		CHECK_LENGTH(1);
		U8 v; pop(v);
		if (v >= N_FIXNUM_START) {
			s = (S8)v;
		}
		else if (v == SINT_8BIT) {
			CHECK_LENGTH(1);
			pop(s);
		}
		return m_c;
	}
	inline int operator << (U16 u) {
		CHECK_LENGTH(3); push(UINT_16BIT); push(u); return m_c; 
	}
	inline int operator >> (U16 &u) {
		CHECK_LENGTH(3); U8 v; pop(v);
		if (v == UINT_16BIT) { pop(u); }
		return m_c;
	}
	inline int operator << (S16 s) { 
		CHECK_LENGTH(3); push(SINT_16BIT); push(s); return m_c; 
	}
	inline int operator >> (S16 &s) {
		CHECK_LENGTH(3); U8 v; pop(v);
		if (v == SINT_16BIT) { pop(s); }
		return m_c;
	}
	inline int operator << (U32 u) { 
		CHECK_LENGTH(5); push(UINT_32BIT); push(u); return m_c; 
	}
	inline int operator >> (U32 &u) {
		CHECK_LENGTH(5); U8 v; pop(v); 
		if (v == UINT_32BIT) { pop(u); }
		return m_c;
	}
	inline int operator << (S32 s) { 
		CHECK_LENGTH(5); push(SINT_32BIT); push(s); return m_c; 
	}
	inline int operator >> (S32 &s) {
		CHECK_LENGTH(5); U8 v; pop(v);
		if (v == SINT_32BIT) { pop(s); }
		return m_c;
	}
	inline int operator << (U64 u) { 
		CHECK_LENGTH(9); push(UINT_64BIT); push(u); return m_c; 
	}
	inline int operator >> (U64 &u) {
		CHECK_LENGTH(9); U8 v; pop(v); 
		if (v == UINT_64BIT) { pop(u); }
		return m_c;
	}
	inline int operator << (S64 s) { 
		CHECK_LENGTH(9); push(SINT_64BIT); push(s); return m_c; 
	}
	inline int operator >> (S64 &s) {
		CHECK_LENGTH(9); U8 v; pop(v); 
		if (v == SINT_64BIT) { pop(s); }
		return m_c;
	}
	inline int operator << (float f) {
		CHECK_LENGTH(sizeof(float) + 1);
		push(FLOAT); 
		char *p_f = (char *)(&f);
		if (sizeof(float) == sizeof(U64)) {
			push(*((U64 *)p_f));
		}
		else if (sizeof(float) == sizeof(U32)) {
			push(*((U32 *)p_f));
		}
		return m_c;
	}
	inline int operator >> (float &f) {
		CHECK_LENGTH(sizeof(float) + 1);
		U8 v; pop(v);
		if (v == FLOAT) {
			char *p_f = (char *)&f;
			if (sizeof(float) == sizeof(U64)) {
				pop(*((U64 *)p_f));
			}
			else if (sizeof(float) == sizeof(U32)) {
				pop(*((U32 *)p_f));
			}
			return m_c;
		}
		return NBR_EINVAL;
	}
	inline int operator << (double f) {
		CHECK_LENGTH(sizeof(double) + 1);
		push(DOUBLE);
		char *p_f = (char *)(&f);
		if (sizeof(double) == sizeof(U64)) {
			push(*((U64 *)p_f));
		}
		else if (sizeof(double) == sizeof(U32)) {
			push(*((U32 *)p_f));
		}
		return m_c;
	}
	inline int operator >> (double &f) {
		CHECK_LENGTH(sizeof(double) + 1);
		U8 v; pop(v);
		if (v == DOUBLE) {
			char *p_f = (char *)&f;
			if (sizeof(double) == sizeof(U64)) {
				pop(*((U64 *)p_f));
			}
			else if (sizeof(double) == sizeof(U32)) {
				pop(*((U32 *)p_f));
			}
			return m_c;
		}
		return NBR_EINVAL;
	}
	inline int push_data(const data &d) {
		ASSERT(false);
		return m_c;
	}
	inline int operator << (const data &d) {
		size_t org = m_c;
		if (push_data(d) < 0) {
			m_c = org;
			return NBR_ESHORT;
		}
		return m_c;
	}
	inline int pop_data(data &d) {
		ASSERT(false);
		return m_c;
	}
	inline int operator >> (data &d) {
		size_t org = m_c;
		if (pop_data(d) < 0) {
			m_c = org;
			return NBR_ESHORT;
		}
		return m_c;
	}
	inline int push_raw_len(size_t l) {
		if (l <= 0xF) {
			CHECK_LENGTH(1);
			push(((U8)(FIXRAW_MASK | (U8)l)));
		}
		else if (l <= 0xFFFF) {
			CHECK_LENGTH(3);
			push(RAW16);
			push(((U16)l));
		}
		else {
			CHECK_LENGTH(5);
			push(RAW32);
			push(((U32)l));
		}
		return m_c;
	}
	inline int push_raw_onlydata(const char *p, size_t l) {
		CHECK_LENGTH(l);
		memcpy(&(m_p[m_c]), p, l);
		m_c += l;
		return m_c;
	}
	inline int push_raw(const char *p, size_t l) {
		int r = push_raw_len(l);
		if (r < 0) { return r; }
		return push_raw_onlydata(p, l);
	}
	inline int push_string(const char *str, size_t l) {
		return push_raw(str, l + 1);	/* last '\0' also */
	}
	inline int pop_raw_len(size_t &l) {
		U8 v; pop(v);
		if ((v & FIXRAW_MASK) == FIXRAW_MASK) {
			l = v & 0xF;
		}
		else if (v == RAW16) {
			CHECK_LENGTH(2);
			U16 s; pop(s);
			l = (size_t)s;
		}
		else if (v == RAW32) {
			CHECK_LENGTH(4);
			U32 u; pop(u);
			l = (size_t)u;
		}
		else {
			ASSERT(false); return NBR_EFORMAT;
		}
		return m_c;
	}
	inline int pop_raw(char *p, size_t l) {
		memcpy(p, &(m_p[m_c]), l);
		m_c += l;
		return m_c;
	}
	inline int push_array_len(size_t l) {
		if (l <= 0xF) {
			CHECK_LENGTH(1);
			push(((U8)(FIXARRAY_MASK | (U8)l)));
		}
		else if (l <= 0xFFFF) {
			CHECK_LENGTH(3);
			push(ARRAY16);
			push(((U16)l));
		}
		else {
			CHECK_LENGTH(5);
			push(ARRAY32);
			push(((U32)l));
		}
		return m_c;
	}
	inline int push_array(const data *a, size_t l) {
		int r = push_array_len(l);
		if (r < 0) { return r; }
		for (size_t s = 0; s < l; s++) {
			if ((*this << a[s]) < 0) { return NBR_ESHORT; }
		}
		return m_c;
	}
	inline int pop_array_len(size_t &l) {
		CHECK_LENGTH(1);
		U8 v; pop(v);
		if ((v & FIXARRAY_MASK) == FIXARRAY_MASK) {
			l = (v & 0xF);
		}
		else if (v == ARRAY16) {
			CHECK_LENGTH(2);
			U16 s; pop(s);
			l = (size_t)s;
		}
		else if (v == ARRAY32) {
			CHECK_LENGTH(4);
			U32 i; pop(i);
			l = (size_t)i;
		}
		else { ASSERT(false); return NBR_EFORMAT; }
		return m_c;
	}
	inline int pop_array(data *a, size_t l) {
		for (size_t i = 0; i < l; i++) {
			if ((*this >> a[i]) < 0) { return NBR_EINVAL; }
		}
		return m_c;
	}
	inline int push_map_len(size_t l) {
		if (l <= 0xF) {
			CHECK_LENGTH(1);
			push(((U8)(FIXMAP_MASK | (U8)l)));
		}
		else if (l <= 0xFFFF) {
			CHECK_LENGTH(3);
			push(MAP16);
			push(((U16)l));
		}
		else {
			CHECK_LENGTH(5);
			push(MAP32);
			push(((U32)l));
		}
		return m_c;
	}
	/* size of a is l * 2 (key + value) */
	inline int push_map(const data *a, size_t l) {
		int r = push_map_len(l);
		if (r < 0) { return r; }
		for (size_t s = 0; s < l; s++) {
		if ((*this << a[2 * s    ])) { return NBR_ESHORT; }
			if ((*this << a[2 * s + 1]) < 0) { return NBR_ESHORT; }
		}
		return m_c;
	}
	inline int pop_map_len(size_t &l) {
		CHECK_LENGTH(1);
		U8 v; pop(v);
		if ((v & FIXMAP_MASK) == FIXMAP_MASK) {
			l = (v & 0xF);
		}
		else if (v == ARRAY16) {
			CHECK_LENGTH(2);
			U16 s; pop(s);
			l = (size_t)s;
		}
		else if (v == ARRAY32) {
			CHECK_LENGTH(4);
			U32 i; pop(i);
			l = (size_t)i;
		}
		else { ASSERT(false); return NBR_EFORMAT; }
		return m_c;
	}
	/* size of a is l * 2 (key + value) */
	inline int pop_map(data *a, size_t l) {
		for (size_t s = 0; s < l; s++) {
			if ((*this >> a[2 * s    ])) { return NBR_ESHORT; }
			if ((*this >> a[2 * s + 1]) < 0) { return NBR_ESHORT; }
		}
		return m_c;
	}
#undef CHECK_LENGTH
};

class mp : public __mp
{
public:
	typedef msgpack::unpacker unpacker;
	typedef msgpack::object data;
protected:
	unpacker m_upk;
public:
	mp() : __mp(), m_upk() {}
	~mp() {}
public:
	char *p() { 
		U8 u8 = (U8)__mp::m_p[0];
		if (__mp::m_c > 0 && (u8 == BOOLEAN_TRUE || u8 == BOOLEAN_FALSE)) {
			ASSERT(__mp::m_c == 1);
		}
		return __mp::m_p; 
	}
	size_t len() { return __mp::m_c; }
	void pack_start(char *p, size_t l) { __mp::start(p, l); }
	size_t unpack_remain() const { return m_upk.nonparsed_size(); }
	size_t pack_remain() const { return (__mp::m_s - __mp::m_c); }
	void unpack_start(const char *p, size_t l) { 
		data d;
		while(unpack(d) > 0);	/* read all unread data */
		m_upk.reset_zone();
		if (l > m_upk.buffer_capacity()) {
			l = m_upk.buffer_capacity();
		}
		if (l > 0) {
			memcpy(m_upk.buffer(), p, l);
			m_upk.buffer_consumed(l);
		}
	}
	int unpack(data &data) { 
		try {
			int r = m_upk.execute();
//			TRACE("msgpack:unpacker:result:%d\n", r);
			if (r > 0) {
				data = m_upk.data();
				m_upk.reset();
				return 1;
			}
			else {
				return 0;
			}
		} catch (const msgpack::type_error e) {
			return -1;
		}
	}	
};

}	//serialize
}	//vm
}	//sfc

#endif

