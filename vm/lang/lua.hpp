/***************************************************************
 * lua.hpp : lua VM implementation
 * 2009/12/23 iyatomi : create
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
#if !defined(__PFM_LUA_H__)
#define __PFM_LUA_H__

extern "C" {
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
/* lua coco */
extern lua_State *lua_newcthread(lua_State *L, int cstacksize);
}

namespace sfc {
using namespace util;
using namespace grid;
namespace vm {
namespace lang {
using namespace serialize;
typedef enum {        
	OT_INVALID,
	OT_PFM,
	OT_RPC,
	OT_SESSION,
} 	OBJTYPE;
typedef struct {
        OBJTYPE type;
        void *o;
}       USERDATA;
typedef enum {
	CALL_NORMAL,
	CALL_PROTECTED,		/* protected (cannot call from client) */
	CALL_NOTIFICATION,	/* notificatoin rpc */
	CALL_CLIENT,	/* client rpc */
} call_attr;
static inline call_attr call_attribute(
		bool rcv, const char *p, const char *&_p) {
	if (rcv) {
		if (*p == '_') {
			return CALL_PROTECTED;
		}
		if (memcmp(p, "client_", 7) == 0) {
			_p = p + 7;
			return CALL_CLIENT;
		}
	}
	else if (memcmp(p, "notify_", 7) == 0) {
		_p = p + 7;
		return CALL_NOTIFICATION;
	}
	return CALL_NORMAL;
}
/* lua_convtype */
typedef bool lua_Boolean;
class lua_convtype {
public:
	/* mp - lua : implemented in lua.inc */
	static inline lua_Number      	to_i(mp::data &d);
	static inline lua_Boolean      	to_b(mp::data &d);
	static inline const char	*to_s(mp::data &d);
	static inline const char	*to_p(mp::data &d);
	static inline int		to_rawlen(mp::data &d);
	static inline int 		to_alen(mp::data &d);
	static inline int		to_maplen(mp::data &d);
	static inline mp::data		&key(mp::data &d, int i);
	static inline mp::data		&val(mp::data &d, int i);
	static inline mp::data		&elem(mp::data &d, int i);
	static inline int		type(mp::data &d);
};
/* lua */
template <class SR, class OF>
class lua : public lua_convtype {
public:	/* typedefs */
	static const U32 max_symbol_len = 32;
	typedef char proc_id[max_symbol_len];
	typedef typename OF::object object;
	typedef typename OF::world world;
	typedef typename OF::world_id world_id;
	typedef typename SR::data data;
	typedef typename OF::conn conn;
	typedef typename OF::session_type S;
	typedef typename OF::UUID UUID;
	typedef typename OF::connector_factory CF;
	typedef typename OF::vm_message_protocol VMMSG;
	typedef vmprotocol::rpctype rpctype;
	typedef lua_State *VM;

	/* macro prepair pack */
#define PREPARE_PACK(szr, len)  				\
	size_t __len = len;							\
	char __buf[pack_use_heap_threshold], 		\
	*__p = (len > pack_use_heap_threshold ? 	\
		(char *)allocator(NULL, NULL, 0, __len):\
		__buf);									\
	szr.start(__p, __len);
#define FINISH_PACK(szr)					\
	if (__len > pack_use_heap_threshold) {	\
		allocator(NULL, __p, __len, 0);		\
	}

	class rpc {
	protected:
		UUID m_uuid;
		proc_id m_p;
	public:
		rpc() : m_uuid() { m_p[0] = '\0';  }
		void set(object *o, const char *id) { 
			m_uuid = o->uuid(); 
			snprintf(m_p, sizeof(m_p) - 1, "%s", id);
		}
		object *obj() { return OF::find(m_uuid); }
		const proc_id &proc() const { return m_p; }
		UUID uuid() const { return m_uuid; }
	};
	class fiber {
	public:
		enum io_type {
			io_type_socket, /* rpc over network */
			io_type_channel,/* rpc over memory (inter thread) */
		};
		template <class V> struct exit_fn {
                typedef void (*type)(V &, V &, VM, int, U32, rpctype, char *, size_t);
                };
	protected:
		VM m_ip;
		struct {
			S *s;
			VMMSG *vmm;
		} m_sender;
		union {
			typename exit_fn<S>::type s;
			typename exit_fn<VMMSG>::type vmm;
		} m_exit_fn;
		U8 m_type, m_rpc_type;
		U16 padd;
		U32 m_msgid;
		const world_id *m_wid;
		lua<SR,OF> *m_scp;
	public:
		/* NOTE : m_ip never destroyed after once created,  
		memory region which is used for allocating fiber object is
		assure to be 0 cleared. because this memory is allocated by
		nbr_array_create... */
		fiber() {}
		~fiber() {}
		operator VM () { return m_ip; }
		static U64 fbkey_from(VM vm) { return (U64)vm; }
		U64 fbkey() const { return fbkey_from(m_ip); }
		U32 rmsgid() const { return m_msgid; }
		S *connection() { return (S *)m_sender.s; }
		VMMSG &vmmsg() { return *m_sender.vmm; }
		io_type iot() const { return (io_type)m_type; }
		rpctype rt() const { return (rpctype)m_rpc_type; }
		const world_id &wid() const { return *m_wid; }
		lua<SR,OF> *scp() { return m_scp; }
		/* only second sender is current thread domain */
		void call_exit(int r, S &sk, rpctype rt, char *p, size_t l) {
			m_exit_fn.s(*connection(), sk, m_ip, r, m_msgid, rt, p, l); }
		void call_exit(int r, VMMSG &v, rpctype rt, char *p, size_t l) {
			m_exit_fn.vmm(vmmsg(), v, m_ip, r, m_msgid, rt, p, l); }
		void set_sender(VMMSG &v, typename exit_fn<VMMSG>::type f); 
		void set_sender(S &s, typename exit_fn<S>::type f) { 
			m_type = (U8)io_type_socket; m_sender.s = &s; 
			m_exit_fn.s = f; }
		template <class V>
		bool init_from_vm(class lua<SR,OF> *scp, const world_id *wid,
			rpctype rt,
			V &v, U32 msgid, typename exit_fn<V>::type fn) {
			ASSERT(wid);
			bool first = false;
			if (!m_ip) {
				m_ip = lua_newcthread(scp->vm(), 1/* use min size */);
				if (!m_ip) { return false; }
				first = true;
			}
			ASSERT(((int **)m_ip)[-1]);
			lua_pushthread(m_ip);
			lua_getfield(m_ip, LUA_REGISTRYINDEX, *wid);
			if (!lua_istable(m_ip, -1)) {
				return false;
			}
			lua_setfenv(m_ip, -2);
			if (first) {
				/* add this thread to registroy index so that 
				thread is never garbage collected.*/
				lua_pushinteger(m_ip, (U32)m_ip);
				lua_pushvalue(m_ip, -2);
				lua_settable(m_ip, LUA_REGISTRYINDEX);
			}
			lua_settop(m_ip, 0);
			m_scp = scp;
			m_msgid = msgid;
			m_wid = wid;
			m_rpc_type = (U8)rt;
			set_sender(v, fn);
			return m_ip != NULL;
		}
		/* because it union, s == NULL means vmm == NULL */
		bool need_reply() const { return m_exit_fn.s == NULL; }
		static void exit_noop(S &, S &, VM, int, U32, rpctype, char *, size_t) {}
	};
	class type_id {
	protected:
		vmprotocol::proc_id m_type;
		U32 m_has_table;
	public:
		type_id() : m_has_table(0) { set_type(""); }
		void set_type(const char *p) {
			strncpy(m_type, p, sizeof(vmprotocol::proc_id));
		}
		bool has_table() const { return m_has_table != 0; }
		void set_has_table() { m_has_table = 1; }
		operator const char *() { return (const char *)m_type; }
	};
	struct write_func_chunk {
		SR *sr;
		int n_write;
	};
	struct read_func_chunk {
		data *d;
		int ret;
	};
public: /* constant */        
	static const U32 max_rpc_packlen = 4 * 1024;   /* max 4kb */
	static const U32 pack_use_heap_threshold = 4 * 1024;
	enum {
		lua_rpc_nobiton = 0,
		lua_rpc_entry = 0x1, 	/* first rpc (start point?) */
		lua_rpc_reply = 0x2,	/* reply is needed? */
	};
#if defined(_DEBUG)
	struct thent {
		THREAD th;
	};
#endif
protected:	/* static variable */
	static map<type_id, char*> 	m_types;
	static array<rpc>		m_rpcs;
	static map<fiber*,U64>		m_fbmap;
#if defined(_DEBUG)
	static map<thent,U64>	m_mmap;
	static RWLOCK m_mlk;
#endif
	array<fiber> 	m_fibers;
	VM 		m_vm;
	THREAD		m_thrd;
	SR		m_serializer;
public:
	lua() : m_fibers(), m_vm(NULL), m_thrd(NULL), m_serializer() {}
	~lua() {}
	/* external interfaces */
	int 	init(int max_rpc_entry, int max_rpc_ongoing, int n_wkr);
	void 	fin();
	int  	init_world(const world_id &wid, const world_id &from, const char *srcfile);
	int	add_global_object(CF &, const world_id &wid, char *p, size_t l);
	void 	set_thread(THREAD th) { m_thrd = th; }
	THREAD	thread() const { return m_thrd; }
	VM	vm() const { return m_vm; }
	SR	&serializer() { return m_serializer; }
	array<fiber> &fibers() { return m_fibers; }
#if defined(_DEBUG)
	static map<thent,U64> &mmap() { return m_mmap; }
#endif
	bool check_fiber(fiber &f) {
		int idx = nbr_array_get_index(m_fibers.get_a(), &f);
		//TRACE("check_fiber: %p %d %d %p %p\n", thread(), idx, 
		//	nbr_array_max(m_fibers.get_a()), m_fibers.get_a(), &f); 
		return (nbr_array_max(m_fibers.get_a()) > idx && idx >= 0); 
	}
	static object 	*object_new(CF &cf, const world_id *w,
			VM vm, lua<SR,OF> *scp, UUID &uuid, SR *sr, bool local);
	int	call_proc(S &, CF &, U32, object &, proc_id &, char *, size_t, rpctype,
			typename fiber::template exit_fn<S>::type);
	template <class SNDR>
	int 	resume_create(SNDR &, CF &, int, const world_id &, fiber &, UUID &, SR &);
	bool 	load(const world_id &, const char *srcfile);
	template <class SNDR>
	int	resume_proc(SNDR &, CF &, const world_id &, fiber &,
			char *, size_t, rpctype);
	int 	local_call(VMMSG &c, CF &cf, U32 rmsgid,
			object &o, proc_id &pid, char *p, size_t l, rpctype rt);
public:
	static int	pack_object(VM, SR &, const object &, bool);
	static object 	*unpack_object(SR &);
protected: 	/* lua methods */
	static int	create(VM);
	static int 	index(VM);
	static int 	newindex(VM);
	static int  	global_newindex(VM);
	static int 	call(VM);
	static int 	gc(VM);
	static int	panic(VM);
	static int 	set_object_type(VM);
	static int 	get_object_type(VM);
	static void	*allocator(void *ud, void *ptr, size_t os, size_t ns);
	static const char 	*chunk_sr_reader(VM, void *, size_t*);
	static int	chunk_sr_writer(VM, const void *, size_t, void *);
#if defined(_DEBUG)
	static int 	gc_fiber(VM);
	static void	dump_table(VM, int);
	static void	dump_stack(VM);
#else
	#define dump_stack(VM)
#endif
protected:	/* rpc hook */
	template <class SNDR> static int
			rpc_sender_hook(SNDR &, const proc_id &p,
					fiber &fb, object &o, SR &, rpctype rt);
	static int 	rpc_recver_hook(S &c, U32 rmsgid, const proc_id &p,
					fiber &fb, object &o, char *p, size_t l, rpctype rt);
protected: 	/* helpers */
	static rpc 	*rpc_new(VM, object *, const char *);
	template <class V> fiber* fiber_new(V &, rpctype, U32, const world_id *, 
			typename fiber::template exit_fn<V>::type fn);
	static void 	fiber_delete(fiber *fb);
	template <class SNDR>
	static void 	fiber_exit_call(fiber &fb, int basestk, int r, SNDR &s, 
				rpctype rt = vmprotocol::rpct_invalid);

	static void	push_object(VM, object *);
	static int 	get_object_value(VM vm, const object &o, const char *key);
	static int 	set_object_value(VM vm, const object &o, const char *key, int from);
	static int 	get_object_method(VM vm, const object &o, const char *key);
	static int 	set_object_method(VM vm, const object &o, const char *key, int from);
	static int 	pack_object_value(VM vm, const object &o, SR &sr);
	static int	pack_lua_stack(SR &, VM, int);
	static int 	unpack_object_value(CF &cf, const world_id &wid,
				VM vm, lua<SR,OF> *scp, const object &o, data &sr);
	static bool	unpack_lua_stack(CF &cf, const world_id &w, SR &sr, fiber &fb) {
		return unpack_lua_stack(cf, w, sr, fb, fb.scp());
	}
	static bool	unpack_lua_stack(CF &, const world_id &, SR &, VM, lua<SR,OF> *scp);
	static int 	put_to_lua_stack(CF &, const world_id &, VM, lua<SR,OF> *scp, data &);
	static int 	unpack_object(CF &, const world_id &, VM, lua<SR,OF> *, data &, object &);

	static int	copy_table(VM, int index_from, int index_to, int type);

	static const char *add_type(VM, const world_id *, const char *typestr);

	template <class SNDR> static int dispatch(SNDR &, fiber &, int, bool);
	static int 	reply_result(fiber &, int, rpctype rt = vmprotocol::rpct_invalid);
	static object 	*to_o(VM, int, bool abort = true);
	static rpc 	*to_r(VM, int, bool abort = true);
	static const char 	*to_k(VM, int);
	static fiber 	*vm_owner(VM);
};
#include "lua.inc"
}	//lang
}	//vm
}	//sfc
#endif

