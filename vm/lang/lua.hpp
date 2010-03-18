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
	typedef vmprotocol::rpctype rpctype;
	typedef lua_State *VM;

	/* macro prepair pack */
#define PREPARE_PACK(conn, len)  				\
	size_t __len = len;							\
	char __buf[pack_use_heap_threshold], 		\
	*__p = (len > pack_use_heap_threshold ? 	\
		(char *)allocator(NULL, NULL, 0, __len):\
		__buf);									\
	conn->sr().start(__p, __len);
#define FINISH_PACK(conn)					\
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
	protected:
		VM m_ip;
		S *m_s;
		U32 m_msgid, m_opt;
	public:
		/* NOTE : m_ip never destroyed after once created,  
		memory region which is used for allocating fiber object is
		assure to be 0 cleared. because this memory is allocated by
		nbr_array_create... */
		fiber() {}
		~fiber() {}
		operator VM () { return m_ip; }
		U32 rmsgid() const { return m_msgid; }
		S *connection() { return m_s; }
		void set_owner(VM vm, VM th, S *s, U32 msgid) {
			m_msgid = msgid;
			m_s = s;
			char k[256];
			snprintf(k, sizeof(k), "fb%08x", (U32)th);
			lua_pushlightuserdata(vm, this);
			lua_setfield(vm, LUA_REGISTRYINDEX, k);
		}
		static fiber *get_owner(VM vm, VM th) {
			char k[256];
			snprintf(k, sizeof(k), "fb%08x", (U32)th);
			lua_getfield(vm, LUA_REGISTRYINDEX, k);
			fiber *fb = lua_islightuserdata(vm, -1) ?
				(fiber *)lua_touserdata(vm, -1) : NULL;
			lua_pop(vm, 1);
			return fb;
		}
		bool init_from_vm(lua_State *vm, S *s, U32 msgid, U32 opt) {
			if (!m_ip) { m_ip = lua_newcthread(vm, 1/* use min size */); }
			if (m_ip) { set_owner(vm, m_ip, s, msgid); }
			m_opt = opt;
			return m_ip != NULL;
		}
		bool need_reply() const { return m_opt&vmprotocol::rpcopt_flag_invoked; }
		bool no_yield() const { return m_opt&vmprotocol::rpcopt_flag_notification; }
	};
	class type_id {
	protected:
		vmprotocol::proc_id m_type;
		volatile U32 m_has_table;
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
protected:	/* static variable */
	static map<type_id, char*> m_types;
	static array<rpc>	m_rpcs;
	static array<fiber>	m_fibers;
	static VM 			m_vm;
	static RWLOCK		m_lock;
public:
	lua() {}
	~lua() {}
	/* external interfaces */
	static int 	init(int max_rpc_entry, int max_rpc_ongoing);
	static object 	*object_new(CF &cf, const world_id &wid,
			VM vm, UUID &uuid, SR *sr, bool local);
	static int	pack_object(SR &, const object &, bool);
	static int	call_proc(S &, CF &, const world_id &,
			U32, object &, proc_id &, char *, size_t, rpctype, U32);
	static int	resume_proc(S &, CF &, const world_id &, VM,
			char *, size_t, rpctype);
	static int 	resume_create(S &, CF &, const world_id &, VM, UUID &, SR &);
	static bool load(const char *srcfile);
protected: 	/* lua methods */
	static int	create(VM);
	static int 	index(VM);
	static int 	newindex(VM);
	static int 	call(VM);
	static int 	gc(VM);
	static int	panic(VM);
	static int 	set_object_type(VM);
	static int 	get_object_type(VM);
	static void	*allocator(void *ud, void *ptr, size_t os, size_t ns);
protected:	/* rpc hook */
	static int rpc_sender_hook(const proc_id &p,
				fiber &fb, object &o, SR &, rpctype rt);
	static int rpc_recver_hook(S &c, U32 rmsgid, const proc_id &p,
			fiber &fb, object &o, char *p, size_t l, rpctype rt);
protected: 	/* helpers */
	static void push_object(VM, object *);
	static rpc 	*rpc_new(VM, object *, const char *);
	static fiber	*fiber_new(S *, U32, U32);

	static int 	get_object_value(VM vm, const object &o, const char *key);
	static int 	set_object_value(VM vm, const object &o, const char *key, int from);
	static int 	pack_object_value(VM vm, const object &o, SR &sr);
	static int 	unpack_object_value(CF &cf, const world_id &wid,
					VM vm, const object &o, data &sr);
	static int 	get_object_method(VM vm, const object &o, const char *key);
	static int 	set_object_method(VM vm, const object &o, const char *key, int from);

	static int	pack_lua_stack(SR &, VM, int);
	static bool	unpack_lua_stack(CF &, const world_id &, SR &, VM);
	static int 	put_to_lua_stack(CF &, const world_id &, VM, data &);

	static int 	unpack_object(CF &, const world_id &, VM, data &, object &);

	static const char *add_type(const char *typestr);

	static int	dispatch(S &, VM, int, bool, rpctype);
	static int	reply_result(S &, VM, int, rpctype);
	static object 	*to_o(VM, int, bool abort = true);
	static rpc 		*to_r(VM, int, bool abort = true);
	static const char *to_k(VM, int);
	static const char *chunk_sr_reader(VM, void *, size_t*);
	static int	chunk_sr_writer(VM, const void *, size_t, void *);
	static fiber	*vm_owner(VM);
};
#include "lua.inc"
}	//lang
}	//vm
}	//sfc
#endif

