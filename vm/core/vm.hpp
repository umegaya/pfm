#if !defined(__VM_HPP__)
#define __VM_HPP__

#include "sfc.hpp"
#include "grid.h"
#include <stdarg.h>

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
	static inline bool valid(const UUID &uuid) {
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

		vmcmd_rpc,		/* c->s, c->c, s->s, vmmsg */
		vmcmd_new_object,	/* s->m, m->s */
		vmcmd_login,	/* c->s->m */
		vmcmd_node_ctrl,	/* m->s */
		vmcmd_node_register,	/* s->m */
		vmcode_rpc,		/* s->c, c->c, s->s, vmmsg */
		vmcode_new_object,	/* s->m, m->s */
		vmcode_login,	/* m->s->c */
		vmcode_node_ctrl,	/* m->s */
		vmcode_node_register,	/* s->m */
		vmnotify_node_change,	/* m->s */
		vmnotify_init_world,		/* vmmsg */
		vmnotify_add_global_object,	/* vmmsg */
		vmnotify_load_module,		/* vmmsg */
	};
	typedef enum {
		rpct_invalid,

		rpct_method,	/* r = object.func(self,a1,a2,...) */
		rpct_global,	/* r = global_func(a1,a2,...) */
		rpct_getter,	/* object[k] */
		rpct_setter,	/* object[k] = v */
		rpct_call,		/* local method call */
		rpct_call_global,	/* local global func call */
		rpct_method_fw,	/* method call forwarded */
		rpct_global_fw,	/* global call forwarded */

		rpct_error = 255,/* error happen on remote */
	} rpctype;
	typedef enum {
		load_purpose_invalid,
		load_purpose_login,
		load_purpose_create,
		load_purpose_create_world,
	} loadpurpose;
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
	static const int vmd_max_worldname = 16;
	static const int vmd_max_node_ctrl_cmd = 256;
	static const int vmd_object_multiplexity = 3;
	typedef char login_id[vmd_account_maxlen];
	typedef char proc_id[vmd_procid_maxlen];
	typedef char world_id[vmd_max_worldname];
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
	vmprotocol_impl &operator = (vmprotocol_impl &v) {
		/* no need copy this pointer (because it will initialize with ctor, 
		and always with this object. */
		ASSERT(&m_this);
		return *this;
	}
	S &_this() { return m_this; }
	const S &_this() const { return m_this; }
	static bool is_valid_id(const UUID &uuid) { return IDG::valid(uuid); }
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
	int recv_cmd_new_object(U32 msgid, const world_id &wid,
			UUID &uuid, char *p, size_t l) {__PE();}
	template <class Q> int recv_code_new_object(Q &q, int r,
			UUID &uuid, char *p, size_t l) {__PE();}
	int recv_cmd_login(U32 msgid, const world_id &wid, const char *acc,
			char *p, size_t l) {__PE();}
	template <class Q> int recv_code_login(Q &q, int r, const world_id &wid, 
			UUID &uuid, char *p, size_t l) {__PE();}
	int recv_cmd_node_ctrl(int r, const char *cmd,
			const world_id &wid, char *p, size_t l) {__PE();}
	template <class Q> int recv_code_node_ctrl(Q &q, int r, const char *cmd,
			const world_id &wid, char *p, size_t l) {__PE();}
	int recv_cmd_node_register(U32 msgid, const address &a) {__PE();}
	template <class Q> int recv_code_node_register(Q &q, int r) {__PE();}
	int recv_notify_node_change(const char *cmd,
			const world_id &wid, const address &a) {__PE();}
	int recv_notify_init_world(const world_id &nw, const world_id &from,
			const char *file) {__PE();}
	int recv_notify_add_global_object(const world_id &nw, char *p, size_t pl)
			{__PE();}
	int recv_notify_load_module(const world_id &nw, const char *file) {__PE();}
public: /* sender */
	template <class Q> int send_rpc(SNDR &s, const UUID &uuid, const proc_id &p, 
			char *a, size_t al, rpctype rt, Q **pq);
	int reply_rpc(SNDR &s, U32 msgid, char *p, size_t l, rpctype rt);
	template <class Q> int send_new_object(SNDR &s, U32 rmsgid,
			const world_id &wid, const UUID &uuid,
			char *p, size_t l, Q **pq);
	int reply_new_object(SNDR &s, U32 msgid, int r, const UUID &uuid,
			char *p, size_t l);
	int send_login(SNDR &s, U32 msgid, const world_id &wid,
			const char *acc, char *p, size_t l);
	int reply_login(SNDR &s, U32 msgid, int r, const world_id &wid,
			const UUID &uuid, char *p, size_t l);
	int send_node_ctrl(SNDR &s, U32 msgid, const char *cmd,
			const world_id &wid, char *p, size_t l);
	int reply_node_ctrl(SNDR &s, U32 msgid, int r, const char *cmd,
			const world_id &wid, char *p, size_t l);
	int send_node_register(SNDR &s, U32 msgid, const address &node_addr);
	int reply_node_register(SNDR &s, U32 msgid, int r);
	int notify_node_change(SNDR &s, const char *cmd,
			const world_id &wid, const address &a);
	int notify_init_world(SNDR &s, const world_id &nw, const world_id &from,
			const char *file);
	int notify_add_global_object(const world_id &nw, char *p, size_t l);
	int notify_load_module(SNDR &s, const world_id &nw, const char *file);
};

/*-------------------------------------------------------------*/
/* sfc::vm::message_passer								       */
/*-------------------------------------------------------------*/
template <class S>
class message_passer : public kernel {
public:
	typedef typename S::factory factory;
	typedef typename S::querydata querydata;
protected:
	static factory *m_f;
	THREAD m_to;
	THREAD m_from;
	U32 m_type;
public:
	message_passer() : m_to(NULL), m_from(NULL), m_type(0) {}
	message_passer(THREAD from, THREAD to, U32 type) : m_type(type) {
		set_thread_to(to);
		set_thread_from(from);
		ASSERT(m_type == ((U32)S::vmd_session_type));
	}
	static void set_factory(factory *f) { m_f = f; }
	void set_thread_to(THREAD th) { m_to = th; }
	void set_thread_from(THREAD th) { m_from = th; }
	THREAD thread_to() const { return m_to; }
	THREAD thread_from() const { return m_from; }
	U32 get_type() const { return m_type; }
	const char *addr() { return "localhost(VMMSG)"; }
	bool thread_current() const { return nbr_thread_is_current(m_to); }
	U32 type() const { return m_type; }
	int log(loglevel lv, const char *fmt, ...);
	static factory *f() { return m_f; }
	static U32 msgid() { return S::get_msgid(m_f); }
	static U32 stype(char p) { return (U32)(((U8)p) >> 6); }
	static void remove_stype(char *p) { (*p) &= (0x3F); }
	static void add_stype(char *p, U32 type) { (*p) |= ((U8)(type << 6)); }
	querydata *senddata(S &, U32, char *, int);
	int event(char *p, int l) { return send(p, l); }
	int send(char *p, int l) {
		U32 msgid_new = ntohl(GET_32(p + 1));
		if (*p == 6) {
			static U8 g_buf[65536], g_flag = 0;
			if (g_flag == 0) {
				memset(g_buf, 0, sizeof(g_buf));
				g_flag = 1;
			}
			if (msgid_new < 65536) {
				ASSERT(g_buf[msgid_new - 1] != 0xfe);
				g_buf[msgid_new - 1] = 0xfe;
			}
		}
		TRACE("msgpssr: %u[%u:%u]\n", l, *p, msgid_new);
		add_stype(p, m_type);
		//ASSERT(((U8)(*p)) != 0xCC && ((U8)(*p)) != 0x33);
		ASSERT(m_type == ((U32)S::vmd_session_type));
		SWKFROM from = { 0, m_from };
#if defined(_DEBUG)
		int r = m_to ? nbr_sock_worker_event(&from, m_to, p, l) :
				nbr_sock_worker_bcast_event(&from, p, l);
		ASSERT(r > 0);
		return r;
#else
		return m_to ? nbr_sock_worker_event(&from, m_to, p, l) :
				nbr_sock_worker_bcast_event(&from, p, l);
#endif
	}
};

template <class S, template <class S> class CP>
class vm_message_protocol_impl : public message_passer<S>,
	public CP<vm_message_protocol_impl<S,CP> > {
public:
	typedef message_passer<S> super;
	typedef typename S::script script;
	typedef typename S::object_factory object_factory;
	typedef typename object_factory::world world;
	typedef typename object_factory::conn connector;	
	typedef CP<vm_message_protocol_impl<S,CP> > protocol;
	typedef typename protocol::UUID UUID;
	typedef typename protocol::rpctype rpctype;
	typedef typename protocol::proc_id proc_id;
	typedef vmprotocol::world_id world_id;
	typedef typename protocol::loadpurpose loadpurpose;
protected:
	script *m_scp;
public:
	vm_message_protocol_impl(script *scp, THREAD from, THREAD to, 
		U32 type = S::vmd_session_type) :
		super(from, to, type), protocol(this), m_scp(scp) {}
	vm_message_protocol_impl() : super(NULL, NULL, 0), protocol(this), m_scp(NULL) {}
	script *scp() { return m_scp; }
	static void on_event(THREAD from, THREAD to, char *p, size_t l);
	void set_receiver(const vm_message_protocol_impl<S,CP> &p);
	int recv_cmd_rpc(U32 msgid, UUID &uuid, proc_id &pid,
			char *p, int l, rpctype rc);
	int recv_notify_init_world(const world_id &nw, const world_id &from, const char *f);
	int recv_notify_add_global_object(const world_id &nw, char *p, size_t pl);
	int recv_notify_load_module(const world_id &nw, const char *file);
	template <class Q> int recv_code_rpc(Q &q, char *p, size_t l, rpctype rc);
	template <class Q> int recv_code_new_object(Q &q, int r, UUID &uuid, char *p, size_t l);
	template <class Q> int load_or_create_object(U32 msgid, const world_id *id,
			UUID &uuid, char *p, size_t l, loadpurpose lp, Q **pq);
};


/*-------------------------------------------------------------*/
/* sfc::vm::object_factory_impl				       */
/*-------------------------------------------------------------*/
template <class S, class IDG, class DBM, class CF, class VMMSG>
class object_factory_impl : public kernel {
public:
	typedef typename IDG::UUID UUID;
	typedef S session_type;
	typedef vmprotocol::world_id world_id;
	typedef CF connector_factory;
	typedef VMMSG vm_message_protocol;
	typedef typename CF::session_type conn;
public:
	template <class C>
	class world_impl  {
	protected:
		CONHASH m_ch;
		world_id m_wid;
		UUID m_world_object;
		map<const C*, address> m_nodes;
	public:
		typedef typename map<const C*, address>::iterator nditer;
		world_impl() : m_ch(NULL), m_world_object() {}
		~world_impl() { fin(); }
		int init(int max_node, int max_replica);
		void fin() { nbr_conhash_fin(m_ch); }
		void set_id(const world_id &wid) { memcpy(m_wid, wid, sizeof(m_wid)); }
		const world_id &id() { return m_wid; }
		const UUID &world_object_uuid() const { return m_world_object; }
		void set_world_object_uuid(const UUID &uuid) { m_world_object = uuid; }
		int lookup_node(const UUID &uuid, const CHNODE *n[], int n_max) {
			*n = nbr_conhash_lookup(m_ch, (const char *)&uuid, sizeof(uuid));
			return (*n) ? 1 : NBR_ENOTFOUND;
		}
		int add_node(const C &s);
		int del_node(const C &s);
		map<const C*, address> &nodes() { return m_nodes; }
		static void set_node(C &s, const char *addr) {
			nbr_conhash_set_node(s.chnode(), addr, vmprotocol::vnode_replicate_num);
		}
		static void remove_node(C &s) {
			typename map<world, world_id>::iterator it = m_wl.begin();
			for (; it != m_wl.end(); it = m_wl.next(it)) {
				it->del_node(s);
			}
		}
		conn *connect_assigned_node(connector_factory &cf, const UUID &uuid);
	protected:
		int add_node(const CHNODE &n) {
			return nbr_conhash_add_node(m_ch, (CHNODE *)&n); }
		int del_node(const CHNODE &n) {
			return nbr_conhash_del_node(m_ch, (CHNODE *)&n); }
	};
	typedef world_impl<S> world;
public:
	/* G=IDG */
	template <class G>
	class object_impl : public kernel {
	protected:
		UUID	m_uuid;		/* unique ID for each object */
		U32		m_flag;		/* object flag */
		conn	*m_conn;	/* remote : rpc replication / local : replication */
		const char *m_type;	/* object type */
		THREAD 	m_thrd;	/* thrd address which assigned to this object */
		const world_id 	*m_wid;	/* WORLD which this object belongs to */
		time_t m_last_save;	/* last save */
	public:
		enum {
			object_flag_local = 0x00000001,
			object_flag_new = 0x00000002,
			object_flag_collected = 0x00000004,
		};
	public:
		object_impl() : m_uuid(), m_flag(0), m_conn(NULL),
			m_type("_generic_"), m_thrd(NULL) {}
		~object_impl() {}
		void set_uuid(const UUID &uuid) { m_uuid = uuid; }
		const UUID &uuid() const { return m_uuid; }
		conn *connection() { return m_conn; }
		const conn *connection() const { return m_conn; }
		THREAD thread() const { return m_thrd; }
		const world_id *wid() { return m_wid; }
		bool thread_current() { return nbr_thread_is_current(m_thrd); }
		void set_connection(conn *c) { m_conn = c; }
		void set_thread(THREAD th) { m_thrd = th; }
		void set_type(const char *type) { m_type = type; }
		void set_wid(const world_id *w) { m_wid = w; }
		void set_flag(U32 f, bool on) {
			if (on) { m_flag |= f; } else { m_flag &= ~(f); } }
		bool local() const { return m_flag & object_flag_local; }
		bool remote() const { return !local(); }
		const char *type() const { return m_type; }
		bool create_new() const { return m_flag & object_flag_new; }
		bool collected() const { return m_flag & object_flag_collected; }
		void saved() { m_last_save = time(NULL); }
		bool save_expire(U32 t) const {
			return (((U32)time(NULL)) > (m_last_save + t)); }
	};
	typedef object_impl<IDG> object;
protected:
	static map<object,UUID> m_om;
	static map<world, world_id> m_wl;	/* world list */
public:
	object_factory_impl() {}
	~object_factory_impl() {}
	static int init(int max_object, int max_world, const char *dbmopt);
	static void fin();
	static object *create(const UUID &id, bool local, void **dp, int *len) {
		ASSERT(m_om.find(id) == NULL);
		object *p = m_om.create(id);
		if (p) {
			p->set_uuid(id);
			p->set_flag(object::object_flag_local, local);
			if (dp) { *dp = DBM::get(&id, sizeof(UUID), *len); }
		}
		return p;
	}
	static world *create_world(const world_id &wid) { return m_wl.create(wid); }
	static object *find(const UUID &id) { return m_om.find(id); }
	static world *find_world(const world_id &wid) { return m_wl.find(wid); }
	static void destroy(object *o) { m_om.erase(o->uuid()); }
	static UUID new_id() { return IDG::new_id(); }
};


/*-------------------------------------------------------------*/
/* sfc::vm::vm_impl										       */
/*-------------------------------------------------------------*/
template <class S, template <class SR, class OF> class L,
		class DBM, class SR, class IDG, class CF, class VMMSG,
		template <class S, class IDG, class DBM, class CF, class VMMSG> class OF>
class vm_impl : public vmprotocol_impl<S,IDG> {
public:
	typedef OF<S,IDG,DBM,CF,VMMSG> object_factory;
	typedef L<SR,object_factory> script;
	typedef vmprotocol_impl<S,IDG> super;
	typedef vmprotocol_impl<S,IDG> protocol;
	typedef SR serializer;
	typedef typename object_factory::object object;
	typedef typename protocol::UUID UUID;
	typedef typename SR::data data;
	typedef DBM dbm;
protected:
	static object_factory m_of;
	static CF *m_cf;
	static THPOOL m_thp;
public:
	vm_impl(S *s) : protocol(s) {}
	static int init_world(int max_object, int max_world, const char *dbmopt);
	static void fin_world();
	static script *init_vm(int max_rpc, int max_rpc_ongoing, int n_wkr);
	static void fin_vm(script *vm);
	static object_factory &of() { return m_of; }
public:
	static CF &cf() { return *m_cf; }
	static THPOOL thp() { return m_thp; }
	static void set_cf(CF *cf) { m_cf = cf; }
};


/*-------------------------------------------------------------*/
/* sfc::vm::vmdconfig									       */
/*-------------------------------------------------------------*/
class vmdconfig : public connector_config {
public:
	char m_lang[16];
	char m_langopt[256];
	char m_dbm[16];
	char m_dbmopt[256];
	char m_root_dir[256];
	char m_be_addr[256];
	U32	m_max_object, m_max_world,
		m_rpc_entry, m_rpc_ongoing;
	U32 m_max_node, m_max_replica;
public:
	vmdconfig() : connector_config(), m_max_object(1000 * 1000), m_max_world(10),
		m_rpc_entry(1000 * 1000), m_rpc_ongoing(1000 * 1000),
		m_max_node(10000), m_max_replica(vmprotocol::vnode_replicate_num) {}
	vmdconfig(BASE_CONFIG_PLIST,
			int max_chain,
			char *lang, char *lopt,
			char *dbm, char *dbmopt,
			char *root_dir, char *be_addr,
			int max_object, int max_world,
			int rpc_entry, int rpc_ongoing,
			int max_node, int max_replica);
	const char *getpath(char *buff, size_t len, const char *file) const {
		snprintf(buff, len, "%s%s", m_root_dir, file);
		return buff;
	}
	virtual int set(const char *k, const char *v);
	virtual config *dup() const {
		vmdconfig *cfg = new vmdconfig;
		*cfg = *this;
		return cfg;
	}
};
} //sfc::vm
} //sfc


/*-------------------------------------------------------------*/
/* sfc::vm::lang (add here for new script language support     */
/*-------------------------------------------------------------*/
#include "lua.hpp"


/*-------------------------------------------------------------*/
/* sfc::vm::vmnode										       */
/*-------------------------------------------------------------*/
namespace sfc {
using namespace cluster;
namespace vm {
using namespace lang;
template <class S, class IDG>
struct vm_gen_factory {
	template <class C> class CP : public vmprotocol_impl<C,IDG,S> {
	public:
		CP(C *c) : vmprotocol_impl<C,IDG,S>(c) {}
		CP() : vmprotocol_impl<C,IDG,S>(NULL) {}
	};
	typedef vm_message_protocol_impl<S,CP> vm_message_protocol;
	typedef connector_factory_impl<S,typename IDG::UUID,CP> connector_factory;
};
template <class S, class CF = typename vm_gen_factory<S,mac_idgen>::connector_factory,
		class VMMSG = typename vm_gen_factory<S,mac_idgen>::vm_message_protocol>
class vmnode : public session,
	/* here is a default setting of VM! */
	public vm_impl<S,lua,tc,mp,mac_idgen,CF,VMMSG,object_factory_impl> {
public:
	typedef vm_impl<S,lua,tc,mp,mac_idgen,CF,VMMSG,object_factory_impl> vmmodule;
	typedef session super;
	typedef typename vmmodule::object_factory object_factory;
	typedef typename vmmodule::object object;
	typedef typename vmmodule::protocol protocol;
	typedef typename vmmodule::UUID UUID;
	typedef typename vmmodule::script script;
	typedef typename vmmodule::loadpurpose loadpurpose;
	typedef typename vmmodule::serializer serializer;
	typedef message_passer<S> message_passer;
	typedef VMMSG vm_msg;
	typedef typename script::VM VM;
	typedef typename script::fiber fiber;
	typedef typename script::proc_id proc_id;
	typedef typename object_factory::conn connector;
	typedef typename object_factory::world world;
	typedef typename object_factory::world_id world_id;
	typedef grid_servant_factory_impl<
		S,UUID,vm_gen_factory<S,mac_idgen>::template CP>
		svnt_base_factory;
//	typedef connector_factory_impl<S,UUID,vmmodule::template CP>
	typedef factory_impl<S,arraypool<S> >
		clnt_base_factory;
	typedef factory_impl<S>
		mstr_base_factory;
protected:
	CHNODE m_node;	/* node information */
	const world_id *m_wid;
	script *m_vm;
#if defined(_DEBUG)
	U32 m_last_msgid;
#endif
public:
	connector *backend_connect(address &a) {
		return vmmodule::cf().backend_connect(a); }
	connector *backend_conn() { return vmmodule::cf().backend_conn(); }
	class querydata {
	public:
		U32 msgid;
		SOCK sk;
		session *s;
		VMMSG m_vmm;
		U8 m_data, padd[3];
		fiber *m_fb;
		world *m_world;
	public:
		querydata() : m_vmm() {}
		void set_from_sock(S *sock) { 
			s = sock; if (s) { sk = s->sk(); } 
		}
		void set_from_vmm(message_passer *pmp) {
			m_vmm.set_receiver(*(VMMSG *)pmp);
		}
		fiber &fb() { return *m_fb; }
		bool valid_fb() const { return m_fb != NULL; }
		bool valid_query() const { 
			return s ? nbr_sock_is_same(s->sk(), sk) : false; }
		world &wld() { return *m_world; }
		S *sender() { return (S *)s; }
		VMMSG &vmmsg() { return m_vmm; }
	};
public:
	vmnode(S *s) : session(), vmmodule(s), m_wid(NULL) {
		memset(&m_node, 0, sizeof(m_node));
	}
	CHNODE *chnode() { return &m_node; }
	const CHNODE *chnode() const { return &m_node; }
	bool thread_current() const { return nbr_sock_worker_is_current(sk()); }
	script *fetch_vm() { ASSERT(false); return NULL; }
	script *vm() { return m_vm; }
	const script *vm() const { return m_vm; }
	int set_wid(const world_id &wid) {
		world *w = object_factory::find_world(wid);
		if (!w) { return NBR_ENOTFOUND; }
		m_wid = &(w->id());
		return NBR_OK;
	}
	U32 msgid() const { return vmmodule::cf().msgid(); }
	const world_id &wid() const { return m_wid ? *m_wid : *(const world_id*)""; }
	bool registered() const { return nbr_conhash_node_registered(&(m_node)); }
	bool trust() const { return true; }
	static const UUID &backend_key() { return protocol::invalid_id(); }
	static void *task_save_object(void *);
	static int save_object(object &o);
	const UUID &verify_uuid(const UUID &uuid) const { return uuid; }
	void fin() {
		if (registered()) { world::remove_node(*((S *)this)); }
		super::fin();
	}
public:
	querydata *senddata(S &s, U32 msgid, char *p, int l);
	world *create_world(VMMSG &vmm, const world_id &wid);
	int on_recv(char *p, int l) { return protocol::on_recv(p, l); }
	int on_event(char *p, int l) { return on_recv(p, l); }
	int on_open(const config &cfg);
#if defined(_DEBUG)
	int send(char *p, int l) {
		static volatile int g_flag[6] = {0, 0, 0, 0, 0, 0};
		m_last_msgid = ntohl(GET_32(p + 1));
		if (S::vmd_session_type == 3 && m_last_msgid < 7) {
			TRACE("g_flag = %u %u %s{%u,%u,%u,%u,%u,%u}\n",
				m_last_msgid, g_flag[m_last_msgid - 1],
				g_flag[m_last_msgid - 1] == 0 ? "true" : "false",
				g_flag[0],g_flag[1],g_flag[2],g_flag[3],g_flag[4],g_flag[5]);
			ASSERT(g_flag[m_last_msgid - 1] == 0);
			g_flag[m_last_msgid - 1] = 1;
		}
		TRACE("vmnode: send(%u:%p)\n", m_last_msgid, this);
		return super::send(p, l);
	}
	U32 last_msgid() const { return m_last_msgid; }
#endif
	int recv_cmd_rpc(U32 msgid, UUID &uuid, proc_id &pid,
			char *p, int l, rpctype rc);
	int recv_code_rpc(querydata &q, char *p, size_t l, rpctype rc);
	template <class Q> int load_or_create_object(U32 msgid,
			const world_id *id, UUID &uuid, char *p, size_t l, loadpurpose lp,
			Q **pq);
	template <class Q> int create_object_with_type(
			const world_id *id,
			const char *type, size_t tlen,
			U32 msgid, loadpurpose lp, Q **pq);
	static void fb_exit_noop(S &, S &, VM, int, U32, rpctype, char *, size_t) {}

};
template <class S> static inline typename S::factory *sf(S &s) {
	return (typename S::factory *)s.f();
}

#include "core.h"
}	//sfc::vm
}	//sfc


#endif//__VM_HPP__

