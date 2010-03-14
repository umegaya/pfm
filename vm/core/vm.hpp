#if !defined(__VM_HPP__)
#define __VM_HPP__

#include "sfc.hpp"
#include "grid.h"

/*-------------------------------------------------------------*/
/* sfc::vm::kvs                                                */
/*-------------------------------------------------------------*/
#include "tc.h"


/*-------------------------------------------------------------*/
/* sfc::vm::serialize                                          */
/*-------------------------------------------------------------*/
#include "mp.h"


/*-------------------------------------------------------------*/
/* sfc::vm::lang	                                       */
/*-------------------------------------------------------------*/
//#include "lua.hpp"


namespace sfc {
namespace cluster {
/*-------------------------------------------------------------*/
/* sfc::cluster::reliable                                      */
/*-------------------------------------------------------------*/
template <class S>
class reliable : public S {};
}	//sfc::cluster


namespace idgen {
/*-------------------------------------------------------------*/
/* sfc::idgen						       */
/*-------------------------------------------------------------*/
class mac_idgen {
public:
	struct UUID {
		U8	mac[6];
		U16	id1;
		U32	id2;
		static const int strkeylen = 24 + 1;
	public:
		UUID() { memset(this, 0, sizeof(*this)); }
		bool operator == (const UUID &uuid) const {
			return nbr_mem_cmp(this, &uuid, sizeof(UUID)) == 0;
		}
		const UUID &operator = (const UUID &uuid) {
			nbr_mem_copy(this, &uuid, sizeof(UUID));
			return *this;
		}
		const char *to_s(char *b, int bl) const {
			const U32 *p = (const U32 *)this;
			snprintf(b, bl, "%08x%08x%08x", p[0], p[1], p[2]);
			return b;
		}
	};
	static const UUID UUID_INVALID;
	static UUID UUID_SEED;
public:
	mac_idgen() {}
	~mac_idgen() {}
	static int init();
	static void fin() {}
	static const UUID &new_id();
	static const UUID &invalid_id() { return UUID_INVALID; }
	static inline bool valid(UUID &uuid) {
		return uuid.id1 != 0 || uuid.id2 != 0; }
};
}	//sfc::idgen

namespace vm {
using namespace base;
using namespace grid;
using namespace idgen;
/*-------------------------------------------------------------*/
/* sfc::vm::vmprotocol		      		                       */
/*-------------------------------------------------------------*/
/* vmprotocol */
class vmprotocol : public binprotocol {
public:
	enum {
		vmcmd_invalid,

		vmcmd_rpc,		/* c->s, c->c, s->s */
		vmcmd_new_object,	/* s->m, m->s */
		vmcmd_login,	/* c->s->m */
		vmcode_rpc,		/* s->c, c->c, s->s */
		vmcode_new_object,	/* s->m, m->s */
		vmcode_login,	/* m->s->c */
	};
	typedef enum {
		rpct_invalid,

		rpct_method,	/* r = object.func(self,a1,a2,...) */
		rpct_global,	/* r = global_func(a1,a2,...) */
		rpct_getter,	/* object[k] */
		rpct_setter,	/* object[k] = v */

		rpct_error = 255,/* error happen on remote */
	} rpctype;
	enum {	/* flag for controling fiber behavior */
		rpcopt_flag_not_set = 0x0,
		rpcopt_flag_invoked = 0x01,/* means invoked by other rpc. so
								 if fiber is finished, should reply to caller */
		rpcopt_flag_notification = 0x02,/* no need to receive response */
	};
	static const int vnode_replicate_num = 30;
	static const int vmd_account_maxlen = 64;
	static const int vmd_procid_maxlen = 32;
	static const int vmd_max_replicate_host = 32;
	typedef char login_id[vmd_account_maxlen];
	typedef char proc_id[vmd_procid_maxlen];
};
typedef vmprotocol::rpctype rpctype;

template <class S, class IDG, class SNDR = S>
class vmprotocol_impl : public vmprotocol {
public:
	typedef vmprotocol super;
	typedef typename IDG::UUID UUID;
protected:
	S &m_this;
public: /* ctor/dtor/(g|s)etter */
	vmprotocol_impl(S *s) : m_this(*s) {}
	S &_this() { return m_this; }
	const S &_this() const { return m_this; }
	static bool is_valid_id(UUID &uuid) { return IDG::valid(uuid); }
	static const UUID &new_id() { return IDG::new_id(); }
	static const UUID &invalid_id() { return IDG::invalid_id(); }
public:	/* receiver */
#if defined(__PE)
#undef __PE
#endif
#define __PE()  { 	\
	fprintf(stderr, "proto error at %s(%u)\n", __FILE__, __LINE__);	\
	ASSERT(false); }
	int on_recv(char *p, int l);
	int recv_cmd_rpc(U32 msgid, UUID &uuid, proc_id &pid,
		char *p, int l, rpctype rc) {__PE(); }
	template <class Q> int recv_code_rpc(Q &q, char *p, size_t l, rpctype rc)
		{__PE(); }
	int recv_cmd_new_object(U32 msgid, const char *acc,
		UUID &uuid, char *addr, size_t adrl, char *p, size_t l) {__PE();}
	template <class Q> int recv_code_new_object(Q &q, int r, const char *acc, 
		UUID &uuid, char *p, size_t l) {__PE();}
	int recv_cmd_login(U32 msgid, const char *acc, char *p, size_t l) {__PE();}
	template <class Q> int recv_code_login(Q &q, int r, UUID &uuid, char *p, int l)
		{__PE();}
public: /* sender */
	template <class Q> int send_rpc(SNDR &s, const UUID &uuid, const proc_id &p, 
			char *a, size_t al, rpctype rt, Q **pq);
	int reply_rpc(SNDR &s, U32 msgid, char *p, size_t l, rpctype rt);
	template <class Q> int send_new_object(SNDR &s, const char *acc, 
		U32 rmsgid, const UUID &uuid,
		char *addr, size_t adrl, char *p, size_t l, Q **pq);
	int reply_new_object(SNDR &s, U32 msgid, int r,
			const char *acc, UUID &uuid, char *p, size_t l);
	int send_login(SNDR &s, U32 msgid, const char *acc, char *p, size_t l);
	int reply_login(SNDR &s, U32 msgid, int r, const UUID &uuid, char *p, size_t l);
};

/*-------------------------------------------------------------*/
/* sfc::vm::object_factory_impl				       */
/*-------------------------------------------------------------*/
template <class S, class IDG, class KVS>
class object_factory_impl : public kernel {
public:
	typedef typename IDG::UUID UUID;
	typedef S session_type;
	template <class C> class CP : public vmprotocol_impl<C,IDG,S> {
	public:
		CP(C *c) : vmprotocol_impl<C,IDG,S>(c) {} 
		CP() : vmprotocol_impl<C,IDG,S>(NULL) {}
	};
	typedef connector_factory_impl<S,UUID,CP> connector_factory;
	typedef typename connector_impl<S,UUID,CP>::connector conn;
	typedef typename connector_impl<S,UUID,CP>::failover_chain chain;
public:
	/* G=IDG */
	template <class G>
	class object_impl : public kernel {
	protected:
		UUID	m_uuid;		/* unique ID for each object */
		U32		m_flag;		/* object flag */
		conn	*m_conn;	/* if != NULL, connection to this object
					otherwise this object is local */
	public:
		enum {
			object_flag_local = 0x00000001,
		};
	public:
		object_impl() : m_uuid(), m_flag(0), m_conn(NULL) {}
		~object_impl() {}
		void set_uuid(const UUID &uuid) { m_uuid = uuid; }
		const UUID &uuid() const { return m_uuid; }
		conn *connection() { return m_conn; }
		const conn *connection() const { return m_conn; }
		void set_connection(conn *c) { m_conn = c; }
		void set_flag(U32 f, bool on) {
			if (on) { m_flag |= f; } else { m_flag &= ~(f); } }
		bool local() const { return m_flag & object_flag_local; }
		bool remote() const { return !local(); }
		/* now no save, so every time newly create */
		bool create_new() const { return true; }
	};
	typedef object_impl<IDG> object;
protected:
	static map<object,UUID> m_om;
public:
	object_factory_impl() {}
	~object_factory_impl() {}
	static int init(int max_object);
	static void fin();
	static object *create(const UUID &id, bool local) {
		object *p = m_om.create(id);
		if (p) {
			p->set_uuid(id);
			p->set_flag(object::object_flag_local, local);
		}
		return p;
	}
	static object *find(const UUID &id) { return m_om.find(id); }
	static void destroy(object *o) { m_om.erase(o->uuid()); }
	static UUID new_id() { return IDG::new_id(); }
};


/*-------------------------------------------------------------*/
/* sfc::vm::vm_impl										       */
/*-------------------------------------------------------------*/
template <class S, template <class SR, class OF> class L,
			class KVS, class SR, class IDG,
			template <class S, class IDG, class KVS> class OF>
class vm_impl : public vmprotocol_impl<S,IDG> {
public:
	typedef L<SR,OF<S,IDG,KVS> > script;
	typedef vmprotocol_impl<S,IDG> super;
	typedef vmprotocol_impl<S,IDG> protocol;
	typedef OF<S,IDG,KVS> object_factory;
	typedef typename object_factory::object object;
	typedef typename protocol::UUID UUID;
protected:
	static object_factory m_of;
	SR m_sr;
public:
	vm_impl(S *s) : protocol(s) {}
	static int init_vm(int max_object, int max_rpc, int max_rpc_ongoing);
	static void fin_vm();
	static object_factory &of() { return m_of; }
	static bool load(const char *srcfile) { return script::load(srcfile); }
public:
	SR &sr() { return m_sr; }
	const SR & sr() const { return m_sr; }
};
} //sfc::vm
} //sfc


/*-------------------------------------------------------------*/
/* sfc::vm::lang                                               */
/*-------------------------------------------------------------*/
#include "lua.hpp"


/*-------------------------------------------------------------*/
/* sfc::vm::vmnode										       */
/*-------------------------------------------------------------*/
namespace sfc {
using namespace cluster;
namespace vm {
using namespace lang;
template <class S>
class vmnode : public session,
	/* here is a default setting of VM! */
	public vm_impl<S,lua,tc,mp,mac_idgen,object_factory_impl> {
public:
	typedef vm_impl<S,lua,tc,mp,mac_idgen,object_factory_impl> vmmodule;
	typedef typename vmmodule::object_factory object_factory;
	typedef typename vmmodule::object object;
	typedef typename vmmodule::protocol protocol;
	typedef typename vmmodule::UUID UUID;
	typedef typename vmmodule::script script;
	typedef typename script::VM VM;
	typedef typename script::proc_id proc_id;
	typedef grid_servant_factory_impl<S,UUID,object_factory::template CP> 
		svnt_base_factory;
	typedef connector_factory_impl<S,UUID,object_factory::template CP>
		clnt_base_factory;
	typedef factory_impl<S>
		mstr_base_factory;
public:
	class dummy_backend : public protocol {
		dummy_backend(S *s) : protocol(s) {}
	};
	dummy_backend *backend_conn() { ASSERT(false); return (dummy_backend *)NULL; }
	class querydata : public node::querydata {
	public:
		union {
			VM m_vm;
			long m_data;
		};
	public:
		VM &vm() { return m_vm; }
		S *sender() { return (S *)node::querydata::s; }
	};
	vmnode(S *s) : session(), vmmodule(s) {}
	static const UUID &backend_key() { return protocol::invalid_id(); }
	const UUID &verify_uuid(const UUID &uuid) const { return uuid; }
	querydata *senddata(S &s, U32 msgid, char *p, int l);
	int on_recv(char *p, int l) { return protocol::on_recv(p, l); }
	int recv_cmd_rpc(U32 msgid, UUID &uuid, proc_id &pid,
			char *p, int l, rpctype rc);
	int recv_code_rpc(querydata &q, char *p, size_t l, rpctype rc);
};
template <class S> static inline typename S::factory *sf(S &s) {
	return (typename S::factory *)s.f();
}

#include "core.h"
}	//sfc::vm
}	//sfc


#endif//__VM_HPP__

