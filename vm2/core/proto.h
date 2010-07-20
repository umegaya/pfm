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
	node_inquiry = 8,
	authentication = 9,
	logout = 10,
	ll_exec_local = 11,
	node_regist = 12,
	ll_exec_client = 13,
	start_replicate = 14,
};
} /* namespace rpc */
enum node_type {
	servant_node,
	master_node,
	client_node,
	test_servant_node,
};
enum {
	replicate_invalid = 0,
	replicate_normal = 1,	/* normal replication (apply update) */
	replicate_move_to = 2,	/* object should move by rehash */
	start_replicate_normal = 11,	/* start 1 normal replicate */
	start_replicate_move_to = 12,	/* start 1 rehash replicate */
};
enum ll_exec_error {
	ll_exec_error_move_to = -1000,
	ll_exec_error_will_move_to = -1001,
};

/* classes */
/* base */
typedef const char *world_id;
static const U32 max_wid = 256;
typedef class object pfmobj;

namespace rpc {
/* protocol class base */
class data : public serializer::data {
public:
	data() : serializer::data() {}
	~data() {}
	operator const UUID & () const {
		ASSERT(serializer::data::type() == datatype::BLOB);
		ASSERT(via.raw.size == sizeof(UUID));
		ASSERT(((const void *)*this) == via.raw.ptr);
		return *((const UUID *)(const void *)*this);
	}
	operator const UUID * () const {
		ASSERT(serializer::data::type() == datatype::BLOB);
		ASSERT(via.raw.size == sizeof(UUID));
		ASSERT(((const void *)*this) == via.raw.ptr);
		return ((UUID *)(const void *)*this);
	}
	operator UUID * () {
		ASSERT(serializer::data::type() == datatype::BLOB);
		ASSERT(via.raw.size == sizeof(UUID));
		ASSERT(((const void *)*this) == via.raw.ptr);
		return ((UUID *)(const void *)*this);
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
	static inline void replace_msgid(MSGID new_msgid, char *p, int l);
	static inline void replace_method(U8 method, char *p, int l);
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
	/* FIXME : it depend on current lua implementation */
	const UUID &rcvr_uuid() const { return rcvr().elem(2); }
	void set_uuid(const UUID &uuid) {	/* FIXME : black magic */
		return super::argv(1).set_ptr((const void *)&uuid);
	}
	int argc() const { return super::argc() - 2; }
	const data &argv(int n) const { return super::argv(n+2); }
	static inline int pack_header(serializer &sr, MSGID msgid,
			pfmobj &o, const char *method, size_t mlen,
			world_id wid, size_t wlen, U8 mth, int n_arg);
	static inline ll_exec_request &cast(request &r) {
		ASSERT((U32)r.method() == ll_exec ||
				(U32)r.method() == ll_exec_local ||
				(U32)r.method() == ll_exec_client);
		return (ll_exec_request &)r;
	}
};
class ll_exec_response : public response {
public:
	RESPONSE_CASTER(ll_exec);
};

/* create_object */
class create_object_request : public world_request {
public:
	typedef world_request super;
	const char *klass() const { return super::argv(0); }
	const UUID &object_id() const { return super::argv(1); }
	const char *node_addr() const { return 
		super::argv(2).type() == datatype::NIL ? "" : super::argv(2); }
	int argc() const { return super::argc() - 3; }
	const data &argv(int n) const { return super::argv(n+3); }
	bool need_load() const { return ((U32)method()) == load_object; }
	static inline int pack_header(serializer &sr, MSGID msgid,
			const UUID &uuid, const char *klass, size_t klen,
			world_id wid, size_t wlen, const char *addr, int n_arg);
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
class replicate_request : public world_request {
public:
	typedef world_request super;
	const UUID &uuid() const { return super::argv(0); }
	const data &type() const { return super::argv(1); }
	const data &log() const { return super::argv(2); }
	static int pack_header(serializer &sr, U8 cmd, MSGID msgid,
			world_id wid, size_t wlen,
			const UUID &uuid, U8 rep_type);
	static inline replicate_request &cast(request &r) {
		ASSERT((U32)r.method() == replicate ||
				(U32)r.method() == start_replicate);
		return (replicate_request &)r;
	}
#define MAKE_REPL_CMD(method, rep_type) (10 * ((U32)method) + ((U32)rep_type))
};

class replicate_response : public response {
public:
	const UUID &uuid() const { return response::ret(); }
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

/* logout */
class logout_request : public world_request {
public:
	typedef world_request super;
	const data &account() const { return super::argv(0); }
	static inline int pack_header(serializer &sr, MSGID msgid,
			world_id wid, size_t wlen,
			const char *account);
	REQUEST_CASTER(logout);
};

/* authentication */
class authentication_request : public world_request {
public:
	typedef world_request super;
	const data &account() const { return super::argv(0); }
	const data &authdata() const { return super::argv(1); }
	static inline int pack_header(serializer &sr, MSGID msgid,
			world_id wid, size_t wlen,
			const char *account, const char *authdata, size_t dlen);
	REQUEST_CASTER(authentication);
};

/* node_inquery */
class node_inquiry_request : public request {
public:
	typedef request super;
	static inline int pack_header(serializer &sr, MSGID msgid, int node_type) {
		super::pack_header(sr, msgid, node_inquiry, 1);
		sr << node_type;
		return sr.len();
	}
	int node_type() const { return super::argv(0); }
	REQUEST_CASTER(node_inquiry);
};

class node_inquiry_response : public response {
public:
	typedef response super;
	const data &node_addr() { return super::ret(); }
	static inline int pack_header(serializer &sr, MSGID msgid,
			const char *node_addr, int alen);
	RESPONSE_CASTER(node_inquiry);
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
		regist = 8,
	};
public:
	typedef world_request super;
	const data &command() const { return super::argv(0); }
	int argc() const { return super::argc() - 1; }
	const data &argv(int n) const { return super::argv(n + 1); }
	data &argv(int n) { return super::argv(n + 1); }
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
	void assign_world_object_id() {
		UUID *puuid = super::argv(2);
		puuid->assign();
	}
	const char *srcfile() const { return super::argv(3); }
	int n_node() const { return super::argc() - 4; }
	const char *addr(int n) const { return super::argv(n+4); }
	static inline int pack_header(serializer &sr, MSGID msgid,
			world_id wid, size_t wlen,
			const char *address, size_t alen,
			world_id from, const UUID &uuid,
			const char *srcfile, int n_nodes, const char *node[]);
};

class regist : public request {
public:
	typedef request super;
	const char *node_server_addr() const { return super::argv(0); }
	int node_server_addr_len() const { return super::argv(0).len(); }
	const int node_type() const { return super::argv(1); }
	static inline int pack_header(serializer &sr, MSGID msgid,
		const char *address, size_t alen, U8 node_type);
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
inline void request::replace_msgid(MSGID new_id, char *p, int l)
{
	/* p, l, must be packed by request::pack_header */
	ASSERT(l >= 6);
	/* array_len(1), msg_request(1), signiture for U32 (1), msgid (4) */
	SET_32((p + 3), htonl(new_id));
}
inline void request::replace_method(U8 mth, char *p, int l)
{
	/* p, l, must be packed by request::pack_header */
	ASSERT(l >= 7);
	/* array_len(1), msg_request(1), signiture for U32 (1), msgid (4), rtype(1) */
	SET_8((p + 7), mth);
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
		world_id wid, size_t wlen, U8 mth, int n_arg)
{
	super::pack_header(sr, msgid, mth, wid, wlen, n_arg + 2);
	sr.push_string(method, mlen);
	super::pack_object(sr, o);
	return sr.len();
}

inline int create_object_request::pack_header(
		serializer &sr, MSGID msgid, const UUID &uuid,
		const char *klass, size_t klen,
		world_id wid, size_t wlen, const char *addr, int n_arg)
{
	super::pack_header(sr, msgid,
		addr ? load_object : create_object, wid, wlen, n_arg + 3);
	sr.push_string(klass, klen);
	sr.push_raw(reinterpret_cast<const char *>(&uuid), sizeof(UUID));
	if (addr) { 
		sr.push_string(addr, nbr_str_length(addr, 32)); 
	}
	else {
		sr.pushnil();
	}
	return sr.len();
}

inline int replicate_request::pack_header(
			serializer &sr, U8 cmd, MSGID msgid, world_id wid, size_t wlen,
			const UUID &uuid, U8 rep_type)
{
	super::pack_header(sr, msgid, cmd, wid, wlen, 3);
	sr.push_raw(reinterpret_cast<const char *>(&uuid), sizeof(UUID));
	sr << rep_type;
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

inline int logout_request::pack_header(serializer &sr, MSGID msgid,
		world_id wid, size_t wlen, const char *account)
{
	super::pack_header(sr, msgid, logout, wid, wlen, 1);
	sr.push_string(account, nbr_str_length(account, login_request::max_account));
	return sr.len();
}

inline int authentication_request::pack_header(serializer &sr, MSGID msgid,
		world_id wid, size_t wlen,
		const char *account, const char *authdata, size_t dlen)
{
	super::pack_header(sr, msgid, authentication, wid, wlen, 2);
	sr.push_string(account, nbr_str_length(account, login_request::max_account));
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

inline int node_inquiry_response::pack_header(serializer &sr, MSGID msgid,
		const char *node_addr, int alen)
{
	super::pack_header(sr, msgid);
	sr.push_string(node_addr, alen);
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

inline int node_ctrl_cmd::regist::pack_header(serializer &sr, MSGID msgid,
		const char *address, size_t alen, U8 node_type)
{
	super::pack_header(sr, msgid, node_regist, 2);
	sr.push_string(address, alen);
	sr << node_type;
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
	super::pack_header(sr, msgid, super::vm_fin, wid, wlen, 0);
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

