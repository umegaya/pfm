#if !defined(__PROTO_H__)
#define __PROTO_H__

#include "nbr.h"
#if !defined(NBR_INLINE)
#define NBR_INLINE static inline
#endif
#include "str.h"
#include "uuid.h"
#include "serializer.h"

namespace pfm {
namespace rpc {
/* message types */
enum {
	msg_request = 0,
	msg_response = 1,
};
/* request types */
enum {
	ll_exec = 1,
	create_object = 2,
	replicate = 3,
	login = 4,
	create_world = 5,
};
} /* namespace rpc */

/* classes */
/* base */
class msgid_generator {
protected:
	U32 m_msgid_seed;
	static const U32 MSGID_NORMAL_LIMIT = 2000000000;
	static const U32 MSGID_COMPACT_LIMIT = 60000;
public:
	typedef U32 NMSGID;
	typedef U16 CMSGID;
	msgid_generator() : m_msgid_seed(0) {}
	inline NMSGID normal_new_id() {
		__sync_val_compare_and_swap(&m_msgid_seed, MSGID_NORMAL_LIMIT, 0);
		return __sync_add_and_fetch(&m_msgid_seed, 1);
	}
	inline CMSGID compact_new_id() {
		__sync_val_compare_and_swap(&m_msgid_seed, MSGID_COMPACT_LIMIT, 0);
		return __sync_add_and_fetch(&m_msgid_seed, 1);
	}
#if !defined(_USE_COMPACT_MSGID)
	typedef NMSGID MSGID;
	inline MSGID new_id() { return normal_new_id(); }
#else
	typedef CMSGID MSGID;
	inline CMSGID new_id() { return compact_new_id(); }
#endif
};
/* typedefs */
typedef msgid_generator::MSGID MSGID;
static const MSGID INVALID_MSGID = 0;
typedef const char *world_id;
static const U32 max_wid = 256;
typedef class object pfmobj;

namespace rpc {
/* protocol class base */
class data : public serializer::data {
public:
	operator const UUID & () const { 
		ASSERT(serializer::data::type() == datatype::BLOB); 
		ASSERT(via.raw.size == sizeof(UUID));
		return *((const UUID *)(const void *)*this);
	}
	const data &elem(int n) const { return (const data &)serializer::data::elem(n); }
	data &elem(int n) { return (data &)serializer::data::elem(n); }
	const data &key(int i) const { return (const data &)serializer::data::key(i); }
	data &key(int i) { return (data &)serializer::data::key(i); }
	const data &val(int i) const { return (const data &)serializer::data::val(i); }
	data &val(int i) { return (data &)serializer::data::val(i); }
	void set_ptr(const void *p) { serializer::data::set_ptr(p); }
};
class request : public data {
public:
	typedef data super;
	const data &method() const { return super::elem(2); }
	MSGID msgid() const { return super::elem(1); }
	int argc() const { return super::elem(3).size(); }
	const data &argv(int n) const { return super::elem(3).elem(n); }
	data &argv(int n) { return super::elem(3).elem(n); }
	static inline int pack_header(serializer &sr, MSGID msgid,
		U8 cmd, int n_arg);
};
class world_request : public request {
public:
	typedef request super;
	const data &wid() const { return super::argv(0); }
	/* FIXME : black magic... */
	void set_world_id(world_id wid) { super::argv(0).set_ptr(wid); }
	int argc() const { return super::argc() - 1; }
	const data &argv(int n) const { return super::argv(n + 1); }
	data &argv(int n) { return super::argv(n + 1); }
	static int pack_object(serializer &sr, pfmobj &o);
	static inline int pack_header(serializer &sr, MSGID msgid,
		U8 cmd, world_id wid, size_t wlen, int n_arg);
	static world_request &cast(request &r) {
		ASSERT(r.argv(0).type() == (int)datatype::BLOB);
		return (world_request &)r; 
	}
};
class response : public data {
public:
	typedef data super;
	MSGID msgid() { return super::elem(1); }
	const data &ret() const { return super::elem(2); }
	const data &err() const { return super::elem(3); }
	bool success() const { return err().type() == (int)datatype::NIL; }
	static inline int pack_header(serializer &sr, MSGID msgid);
};

#define REQUEST_CASTER(type) static inline type##_request &cast(request &r) {\
	ASSERT((U32)r.method() == type); return (type##_request &)r; }
#define RESPONSE_CASTER(type) static inline type##_response &cast(response &r) {\
	return (type##_response &)r; }

/* ll_exec */
class ll_exec_request : public world_request {
public:
	typedef world_request super;
	const data &method() const { return super::argv(0); }
	const data &rcvr() const { return super::argv(1); }
	void set_uuid(const UUID &uuid) {	/* FIXME : black magic */
		return super::argv(1).set_ptr((const void *)&uuid);
	}
	int argc() const { return super::argc() - 2; }
	const data &argv(int n) const { return super::argv(n+2); }
	static inline int pack_header(serializer &sr, MSGID msgid,
			pfmobj &o, const char *method, size_t mlen,
			world_id wid, size_t wlen, int n_arg);
	REQUEST_CASTER(ll_exec);
};
class ll_exec_response : public response {
public:
	RESPONSE_CASTER(ll_exec);
};

/* create_object */
class create_object_request : public world_request {
public:
	typedef world_request super;
	const data &klass() const { return super::argv(0); }
	const UUID &object_id() const { return super::argv(1); }
	int argc() const { return super::argc() - 2; }
	const data &argv(int n) const { return super::argv(n+2); }
	static inline int pack_header(serializer &sr, MSGID msgid,
			const UUID &uuid, const char *klass, size_t klen,
			world_id wid, size_t wlen, int n_arg);
	REQUEST_CASTER(create_object);
};
class create_object_response : public response {
public:
	static inline int pack_header(serializer &sr, MSGID msgid,
			pfmobj *o, const char *e, size_t el);
	RESPONSE_CASTER(create_object);
};

/* create_world */
class create_world_request : public world_request
{
public:
	typedef world_request super;
	world_id from() const { return super::argv(0); }
	const UUID &world_object_id() const { return super::argv(1); }
	const char *srcfile() const { return super::argv(2); }
	int n_node() const { return super::argc() - 3; }
	const data &addr(int n) const { return super::argv(n+3); }
	static inline int pack_header(serializer &sr, MSGID msgid,
			world_id wid, world_id from, const UUID &uuid,
			const char *srcfile, int n_nodes, const char *node[]);
	REQUEST_CASTER(create_world);
};

class create_world_response : public response
{
public:
	world_id wid() const { return response::ret().elem(0); }
	const UUID &world_object_id() const { return response::ret().elem(1); }
	static int pack_header(serializer &sr, MSGID msgid,
			world_id wid, size_t wlen, const UUID &uuid);
	RESPONSE_CASTER(create_world);
};

inline int request::pack_header(serializer &sr, MSGID msgid, U8 reqtype, int n_arg)
{
	sr.push_array_len(4);
	sr << ((U8)msg_request);
	sr << msgid;
	sr << reqtype;
	sr.push_array_len(n_arg);
	return sr.len();
}

inline int response::pack_header(serializer &sr, MSGID msgid)
{
	sr.push_array_len(4);
	sr << ((U8)msg_response);
	sr << msgid;
	return sr.len();
}

inline int world_request::pack_header(serializer &sr, MSGID msgid, U8 reqtype,
		world_id wid, size_t wlen, int n_arg)
{
	super::pack_header(sr, msgid, reqtype, n_arg + 1);
	sr.push_string(wid, wlen);
	return sr.len();
}

inline int ll_exec_request::pack_header(serializer &sr, MSGID msgid,
		pfmobj &o, const char *method, size_t mlen,
		world_id wid, size_t wlen, int n_arg)
{
	super::pack_header(sr, msgid, ll_exec, wid, wlen, n_arg + 2);
	sr.push_string(method, mlen);
	super::pack_object(sr, o);
	return sr.len();
}

inline int create_object_request::pack_header(
		serializer &sr, MSGID msgid, const UUID &uuid,
		const char *klass, size_t klen,
		world_id wid, size_t wlen, int n_arg)
{
	super::pack_header(sr, msgid, create_object, wid, wlen, n_arg + 2);
	sr.push_string(klass, klen);
	sr.push_raw(reinterpret_cast<const char *>(&uuid), sizeof(UUID));
	return sr.len();

}

inline int create_object_response::pack_header(
			serializer &sr, MSGID msgid, pfmobj *o, const char *e, size_t el)
{
	response::pack_header(sr, msgid);
	o ? world_request::pack_object(sr, *o) : sr.pushnil();
	e ? sr.push_string(e, el) : sr.pushnil();
	return sr.len();
}

inline int create_world_request::pack_header(serializer &sr, MSGID msgid,
		world_id wid, world_id from, const UUID &uuid,
		const char *srcfile, int n_nodes, const char *node[])
{
	super::pack_header(sr, msgid, create_world,
		wid, nbr_str_length(wid, max_wid), n_nodes + 3);
	sr.push_string(from, nbr_str_length(from, max_wid));
	sr.push_raw(reinterpret_cast<const char *>(&uuid), sizeof(UUID));
	sr.push_string(srcfile, nbr_str_length(srcfile, 256));
	for (int i = 0; i < n_nodes; i++) {
		sr.push_string(node[i], nbr_str_length(node[i], 32));
	}
	return sr.len();
}

inline int create_world_response::pack_header(serializer &sr, MSGID msgid,
		world_id wid, size_t wlen, const UUID &uuid)
{
	response::pack_header(sr, msgid);
	sr.push_array_len(2);
	sr.push_string(wid, wlen);
	sr.push_raw(reinterpret_cast<const char *>(&uuid), sizeof(UUID));
	sr.pushnil();
	return sr.len();

}


} /* namespace rpc */
}

#endif

