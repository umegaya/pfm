#if !defined(__LUAVM_H__)
#define __LUAVM_H__

#include "common.h"
#include <memory.h>
#include "proto.h"

extern "C" {
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
/* lua coco : TODO use luajit2 or lua 5.2.0*/
extern lua_State *lua_newcthread(lua_State *L, int cstacksize);
}

namespace pfm {
using namespace sfc::util;
class lua {
public:
	typedef lua_State *VM;
	typedef lua_Number num;
	static const U32 smblock_size = 64;
	static const char ctor_string[];
	static const char klass_method_table[];
	static const char klass_name_key[];
	static const char world_klass_name[]; /* World */
	static const U32 world_klass_name_len = 5;
	static const char player_klass_name[];/* Player */
	static const U32 player_klass_name_len = 6;
	static const char world_object_name[];
	static const U32 enter_world_proc_name_len = 5;/* enter */
	static const char enter_world_proc_name[];
	static const U32 login_proc_name_len = 5;/* login */
	static const char login_proc_name[];
	static const U32 convention_processor_name_len = 6; /* lambda */
	static const char convention_processor[];
	static const int max_sys_convention = 10;
public:
	struct method {
	public:
		enum callattr {
			ca_none = 0,
			ca_protected = 1 << 0,
			ca_notification = 1 << 1,
			ca_clientcall = 1 << 2,
		};
	public:
		const char *m_name;
		void *p;
		const char *name() const { return m_name; }
		const char *parse(bool rcv, U32 &attr) {
			return parse(name(), rcv, attr); }
		static const char *parse(const char *name, bool rcv, U32 &attr);
	};
	struct remote_object {
		UUID m_uuid;
		const char *m_klass;
	};
	class coroutine {
	friend class lua;
	protected:
		VM m_exec;
		class fiber *m_fb;
		class lua *m_scr;
	public:
		coroutine() : m_fb(NULL), m_scr(NULL) {}
		~coroutine() {}
		int init(class fiber *f, class lua *scr);
		void fin();
		int call(rpc::ll_exec_request &req, bool trusted);
		int call(rpc::create_object_request &req);
		int call(rpc::authentication_request &req);
		int call(rpc::replicate_request &req);
		int call(rpc::ll_exec_request &req, const char *method);
		int resume(rpc::ll_exec_response &res, bool packobj = false);
#if !defined(_OLD_OBJECT)
		int push_world_object(const UUID &uuid);
		static int pack_object(serializer &sr,
			const UUID &uuid, const char *klass,
			bool packobj = false);
#else
		int push_world_object(class object &o);
		static int pack_object(serializer &sr, class object &o,
			bool packobj = false);
#endif
		int to_stack(rpc::ll_exec_response &res);
		int save_object(class object &o, char *p, int l);
		int load_object(class object &o, const char *p, int l);
	protected:
		int respond(bool err, serializer &sr);
		int dispatch(int argc, bool packobj);
		int to_stack(rpc::ll_exec_request &req, bool trusted);
		int to_stack(rpc::replicate_request &res);
		int to_stack(rpc::ll_exec_request &req, const char *method);
		int to_stack(rpc::create_object_request &res);
		int to_stack(rpc::authentication_request &req);
		int to_stack(const rpc::data &d);
		int to_func(const rpc::data &d);
		int from_stack(int start_id, serializer &sr, bool packobj = false);
		int from_stack(serializer &sr, int stkid, bool packobj = false);
		int from_func(serializer &sr);
		bool callable(int stkid);
#if !defined(_OLD_OBJECT)
		int push_object(const rpc::data *d);
#else
		int push_object(class object *o, const rpc::data *d = NULL, bool ok_nil = false);
#endif
		class object *to_o(int stkid);
		remote_object *to_ro(int stkid);
		method *to_m(int stkid);
		static class coroutine *to_co(VM vm);
		class lua &scr() { return *m_scr; }
		class fiber &fb() { return *m_fb; }
		VM vm() { return m_exec; }
	protected:/* metamethods */
		static int object_index(VM vm);
		static int object_newindex(VM vm);
		static int method_call(VM vm);
		static int ctor_call(VM vm);
	};
	struct watcher {
		coroutine *m_co;
	};
	struct userdata {
		enum { method, watcher, remote_object, object };
		U8 type, padd[3];
		union {
			struct method m;
			struct watcher w;
			class object *o;
		};
	};
protected:
	VM m_vm;
	THREAD m_thrd;
	serializer *m_sr;
	class object_factory &m_of;/* weakref */
	class world_factory &m_wf;/* weakref2 */
	array<coroutine> m_copool;
	array<char[smblock_size]> m_smpool;
public:
	lua(class object_factory &of, class world_factory &wf) :
		m_vm(NULL), m_thrd(NULL), m_sr(NULL), m_of(of), m_wf(wf), m_smpool() {}
	~lua() { fin(); }
	int init(int max_rpc_ongoing, THREAD th);
	void fin();
	int init_world(world_id wid, world_id from, const char *srcfile);
	void fin_world(world_id wid);
	class object_factory &of() { return m_of; }
	class world_factory &wf() { return m_wf; }
	operator VM() { return m_vm; }
	THREAD attached_thrd() const { return m_thrd; }
	coroutine *co_new() { return m_copool.create(); }
	void co_destroy(coroutine *co) { m_copool.destroy(co); }
	bool has_convention_processor() const { return false; }
	void check_sr() { ASSERT(m_sr); }
protected:
	serializer &sr() { return *m_sr; }
	array<char[smblock_size]> &smpool() { return m_smpool; }
	int load_module(world_id wid, const char *srcfile);
	/* metamethod */
	static int class_index(VM vm);
	static int class_newindex(VM vm);
	static int generic_gc(VM vm);
	/* lua hook */
	static int panic(VM vm);
	static void *allocator(void *ud, void *ptr, size_t os, size_t ns);
	/* API */
	static int add_class(VM vm);
	static int get_object_type(VM vm);
	/* helper */
	static int copy_table(VM vm, int from, int to, int type);
	static method *method_new(VM vm, lua_CFunction fn, const char *name);
	static remote_object *remote_object_new(coroutine *co,
			const UUID &uuid, const char *klass, bool try_load);
	static void dump_table(VM vm, int index);
};
}

#endif
