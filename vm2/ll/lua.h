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
public:
	struct method {
		const char *m_name;
		void *p;
		const char *name() const { return m_name; }
	};
	class coroutine {
	friend class lua;
	protected:
		VM m_exec;
		MSGID m_msgid;
		class lua *m_scr;
		world_id m_wid;
	public:
		coroutine() : m_msgid(0), m_scr(NULL), m_wid(NULL) {}
		TEST_VIRTUAL ~coroutine() {}
		int init(class lua *scr, world_id wid);
		int call(rpc::ll_exec_request &req);
		int call(rpc::create_object_request &req);
		int resume(rpc::ll_exec_response &res);
		static int pack_object(serializer &sr, class object &o);
	protected:
		TEST_VIRTUAL int respond(bool err, serializer &sr);
		int dispatch(int argc);
		int to_stack(rpc::ll_exec_request &req);
		int to_stack(rpc::ll_exec_response &res); 
		int to_stack(rpc::create_object_request &res);
		int to_stack(const rpc::data &d);
		int to_func(const rpc::data &d);
		int from_stack(int start_id, serializer &sr);
		int from_stack(serializer &sr, int stkid);
		int from_func(serializer &sr);
		int push_object(class object *o);
		class object *to_o(int stkid);
		method *to_m(int stkid);
		static class coroutine *to_co(VM vm);
		class lua &scr() { return *m_scr; }
		world_id wid() const { return m_wid; }
	protected:/* metamethods */
		static int object_index(VM vm);
		static int object_newindex(VM vm);
		static int method_call(VM vm);
		static int ctor_call(VM vm);
	};
	struct userdata {
		enum { method, object };
		U8 type, padd[3];
		union {
			struct method m;
			class object *o;
		};
	};
protected:
	VM m_vm;
	serializer &m_sr;
	msgid_generator &m_seed;
	class object_factory &m_of;/* weakref */
	class world_factory &m_wf;/* weakref2 */
	array<coroutine> m_copool;
	array<char[smblock_size]> m_smpool;
public:
	lua(class object_factory &of, class world_factory &wf,
		serializer &sr, msgid_generator &seed) :
		m_vm(NULL), m_sr(sr), m_seed(seed), m_of(of), m_wf(wf), m_smpool() {}
	~lua() { fin(); }
	int init(int max_rpc_ongoing);
	void fin();
	int init_world(world_id wid, world_id from, const char *srcfile);
	class object_factory &of() { return m_of; }
	class world_factory &wf() { return m_wf; }
	operator serializer &() { return m_sr; }
	operator VM() { return m_vm; }
	coroutine *co_new() { return m_copool.create(); }
protected:
	array<char[smblock_size]> &smpool() { return m_smpool; }
	MSGID new_msgid() { return m_seed.new_id(); }
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
	static int get_object_type(VM vm) { return 0; }
	/* helper */
	static int copy_table(VM vm, int from, int to, int type);
	static method *method_new(VM vm, lua_CFunction fn, const char *name);
	static void dump_table(VM vm, int index);
};
}

#endif
