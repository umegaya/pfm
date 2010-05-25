#if !defined(__PROTO_H__)
#define __PROTO_H__

#include "nbr.h"
#include "uuid.h"
#include "serializer.h"

namespace pfm {
/* typedefs */
typedef U32 MSGID;
typedef U16 CMSGID;
typedef const char *world_id;
typedef class object pfmobj;
namespace rpc {
/* message types */
enum {
	msg_request = 0,
	msg_response = 1,
};
/* request types */
enum {
	req_ll = 1,
	req_create_object = 2,
	req_replicate = 3,
	req_login = 4,
	req_create_world = 5,
};

/* classes */
/* base */
class data : public serializer::data {
public:
	operator const UUID & () const { 
		ASSERT(serializer::data::type() == datatype::BLOB); 
		ASSERT(via.raw.size == sizeof(UUID));
		return *((const UUID *)via.raw.ptr); 
	}
	const data &elem(int n) const { return (const data &)serializer::data::elem(n); }
	data &elem(int n) { return (data &)serializer::data::elem(n); }
	const data &key(int i) const { return (const data &)serializer::data::key(i); }
	data &key(int i) { return (data &)serializer::data::key(i); }
	const data &val(int i) const { return (const data &)serializer::data::val(i); }
	data &val(int i) { return (data &)serializer::data::val(i); }
};
class request : public data {
public:
	typedef data super;
	const data &method() const { return super::elem(1); }
	MSGID msgid() const { return super::elem(2); }
	int argc() const { return super::elem(3).size(); }
	const data &argv(int n) const { return super::elem(3).elem(n); }
	static int pack_object(serializer &sr, pfmobj &o);
	static inline int pack_header(serializer &sr, MSGID msgid, U8 cmd, int n_arg);
};
class response : public data {
public:
	typedef data super;
	MSGID msgid() { return super::elem(1); }
	const data &ret() const { return super::elem(2); }
	const data &err() const { return super::elem(3); }
	static inline int pack_header(serializer &sr, MSGID msgid);
};

/* req_ll */
class ll_request : public request {
public:
	int argc() const { return request::argc() - 3; }
	const data &method() const { return request::argv(0); }
	const data &argv(int n) const { return request::argv(n+3); }
	world_id wid() const { return request::argv(1); }
	const data &rcvr() const { return request::argv(2); }
	static inline int pack_header(serializer &sr, MSGID msgid,
			pfmobj &o, const char *method, size_t mlen,
			world_id wid, size_t wlen, int n_arg);
};
class ll_response : public response {
};

/* req_create_object */
class create_object_request : public request {
public:
	const data &klass() const { return request::argv(0); }
	world_id wid() const { return request::argv(1); }
	const UUID &object_id() const { return request::argv(2); }
	int argc() const { return request::argc() - 3; }
	const data &argv(int n) const { return request::argv(n+3); }
	static inline int pack_header(serializer &sr, MSGID msgid,
			const UUID &uuid, const char *klass, size_t klen,
			world_id wid, size_t wlen, int n_arg);
};
class create_object_response : public response {
public:
	static inline int pack_header(serializer &sr, MSGID msgid,
			pfmobj *o, const char *e, size_t el);
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

inline int ll_request::pack_header(serializer &sr, MSGID msgid,
		pfmobj &o, const char *method, size_t mlen,
		world_id wid, size_t wlen, int n_arg)
{
	request::pack_header(sr, msgid, req_ll, n_arg + 3);
	sr.push_string(method, mlen);
	sr.push_string(wid, wlen);
	request::pack_object(sr, o);
	return sr.len();
}

inline int create_object_request::pack_header(
		serializer &sr, MSGID msgid, const UUID &uuid,
		const char *klass, size_t klen,
		world_id wid, size_t wlen, int n_arg)
{
	request::pack_header(sr, msgid, req_create_object, n_arg + 3);
	sr.push_string(klass, klen);
	sr.push_string(wid, wlen);
	sr.push_raw(reinterpret_cast<const char *>(&uuid), sizeof(UUID));
	return sr.len();

}

inline int create_object_response::pack_header(
			serializer &sr, MSGID msgid, pfmobj *o, const char *e, size_t el)
{
	response::pack_header(sr, msgid);
	o ? request::pack_object(sr, *o) : sr.pushnil();
	e ? sr.push_string(e, el) : sr.pushnil();
	return sr.len();
}

}
}

#endif

