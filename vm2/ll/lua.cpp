#include "ll.h"
#include "object.h"
#include "world.h"
#include "fiber.h"
#include "serializer.h"

using namespace pfm;
using namespace pfm::rpc;

#define CAN_PROCESS(co, o) (o->can_process_with(ll::to_ll(&(co->scr()))))

/* lua::coroutine */
int lua::coroutine::init(class fiber *fb, class lua *scr) {
	bool first = !m_exec;
	if (first && !(m_exec = lua_newcthread((VM)*scr, 0))) {
		return NBR_EEXPIRE;
	}
	/* Isolation */
	lua_pushthread(m_exec);
	lua_getfield(m_exec, LUA_REGISTRYINDEX, fb->wid());
	if (!lua_istable(m_exec, -1)) { ASSERT(0); return NBR_ENOTFOUND; }
	lua_setfenv(m_exec, -2);
	if (first) {
		/* register thread object to registry so than never gced. */
		lua_pushvalue(m_exec, -1);
		lua_pushlightuserdata(m_exec, this);
		lua_settable(m_exec, LUA_REGISTRYINDEX);
		/* add this pointer to registry so that can find this ptr faster */
		lua_pushlightuserdata(m_exec, this);
		lua_pushvalue(m_exec, -2);
		lua_settable(m_exec, LUA_REGISTRYINDEX);
		ASSERT(to_co(m_exec) == this);
	}
	/* reset stack */
	lua_settop(m_exec, 0);
	m_scr = scr;
	m_fb = fb;
	ASSERT(m_fb);
	return NBR_OK;
}

int lua::coroutine::call(create_object_request &req)
{
	int r;
	if ((r = to_stack(req)) < 0) { 
		if (r == NBR_EALREADY) {
			/* already create: regard as success */
			serializer &sr = *m_scr;
			PREPARE_PACK(sr);
			ll_exec_response::pack_header(sr, m_fb->msgid());
			if ((r = from_stack(2, sr)) < 0) { return r; }
			sr.pushnil();
			return respond(false, sr);
		}
		return r; 
	}
	return dispatch(r);
}

int lua::coroutine::call(ll_exec_request &req, bool trusted)
{
	int r;
	if ((r = to_stack(req, trusted)) < 0) { return r; }
	return dispatch(r);
}

int lua::coroutine::dispatch(int argc)
{
	int r;
	if ((r = lua_resume(m_exec, argc)) == LUA_YIELD) {
		return NBR_OK;	/* successfully suspended */
	}
	else if (r != 0) {	/* error happen */
		fprintf(stderr,"fiber failure %d <%s>\n", r, lua_tostring(m_exec, -1));
		serializer &sr = *m_scr;
		PREPARE_PACK(sr);
		ll_exec_response::pack_header(sr, m_fb->msgid());
		sr.pushnil();
		if ((r = from_stack(lua_gettop(m_exec), sr)) < 0) { 
			ASSERT(false); return r; 
		}
		return respond(true, sr);	
	}
	else {	/* successfully finished */
		serializer &sr = *m_scr;
		PREPARE_PACK(sr);
		ll_exec_response::pack_header(sr, m_fb->msgid());
		if ((r = from_stack(1, sr)) < 0) {
			ASSERT(false); return r; 
		}
		sr.pushnil();
		return respond(false, sr);
	}
}

int lua::coroutine::resume(ll_exec_response &res)
{
	int r;
	if (res.err().type() != datatype::NIL) {
		serializer &sr = *m_scr;
		PREPARE_PACK(sr);
		ll_exec_response::pack_header(sr, m_fb->msgid());
		sr.pushnil();
		/* TODO : need support except string type? */
		if ((r = sr.push_string(res.err(), res.err().len())) < 0) { 
			return r; 
		}
		return respond(true, *m_scr);
	}
	if ((r = to_stack(res)) < 0) { return r; }
	return dispatch(r);
}

int lua::coroutine::push_world_object(object *o)
{
	TRACE("push_world_object:%p,%p,%p,of=%p\n", o, this, m_scr,&(m_scr->of()));
	push_object(o);
	lua_setfield(m_exec, LUA_GLOBALSINDEX, lua::world_object_name);
	return NBR_OK;
}

int lua::coroutine::pack_object(serializer &sr, object &o)
{
	int r;
	if ((r = sr.push_array_len(3)) < 0) { return r; }
	if ((r = (sr << ((U8)LUA_TUSERDATA))) < 0) { return r; }
	if ((r = sr.push_string(o.klass(), strlen(o.klass()))) < 0) { return r; }
	if ((r = sr.push_raw(reinterpret_cast<const char *>(&(o.uuid())), 
		sizeof(UUID))) < 0) {
		return r;
	}
	return NBR_OK;
}

int lua::coroutine::respond(bool err, serializer &sr)
{
	return m_fb->respond(err, sr);
}

int lua::coroutine::from_stack(int start_id, serializer &sr)
{
	int top = lua_gettop(m_exec), r;
	ASSERT(start_id <= top);
	for (int i = start_id; i <= top; i++) {
		if ((r = from_stack(sr, i)) < 0) { return r; }
	}
	return sr.len();
}

int lua::coroutine::from_func(serializer &sr)
{
	int r = NBR_OK;
#if 1
	sr.pushnil();
#else
	lua::writer_context wr;
	wr.sr = &sr;
	wr.n_write = 0;
	int cp = sr.curpos();
	sr.push_raw_len(64 * 1024);
	r = lua_dump(m_exec, lua::writer, &wr);
	lua_pop(m_exec, 1);
	sr.rewind(sr.curpos() - cp);
	sr.push_raw_len(wr.n_write);
	sr.skip(wr.n_write);
#endif
	return r;
}

int lua::coroutine::from_stack(serializer &sr, int stkid) 
{
	int r;
	switch(lua_type(m_exec, stkid)) {
	case LUA_TNIL: 		sr.pushnil(); break;
	case LUA_TNUMBER:	sr << lua_tonumber(m_exec, stkid); break;
	case LUA_TBOOLEAN:	sr << (lua_toboolean(m_exec, stkid) ? true : false); break;
	case LUA_TSTRING:
//		TRACE("type=str: %s\n", lua_tostring(m_exec, stkid));
		sr.push_string(lua_tostring(m_exec, stkid), lua_objlen(m_exec, stkid));
		break;
	case LUA_TTABLE: { /* = map {...} */
		/* convert to real stkid (because minus stkid change its meaning
		* during below iteration) */
		stkid = (stkid < 0 ? lua_gettop(m_exec) + stkid + 1 : stkid);
		r = lua_objlen(m_exec, stkid);
		sr.push_map_len(r);
		r = lua_gettop(m_exec);     /* preserve current stack size to r */
		lua_pushnil(m_exec);        /* push first key (idiom, i think) */
		while(lua_next(m_exec, stkid)) {    /* put next key/value on stack */
			int top = lua_gettop(m_exec);       /* use absolute stkid */
			from_stack(sr, top - 1);        /* pack table key */
			from_stack(sr, top);    /* pack table value */
			lua_pop(m_exec, 1); /* destroy value */
		}
		lua_settop(m_exec, r);      /* recover stack size */
		} break;
	case LUA_TFUNCTION:     /* = array ( LUA_TFUNCTION, binary_chunk ) */
		/* actually it dumps function which placed in stack top.
		but this pack routine calles top -> bottom and packed stack
		stkid is popped. so we can assure target function to dump
		is already on the top of stack here. */
		if (!lua_iscfunction(m_exec, stkid)) {
			sr.push_array_len(2);
			sr << ((U8)LUA_TFUNCTION);
			lua_pushvalue(m_exec, stkid);
			if (from_func(sr) < 0) { return NBR_EINVAL; }
			lua_pop(m_exec, 1);
		} break;
	case LUA_TUSERDATA: { /* = array ( LUA_TUSERDATA, UUID, objectdata ) */
		object *o = to_o(stkid);
		if (o) { pack_object(sr, *o); }
		} break;
	case LUA_TTHREAD:
	case LUA_TLIGHTUSERDATA:
	default:
		//we never use it.
		ASSERT(false);
		return NBR_EINVAL;
	}
	return sr.len();
}

int lua::coroutine::to_func(const rpc::data &d)
{
#if 0
	lua::reader_context rd;
	rd.d = &d;
	rd.ret = 0;
	if (lua_load(vm, lua::reader, &rd, "rfn") < 0) {
		TRACE("lua_load error <%s>\n", lua_tostring(vm, -1));
		return NBR_ESYSCALL;
	}
#endif
	return NBR_OK;
}

int lua::coroutine::to_stack(const rpc::data &d)
{
	size_t i;
	int r;
	bool b;
	switch(d.type()) {
	case datatype::NIL:
		lua_pushnil(m_exec);
		break;
	case datatype::BOOLEAN:
		lua_pushboolean(m_exec, d);
		break;
	case datatype::ARRAY:
		/* various lua types are passed as lua array */
		/* [ type:int(LUA_T*), arg1, arg2, ..., argN ] */
		switch((int)(d.elem(0))) {
		case LUA_TFUNCTION:
			/* [LUA_TFUNCTION, func code:blob] */
			if ((r = to_func(d.elem(1))) < 0) { return r; }
			break;
		case LUA_TUSERDATA:
			/* [LUA_TUSERDATA, subtype:string(blob), UUID:blob] */
			ASSERT(d.elem(1).type() == datatype::BLOB);
			lua_getfield(m_exec, LUA_GLOBALSINDEX, d.elem(1));
			b = lua_istable(m_exec, -1);
			lua_pop(m_exec, 1);
			if (b) {
				/* object */
				world *w = m_scr->wf().find(m_fb->wid());
				if (!w) { ASSERT(false); return NBR_ENOTFOUND; }
				object *o = m_scr->of().load(d.elem(2), w, 
					ll::to_ll(m_scr), d.elem(1));
				if (!o) { ASSERT(false); return NBR_EINVAL; }
				if ((r = push_object(o)) < 0) { return r; }	
			}
			else {
				/* TODO: another developper defined objects */
			}
			break;
		default:
			ASSERT(false);
			return NBR_EINVAL;
		}
		break;
	case datatype::MAP:
		lua_newtable(m_exec);
		for (i = 0; i < d.size(); i++) {
			to_stack(d.key(i));
			to_stack(d.val(i));
			lua_settable(m_exec, -3);
		}
		break;
	case datatype::BLOB:
		lua_pushstring(m_exec, d);
		break;
	case datatype::DOUBLE:
		lua_pushnumber(m_exec, d);
		break;
	case datatype::INTEGER:
		lua_pushinteger(m_exec, d);
		break;
	default:
		ASSERT(false);	/* cannot come here */
		return NBR_EINVAL;
	}
	return NBR_OK;
}

int lua::coroutine::to_stack(ll_exec_request &req, bool trusted)
{
	int r, i, an;
	if ((r = to_stack(req.rcvr())) < 0) { return r; }
	if ((r = to_stack(req.method())) < 0) { return r; }
	lua_gettable(m_exec, -2);	/* load function */
	if (!lua_isfunction(m_exec, -1)) { ASSERT(false); return NBR_EINVAL; }
	lua_pushvalue(m_exec, -2);
	ASSERT(lua_gettop(m_exec) == 3);
	lua_remove(m_exec, 1); /* remove object on bottom of stack */
	for (i = 0, an = req.argc(); i < an; i++) {
		if ((r = to_stack(req.argv(i))) < 0) { return r; }
	}
	return (an + 1); /* +1 for rcvr(re-push object) */
}

int lua::coroutine::to_stack(create_object_request &req)
{
	int r, i, an;
#if defined(_DEBUG)
	char buf[256];
#endif
	lua_getfield(m_exec, LUA_GLOBALSINDEX, req.klass());	/* load class method tbl */
	lua_getfield(m_exec, -1, lua::klass_method_table);
	lua_getfield(m_exec, -1, lua::ctor_string);	/* load constructor */
	if (!lua_isfunction(m_exec, -1)) { ASSERT(false); return NBR_EINVAL; }
	ASSERT(lua_gettop(m_exec) == 3);
	lua_remove(m_exec, 2); lua_remove(m_exec, 1);	/* remove tables */
	world *w = m_scr->wf().find(req.wid());
	if (!w) { return NBR_ENOTFOUND; }
	TRACE("object create uuid = <%s>\n", req.object_id().to_s(buf, sizeof(buf)));
	object *o = m_scr->of().create(req.object_id(), w, 
		ll::to_ll(m_scr), req.klass());
	ASSERT(!o || o == m_scr->of().find(req.object_id()));
	if (!o) { 
		o = m_scr->of().load(req.object_id(), w, ll::to_ll(m_scr), req.klass());
		if (o && o->local()) {
			/* always created! */
#if defined(_DEBUG)
			char buf[256];
			TRACE("object(%s:%p) already created!\n", 
				o->uuid().to_s(buf, sizeof(buf)), m_scr);
#endif
			push_object(o);
			return NBR_EALREADY;
		}
		return NBR_EEXPIRE; 
	}
	push_object(o);
	for (i = 0, an = req.argc(); i < an; i++) {
		if ((r = to_stack(req.argv(i))) < 0) { return r; }
	}
	return (an + 1); /* +1 for push_object(created!) */
}


int lua::coroutine::to_stack(rpc::ll_exec_response &res)
{
	int r;
	if ((r = to_stack(res.ret())) < 0) { return r; }
	return 1;
}

object *lua::coroutine::to_o(int stkid)
{
	lua::userdata *p = (lua::userdata *)lua_touserdata(m_exec, stkid);
	if (!p || p->type != lua::userdata::object) {
		return NULL;
	}
	return p->o;
}

lua::method *lua::coroutine::to_m(int stkid)
{
	lua::userdata *p = (lua::userdata *)lua_touserdata(m_exec, stkid);
	if (!p || p->type != lua::userdata::method) {
		return NULL;
	}
	return &(p->m);
}

lua::coroutine *
lua::coroutine::to_co(VM vm)
{
	lua_pushthread(vm);
	lua_gettable(vm, LUA_REGISTRYINDEX);
	coroutine *co = (coroutine *)lua_touserdata(vm, -1);
	lua_pop(vm, 1);
	return co;
}

int lua::coroutine::push_object(object *o)
{
	/* TODO : prepare different function for main VM and fiber VM ? */
	lua::userdata *p = (lua::userdata *)lua_newuserdata(m_exec, sizeof(userdata));
	p->o = o;
	p->type = lua::userdata::object;
	/* create metatable : TODO : use pre-created metatable */
	bool need_init = false;
	if (CAN_PROCESS(this, o)) {
		lua_pushlstring(m_exec, (const char *)&(o->uuid()), sizeof(UUID));
		if (o->loaded()) {
			o->set_flag(object::flag_loaded, false);
			/* used object data store as well */
			lua_pushvalue(m_exec, -1);
			lua_createtable(m_exec, 0, 100);
#if defined(_DEBUG)
			TRACE("metatable %p:%p:%p:%s\n", &(m_scr->of()), 
				m_scr,lua_topointer(m_exec, -1), o->klass());
			dump_table(m_exec, lua_gettop(m_exec));
#endif
			lua_settable(m_exec, LUA_GLOBALSINDEX);
			/* not created yet: */
			need_init = true;
		}
		lua_gettable(m_exec, LUA_GLOBALSINDEX);
#if defined(_DEBUG)
		if (!o->loaded()) {
			TRACE("metatable %p:%p:%p:%s(reuse)\n", &(m_scr->of()), 
				m_scr,lua_topointer(m_exec, -1), o->klass());
			dump_table(m_exec, lua_gettop(m_exec));
		}
#endif
	}
	else {
		lua_createtable(m_exec, 0, 3);
		need_init = true;
	}
	if (need_init) {
		lua_pushcfunction(m_exec, object_index);
		lua_setfield(m_exec, -2, "__index");
		lua_pushcfunction(m_exec, object_newindex);
		lua_setfield(m_exec, -2, "__newindex");
		lua_pushcfunction(m_exec, generic_gc);
		lua_setfield(m_exec, -2, "__gc");
	}
	/* register metatable */
	lua_setmetatable(m_exec, -2);
	return NBR_OK;
}

int lua::coroutine::object_index(VM vm)
{
	ASSERT(lua_isuserdata(vm, -2));
	coroutine *co = to_co(vm);
	if (!co) {
		lua_pushfstring(vm, "no coroutine correspond to %p", vm);
		lua_error(vm);
	}
	object *o = co->to_o(-2);
	if (CAN_PROCESS(co, o)) {
		lua_getmetatable(vm, -2);	/* object's metatable */
		lua_pushvalue(vm, -2);		/* key to get */
		lua_gettable(vm, -2);
		if (lua_isnil(vm, -1)) {
			/* if not found from object metatable,
			 * then search klass method table */
			lua_getfield(vm, LUA_GLOBALSINDEX, o->klass());
			lua_getfield(vm, -1, lua::klass_method_table);
			lua_pushvalue(vm, -5);
				/* table class.Klass has __index metamethod */
			lua_rawget(vm, -2);
		}
		ASSERT(!lua_isnil(vm, -1));
		return 1;
	}
	lua_CFunction fn;
	const char *mth = lua_tostring(vm, -1);
	if (mth && GET_32(mth) == GET_32(ctor_string)) {
		mth = o->klass();
		fn = coroutine::ctor_call;
	}
	else {
		fn = coroutine::method_call;
	}
	/* return method data instead of real func */
	if (!method_new(vm, fn, mth)) {
		lua_pushstring(vm, "no buffer for new method");
		lua_error(vm);
	}
	return 1;	/* return method object */
}

int lua::coroutine::object_newindex(VM vm)
{
	/* now stack is [object][key to set value][value to set] */
	coroutine *co = to_co(vm);
	if (!co) {
		lua_pushfstring(vm, "no coroutine correspond to %p", vm);
		lua_error(vm);
	}
	object *o = co->to_o(-3);
	if (CAN_PROCESS(co, o)) {
		lua_getmetatable(vm, -3);	/* object's metatable */
		lua_pushvalue(vm, -3);		/* key to set */
		lua_pushvalue(vm, -3);		/* value to set */
		lua_settable(vm, -3);		/* metatable[key] = value */
	}
	else {
		lua_pushstring(vm, "cannot set data to object belongs to other VM");
		lua_error(vm);
	}
	return 0;
}

int lua::coroutine::method_call(VM vm)
{
	/* stuck : method,object,a1,a2,....,aN */
	/* -> method.m_name(object,a1,a2,...,aN)
	 * -> return value is return value of m_name(...) */
	coroutine *co = to_co(vm);
	if (!co) {
		lua_pushfstring(vm, "no coroutine correspond to %p", vm);
		lua_error(vm);
	}
	object *o = co->to_o(2);
	if (!o) {
		lua_pushstring(vm, "function arugment wrong");
		lua_error(vm);
	}
	lua::method *m = co->to_m(1);
	int top = lua_gettop(vm), r;
	if (CAN_PROCESS(co, o)) {
		/* call Klass._ftbl_.func */
		lua_getfield(vm, LUA_GLOBALSINDEX, o->klass());
		ASSERT(lua_istable(vm, -1));
		lua_getfield(vm, -1, lua::klass_method_table);
		ASSERT(lua_istable(vm, -1));
		lua_getfield(vm, -1, m->name());
		ASSERT(lua_isfunction(vm, -1));
		lua_replace(vm, 1);	/* replace lua::method with function */
		lua_pop(vm, 2);	/* remove klass & klass method table */
		if ((r = lua_pcall(vm, top - 1, LUA_MULTRET, 0)) != 0) {
			lua_pushfstring(vm, "local call : fail (%d)", r);
			lua_error(vm);
		}
		return lua_gettop(vm);
	}
	else {
		/* rpc */
		MSGID msgid = co->fb().new_msgid();
		if (msgid == INVALID_MSGID) {
			lua_pushstring(vm, "cannot register fiber");
			lua_error(vm);
		}
		PREPARE_PACK(co->scr());
		ll_exec_request::pack_header(co->scr(), msgid, *o,
			m->name(), strlen(m->name()),
			o->belong()->id(), strlen(o->belong()->id()),
			top - 2);
		/* arguments are starts from index 3 */
		for (int i = 3; i <= top; i++) {
			if ((r = co->from_stack(co->scr(), i)) < 0) {
				lua_pushfstring(vm, "pack lua stack fail at (%d:%d)", i, r);
				lua_error(vm);
			}
		}
		if ((r = o->request(co->scr())) < 0) {
			lua_pushfstring(vm, "send request to object fail (%d)", r);
			lua_error(vm);
		}
		return lua_yield(vm, 0);
	}
}

int lua::coroutine::ctor_call(VM vm)
{
	/* stuck : method,(klass|object),a1,a2,....,aN */
	/* -> klass name,a1,a2,a3....aN is sent to remote
	 * -> method.new(newly created object,a1,a2,...,aN)
	 * -> return value is rv of method.new(...) */
	coroutine *co = to_co(vm);
	if (!co) {
		lua_pushfstring(vm, "no coroutine correspond to %p", vm);
		lua_error(vm);
	}
	UUID uuid;
	uuid.assign();
	lua::method *m = co->to_m(1);
	int top = lua_gettop(vm), r;
	PREPARE_PACK(co->scr());
	MSGID msgid = co->fb().new_msgid();
	if (msgid == INVALID_MSGID) {
		lua_pushstring(vm, "cannot register fiber");
		lua_error(vm);
	}
	create_object_request::pack_header(co->scr(), msgid, uuid, m->name(), strlen(m->name()), 
		co->fb().wid(), strlen(co->fb().wid()), top - 2);
	/* arguments are starts from index 3 (1:method object,2:Klass or object)*/
	for (int i = 3; i <= top; i++) {
		if ((r = co->from_stack(co->scr(), i)) < 0) {
			lua_pushfstring(vm, "pack lua stack fail at (%d:%d)", i, r);
			lua_error(vm);
		}
	}
	if (co->scr().of().request_create(uuid, co->scr()) < 0) {
		lua_pushfstring(vm, "send request object creation fail (%d)", r);
		lua_error(vm);
	}
	return lua_yield(vm, 0);
}


/* luavm */
const char lua::ctor_string[] = "new";
const char lua::klass_method_table[] = "_ftbl_";
const char lua::klass_name_key[] = "_name_";
const char lua::world_klass_name[] = "World";
const char lua::player_klass_name[] = "Player";
const char lua::world_object_name[] = "world";

int lua::init(int max_rpc_ongoing)
{
	/* initialize object pools */
	if (!m_smpool.init(10000, -1, opt_expandable)) {
		return NBR_EMALLOC;
	}
	if (!m_copool.init(max_rpc_ongoing, -1, opt_expandable)) {
		return NBR_EMALLOC;
	}
	if (!(m_vm = lua_newstate(allocator, this))) {
		return NBR_ESYSCALL;
	}
	lua_settop(m_vm, 0);
	/* set panic callback */
	lua_atpanic(m_vm, panic);
	/* load basic library */
	lua_pushcfunction(m_vm, luaopen_base);
	if (0 != lua_pcall(m_vm, 0, 0, 0)) {
		TRACE("lua_pcall fail (%s)\n", lua_tostring(m_vm, -1));
		return NBR_ESYSCALL;
	}
	return NBR_OK;
}

/* load module */
int lua::load_module(world_id wid, const char *srcfile)
{
	/* loadfile only load file into lua stack (thus soon it loses)
	 * so need to call this chunk. */
	if (luaL_loadfile(m_vm, srcfile) != 0) {	/* 1:srcfile func */
		TRACE("luaL_loadfile : error <%s>\n", lua_tostring(m_vm, -1));
		ASSERT(false);
		return NBR_ESYSCALL;
	}
	lua_getfield(m_vm, LUA_REGISTRYINDEX, wid);	/* 2:world env */
	lua_setfenv(m_vm, -2);                          /* 2->removed */
	if (lua_pcall(m_vm, 0, 0, 0) != 0) {            /* 1->removed */
		/* recover metatable */
		return NBR_ESYSCALL;
	}
	return NBR_OK;
}

lua::method *
lua::method_new(VM vm, lua_CFunction fn, const char *name)
{
	userdata *ud = (userdata *)lua_newuserdata(vm, sizeof(userdata));
	ud->type = userdata::method;
	ud->m.m_name = name;
	lua_newtable(vm); /* this is metatable */
	lua_pushcfunction(vm, fn);
	lua_setfield(vm, -2, "__call");
	lua_pushcfunction(vm, generic_gc);
	lua_setfield(vm, -2, "__gc");
	lua_setmetatable(vm, -2);
	/* after this function finish, stack size increases by 1.(USERDATA *ud) */
	return &(ud->m);
}

/* Isolation, etc... */
int lua::init_world(world_id wid, world_id from, const char *srcfile)
{
	TRACE("%p:init VM for world(%s)\n", this, wid);
	lua_settop(m_vm, 0);
	lua_getfield(m_vm, LUA_REGISTRYINDEX, wid);
	if (!lua_isnil(m_vm, -1)) {
		lua_pop(m_vm, 1);
		TRACE("%p: already initialized\n", this);
		return NBR_OK;  /* already initialized */
	}
	lua_pop(m_vm, 1);
	if (from) { /* reuse existing table */
		lua_getfield(m_vm, LUA_REGISTRYINDEX, from);
		if (!lua_isnil(m_vm, -1)) {
			lua_setfield(m_vm, LUA_REGISTRYINDEX, wid);
			return NBR_OK;
		}
		lua_pop(m_vm, 1);       /* remove nil object */
	}
	/* initialize new env */
	lua_newtable(m_vm);
	/* create basic method table */
	lua_newtable(m_vm);
	lua_setfield(m_vm, -2, "_generic_");
	/* load pfm library table */
	lua_newtable(m_vm);
	/* add API class */
	lua_pushcfunction(m_vm, add_class); 
	lua_pushvalue(m_vm, -3);
	lua_setfenv(m_vm, -2);	/* change env table */
	lua_setfield(m_vm, -2, "class");
	/* add API typeof */
	lua_pushcfunction(m_vm, get_object_type);
	lua_pushvalue(m_vm, -3);
	lua_setfenv(m_vm, -2);	/* change env table */
	lua_setfield(m_vm, -2, "typeof");
	/* register to new world env table which name as pfm */
	lua_setfield(m_vm, -2, "pfm");
	/* copy built in library into this table */
	if (copy_table(m_vm, LUA_GLOBALSINDEX, lua_gettop(m_vm), LUA_TNIL) < 0) {
		return NBR_ESYSCALL;
	}
	ASSERT(lua_istable(m_vm, -1));
	/* insert this table and initialize */
	lua_setfield(m_vm, LUA_REGISTRYINDEX, wid);
	return load_module(wid, srcfile);
}

void lua::fin_world(world_id wid)
{
	lua_pushnil(m_vm);
	lua_setfield(m_vm, LUA_GLOBALSINDEX, wid);
}

void lua::fin()
{
	if (m_vm) {
		lua_close(m_vm);
		m_vm = NULL;
	}
	m_smpool.fin();
	m_copool.fin();
}

int lua::class_newindex(VM vm)
{
	ASSERT(lua_istable(vm, -3));
	if (lua_isfunction(vm, -1)) {
		/* function should be registered real func table */
		lua_getfield(vm, -3, klass_method_table);	/* real func table */
		ASSERT(lua_istable(vm, -1));
		lua_pushvalue(vm, -3);	/* copy key */
		lua_pushvalue(vm, -3);	/* copy value */
	}
	lua_rawset(vm, -3);
	return 0;
}

int lua::class_index(VM vm)
{
	ASSERT(lua_istable(vm, -2) && lua_isstring(vm, -1));
	const char *mth = lua_tostring(vm, -1);
	if (!mth) {
		lua_pushstring(vm, "class method should be string");
		lua_error(vm);
	}
	lua_CFunction fn;
	if (GET_32(mth) == GET_32(ctor_string)) {
		lua_getfield(vm, -2, klass_name_key);
		mth = lua_tostring(vm, -1);
		fn = coroutine::ctor_call;
	}
	else {
		fn = coroutine::method_call;
	}
	/* return method data instead of real func */
	if (!method_new(vm, fn, mth)) {
		lua_pushstring(vm, "no buffer for new method");
		lua_error(vm);
	}
	return 1;	/* return method object */
}

int lua::add_class(VM vm)
{
	const char *klass;
	if (!(klass = lua_tostring(vm, -1))) {
		lua_pushstring(vm, "class name should be string");
		lua_error(vm);
	}
	/* set up new class table */
	lua_newtable(vm);
	/* set field to it */
	lua_pushstring(vm, klass);
	lua_setfield(vm, -2, klass_name_key);	/* class name */
	lua_newtable(vm);
	lua_setfield(vm, -2, klass_method_table); /* real method table */
	/* set metatable */
	lua_newtable(vm);
	lua_pushcfunction(vm, class_index);
	lua_setfield(vm, -2, "__index");
	lua_pushcfunction(vm, class_newindex);
	lua_setfield(vm, -2, "__newindex");
	lua_setmetatable(vm, -2);
	lua_pushvalue(vm, -1);
	lua_setfield(vm, LUA_ENVIRONINDEX, klass);
	return 1;
}

int lua::generic_gc(VM vm)
{
	return 0;
}

int lua::panic(VM vm)
{
	fprintf(stderr, "lua: panic: <%s>\n", lua_tostring(vm, -1));
	ASSERT(false);
	return 0;
}

void *lua::allocator(void *ud, void *ptr, size_t os, size_t ns)
{
	lua* scp = (lua *)ud;
	if (ns == 0) {
		if (os <= smblock_size) {
			scp->smpool().destroy((char *)ptr);
			return NULL;
		}
		free(ptr);  /* ANSI define free for NULL ptr never causes any change */
		return NULL;
	}
	else {
		/* ANSI defines realloc(NULL,ns) == malloc(ns) */
		void *p;
		if (ns <= smblock_size) {
			if (!(p = scp->smpool().alloc())) {
				ASSERT(false);
				return NULL;
			}
			if (ptr) {
				if (os <= smblock_size) {
					return ptr;
				}
				memcpy(p, ptr, ns);
				free(ptr);
			}
			return p;
		}
		else if (ptr && os <= smblock_size) {
			if (!(p = malloc(ns))) {
				ASSERT(false);
				return NULL;
			}
			memcpy(p, ptr, ns);
			scp->smpool().destroy((char *)ptr);
		}
		else {
			ASSERT((!ptr && ns > smblock_size) || 
				(ptr && os > smblock_size && ns > smblock_size));
			p = realloc(ptr, ns);
		}
		ASSERT(p);
		return p;
	}
}

int lua::copy_table(VM vm, int from, int to, int type)
{
	//TRACE("copy_table(%u) : %d -> %d\n", type, from, to);
	int cnt = 0;
	ASSERT(lua_istable(vm, from) && lua_istable(vm, to));
	lua_pushnil(vm);        /* push first key (idiom, i think) */
	while(lua_next(vm, from)) {     /* put next key/value on stack */
		if (type > 0 && lua_type(vm, -1) != type) { continue; }
		const char *k = lua_tostring(vm, -2);
		if (!k) { continue; }
		// TRACE("add element[%s]:%u:%u\n", k, lua_type(vm, -1), lua_gettop(vm));
		lua_setfield(vm, to, k);
		cnt++;
	}
	return cnt;
}

void lua::dump_table(VM vm, int index)
{
	ASSERT(index > 0 || index < -10000);      /* should give positive index(because minus index
				changes its meaning after lua_pushnil below) */
	lua_pushnil(vm);	/* push first key (idiom, i think) */
	printf("table ptr = %p\n", lua_topointer(vm, index));
	if (lua_topointer(vm, index) == NULL) { return; }
	while(lua_next(vm, index)) {     /* put next key/value on stack */
		printf("table[%s]=%u\n", lua_tostring(vm, -2), lua_type(vm, -1));
		lua_pop(vm, 1);
	}
}


