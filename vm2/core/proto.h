#if !defined(__PROTO_H__)
#define __PROTO_H__

#include "nbr.h"
#if !defined(NBR_INLINE)
#define NBR_INLINE static inline
#endif
#include "str.h"
#include "uuid.h"
#include "serializer.h"
#include "msgid.h"

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
	node_ctrl = 5,
	vote = 6,			/* for commit protocol */
	load_object = 7,
};
} /* namespace rpc */

/* classes */
/* base */
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
	template <class CMD> operator CMD &() { return (CMD &)*this; }
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
	bool need_load() const { return ((U32)method()) == load_object; }
	static inline int pack_header(serializer &sr, MSGID msgid,
			const UUID &uuid, const char *klass, size_t klen,
			world_id wid, size_t wlen, bool load, int n_arg);
	static inline create_object_request &cast(request &r) {
		ASSERT((U32)r.method() == create_object ||
				(U32)r.method() == load_object);
		return (create_object_request &)r;
	}
};
class create_object_response : public response {
public:
	RESPONSE_CASTER(create_object);
};

/* replicate */
class replicate_request : public request {
public:
	const data &uuid() const { return request::argv(0); }
	const data &log() const { return request::argv(1); }
	static int pack_header(serializer &sr, MSGID msgid, const UUID &uuid,
			int log_size);
	REQUEST_CASTER(replicate);
};

class replicate_response : public response {
public:
	RESPONSE_CASTER(replicate);
};

/* login */
class login_request : public world_request {
public:
	static const U32 max_account = 256;
	typedef world_request super;
	const data &account() const { return super::argv(0); }
	const data &authdata() const { return super::argv(1); }
	static inline int pack_header(serializer &sr, MSGID msgid,
			world_id wid, size_t wlen,
			const char *account, const char *authdata, size_t dlen);
	REQUEST_CASTER(login);
};

class login_response : public response {
public:
	typedef response super;
	const UUID &object_id() { return super::ret(); }
	static inline int pack_header(serializer &sr, MSGID msgid,
			const UUID &uuid);
	RESPONSE_CASTER(login);
};

/* node control */
class node_ctrl_request : public world_request {
public:
	enum {
		add = 1,
		del = 2,
		list = 3,
		deploy = 4,
		vm_init = 5,
		vm_fin = 6,
		vm_deploy = 7,
	};
public:
	typedef world_request super;
	const data &command() const { return super::argv(0); }
	int argc() const { return super::argc() - 1; }
	const data &argv(int n) const { return super::argv(n + 1); }
	static inline int pack_header(
			serializer &sr, MSGID msgid,
			U8 cmd, world_id wid, size_t wlen, int n_arg);
	REQUEST_CASTER(node_ctrl);
	template <class CMD> operator CMD &() { return (CMD &)*this; }
};

namespace node_ctrl_cmd {
class add : public node_ctrl_request {
public:
	typedef node_ctrl_request super;
	const data &node_addr() const { return super::argv(0); }
	world_id from() const { return super::argv(1); }
	const UUID &world_object_id() const { return super::argv(2); }
	const data &srcfile() const { return super::argv(3); }
	int n_node() const { return super::argc() - 4; }
	const data &addr(int n) const { return super::argv(n+4); }
	static inline int pack_header(serializer &sr, MSGID msgid,
			world_id wid, size_t wlen,
			const char *address, size_t alen,
			world_id from, const UUID &uuid,
			const char *srcfile, int n_nodes, const char *node[]);
};

class vm_init : public node_ctrl_request {
public:
	typedef node_ctrl_request super;
	world_id from() const { return super::argv(0); }
	const data &srcfile() const { return super::argv(1); }
	static inline int pack_header(serializer &sr, MSGID msgid,
			world_id wid, size_t wlen,
			world_id from, const char *srcfile);
};

class del : public node_ctrl_request {
public:
	typedef node_ctrl_request super;
	const data &node_addr() const { return super::argv(0); }
	static inline int pack_header(serializer &sr, MSGID msgid, 
			world_id wid, size_t wlen,
			const char *address, size_t alen);
};

class vm_fin : public node_ctrl_request {
public:
	typedef node_ctrl_request super;
	static inline int pack_header(serializer &sr, MSGID msgid,
		world_id wid, size_t wlen);
};

class list : public node_ctrl_request {
public:
	typedef node_ctrl_request super;
	static inline int pack_header(serializer &sr, MSGID msgid, 
			world_id wid, size_t wlen);
};

class deploy : public node_ctrl_request {
public:
	typedef node_ctrl_request super;
	const data &srcfile() const { return super::argv(0); }
	static inline int pack_header(serializer &sr, MSGID msgid, 
			world_id wid, size_t wlen,
			const char *srcpath);
};

class vm_deploy : public node_ctrl_request {
public:
	typedef node_ctrl_request super;
	const data &srcfile() const { return super::argv(0); }
	static inline int pack_header(serializer &sr, MSGID msgid,
			world_id wid, size_t wlen,
			const char *srcpath);
};
}


class node_ctrl_response : public response {
public:
	RESPONSE_CASTER(node_ctrl);
};


inline int request::pack_header(serializer &sr, MSGID msgid, U8 reqtype, int n_arg)
{
	sr.set_curpos(0);
	sr.push_array_len(4);
	sr << ((U8)msg_request);
	sr << msgid;
	sr << reqtype;
	sr.push_array_len(n_arg);
	ASSERT(msgid != 0);
	return sr.len();
}

inline int response::pack_header(serializer &sr, MSGID msgid)
{
	sr.set_curpos(0);
	sr.push_array_len(4);
	sr << ((U8)msg_response);
	sr << msgid;
	ASSERT(msgid != 0);
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
		world_id wid, size_t wlen, bool load, int n_arg)
{
	super::pack_header(sr, msgid,
		load ? load_object : create_object, wid, wlen, n_arg + 2);
	sr.push_string(klass, klen);
	sr.push_raw(reinterpret_cast<const char *>(&uuid), sizeof(UUID));
	return sr.len();

}

inline int replicate_request::pack_header(
			serializer &sr, MSGID msgid, const UUID &uuid,
			int log_size)
{
	request::pack_header(sr, msgid, replicate, 2);
	sr.push_raw(reinterpret_cast<const char *>(&uuid), sizeof(UUID));
	sr.push_map_len(log_size);
	return sr.len();
}

inline int login_request::pack_header(serializer &sr, MSGID msgid,
		world_id wid, size_t wlen,
		const char *account, const char *authdata, size_t dlen)
{
	super::pack_header(sr, msgid, login, wid, wlen, 2);
	sr.push_string(account, nbr_str_length(account, max_account));
	sr.push_raw(authdata, dlen);
	return sr.len();
}

inline int login_response::pack_header(serializer &sr, MSGID msgid,
		const UUID &uuid)
{
	super::pack_header(sr, msgid);
	sr.push_raw(reinterpret_cast<const char *>(&uuid), sizeof(UUID));
	sr.pushnil();
	return sr.len();
}



inline int node_ctrl_cmd::add::pack_header(serializer &sr, MSGID msgid,
		world_id wid, size_t wlen,
		const char *address, size_t alen,
		world_id from, const UUID &uuid,
		const char *srcpath, int n_nodes, const char *node[])
{
	super::pack_header(sr, msgid, super::add, wid, wlen, 4 + n_nodes);
	sr.push_string(address, alen);
	sr.push_string(from, nbr_str_length(from, max_wid));
	sr.push_raw(reinterpret_cast<const char *>(&uuid), sizeof(UUID));
	sr.push_string(srcpath, nbr_str_length(srcpath, 256));
	for (int i = 0; i < n_nodes; i++) {
		sr.push_string(node[i], nbr_str_length(node[i], 256));
	}
	return sr.len();
}

inline int node_ctrl_cmd::vm_init::pack_header(serializer &sr, MSGID msgid,
		world_id wid, size_t wlen,
		world_id from, const char *srcpath)
{
	super::pack_header(sr, msgid, super::vm_init, wid, wlen, 2);
	sr.push_string(from, nbr_str_length(from, max_wid));
	sr.push_string(srcpath, nbr_str_length(srcpath, 256));
	return sr.len();
}

inline int node_ctrl_cmd::del::pack_header(serializer &sr, MSGID msgid, 
		world_id wid, size_t wlen,
		const char *address, size_t alen)
{
	super::pack_header(sr, msgid, super::del, wid, wlen, 1);
	sr.push_string(address, alen);
	return sr.len();
}

inline int node_ctrl_cmd::list::pack_header(serializer &sr, MSGID msgid, 
		world_id wid, size_t wlen)
{
	super::pack_header(sr, msgid, super::list, wid, wlen, 0);
	return sr.len();
}

inline int node_ctrl_cmd::deploy::pack_header(serializer &sr, MSGID msgid, 
		world_id wid, size_t wlen,
		const char *srcpath)
{
	super::pack_header(sr, msgid, super::deploy, wid, wlen, 1);
	sr.push_string(srcpath, nbr_str_length(srcpath, 256));
	return sr.len();
}

inline int node_ctrl_cmd::vm_fin::pack_header(serializer &sr, MSGID msgid,
		world_id wid, size_t wlen)
{
	super::pack_header(sr, msgid, super::vm_fin, wid, wlen, 1);
	return sr.len();
}

inline int node_ctrl_cmd::vm_deploy::pack_header(serializer &sr, MSGID msgid,
		world_id wid, size_t wlen,
		const char *srcpath)
{
	super::pack_header(sr, msgid, super::vm_deploy, wid, wlen, 1);
	sr.push_string(srcpath, nbr_str_length(srcpath, 256));
	return sr.len();
}

inline int node_ctrl_request::pack_header(
		serializer &sr, MSGID msgid,
		U8 cmd, world_id wid, size_t wlen, int n_arg)
{
	super::pack_header(sr, msgid, node_ctrl, wid, wlen, n_arg + 1);
	sr << cmd;
	return sr.len();
}



} /* namespace rpc */
}

#endif

