/***************************************************************
 * lua.h : template implementation part of lua.hpp
 * 2010/02/15 iyatomi : create
 *                             Copyright (C) 2008-2010 Takehiro Iyatomi
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

/*-------------------------------------------------------------*/
/* sfc::vm::lang::lua_convtype								   */
/*-------------------------------------------------------------*/
/* mp - lua : implemented in lua.cpp */
inline lua_Number
lua_convtype::to_i(mp::data &d)
{
	ASSERT(d.type == msgpack::type::DOUBLE);
	return d.via.dec;
}

inline lua_Boolean
lua_convtype::to_b(mp::data &d)
{
	ASSERT(d.type == msgpack::type::BOOLEAN);
	return d.via.boolean;
}

inline const char*
lua_convtype::to_s(mp::data &d)
{
	ASSERT(d.type == msgpack::type::RAW);
	return d.via.raw.ptr;
}

inline const char*
lua_convtype::to_p(mp::data &d)
{
	ASSERT(d.type == msgpack::type::RAW);
	return d.via.raw.ptr;
}

inline int
lua_convtype::to_rawlen(mp::data &d)
{
	ASSERT(d.type == msgpack::type::RAW);
	return d.via.raw.size;
}

inline int
lua_convtype::to_maplen(mp::data &d)
{
	ASSERT(d.type == msgpack::type::MAP);
	return d.via.map.size;
}

inline int
lua_convtype::to_alen(mp::data &d)
{
	ASSERT(d.type == msgpack::type::ARRAY);
	return d.via.array.size;
}

inline mp::data &
lua_convtype::key(mp::data &d, int i)
{
	ASSERT(d.type == msgpack::type::MAP);
	ASSERT(d.via.map.size > ((size_t)i));
	return d.via.map.ptr[i].key;
}

inline mp::data &
lua_convtype::val(mp::data &d, int i)
{
	ASSERT(d.type == msgpack::type::MAP);
	ASSERT(d.via.map.size > ((size_t)i));
	return d.via.map.ptr[i].val;
}

inline mp::data &
lua_convtype::elem(mp::data &d, int i)
{
	ASSERT(d.type == msgpack::type::ARRAY);
	ASSERT(d.via.array.size > ((size_t)i));
	return d.via.array.ptr[i];
}

/* dont use it for child */
inline int
lua_convtype::type(mp::data &d)
{
	switch(d.type) {
	case msgpack::type::NIL:
		return LUA_TNIL;
	case msgpack::type::BOOLEAN:
		return LUA_TBOOLEAN;
	case msgpack::type::POSITIVE_INTEGER:
	case msgpack::type::NEGATIVE_INTEGER:
	case msgpack::type::DOUBLE:
		return LUA_TNUMBER;
	case msgpack::type::MAP:
		return LUA_TTABLE;
	case msgpack::type::RAW:
		return LUA_TSTRING;
	case msgpack::type::ARRAY:
		ASSERT(d.via.array.size > 0);
		return elem(d, 0).via.i64;
	default:
		ASSERT(false);
		return LUA_TNIL;
	}
}

/*-------------------------------------------------------------*/
/* sfc::vm::lang::lua                                          */
/*-------------------------------------------------------------*/
/* static variable */
template <class SR, class OF> 
array<typename lua<SR,OF>::rpc> 	lua<SR,OF>::m_rpcs;
template <class SR, class OF> 
array<typename lua<SR,OF>::fiber> 	lua<SR,OF>::m_fibers;
template <class SR, class OF> 
map<typename lua<SR,OF>::type_id, char *> 	lua<SR,OF>::m_types;
template <class SR, class OF> 
lua_State				*lua<SR,OF>::m_vm = NULL;
template <class SR, class OF>
RWLOCK					lua<SR,OF>::m_lock = NULL;
/* lua */
template <class SR, class OF>
int lua<SR,OF>::init(int max_rpc_entry, int max_rpc_ongoing)
{
	if (!m_rpcs.init(max_rpc_entry, -1, opt_threadsafe | opt_expandable)) {
		return NBR_EMALLOC;
	}
	if (!m_fibers.init(max_rpc_ongoing, -1, opt_threadsafe | opt_expandable)) {
		return NBR_EMALLOC;
	}
	if (!m_types.init(256, 256, -1, opt_threadsafe | opt_expandable)) {
		return NBR_EMALLOC;
	}
	if (!(m_vm = lua_newstate(allocator, NULL))) {
		return NBR_ESYSCALL;
	}
	if (!(m_lock = nbr_rwlock_create())) {
		return NBR_EPTHREAD;
	}
	lua_settop(m_vm, 0);
	/* load built in library */
	lua_pushcfunction(m_vm, luaopen_base);
	lua_pcall(m_vm, 0, 0, 0);
	/* set panic callback */
	lua_atpanic(m_vm, panic);
	/* create global table object (that represent library) */
	lua_newtable(m_vm);
	lua_setfield(m_vm, LUA_REGISTRYINDEX, "_generic_");
	lua_newtable(m_vm);
	lua_setglobal(m_vm, "pfm");
	/* add API */
	lua_getglobal(m_vm, "pfm");
	lua_pushcfunction(m_vm, create);
	lua_setfield(m_vm, -2, "new");
	lua_pushcfunction(m_vm, set_object_type);
	lua_setfield(m_vm, -2, "bless");
	lua_pushcfunction(m_vm, get_object_type);
	lua_setfield(m_vm, -2, "typeof");
	return NBR_OK;
}

template <class SR, class OF>
bool lua<SR,OF>::load(const char *srcfile)
{
	/* loadfile only load file into lua stack (thus soon it loses)
	 * so need to call this chunk. */
	return (luaL_loadfile(m_vm, srcfile) == 0 &&
			lua_pcall(m_vm, 0, 0, 0) == 0);
}

template <class SR, class OF>
int	lua<SR,OF>::panic(VM vm)
{
	fprintf(stderr, "lua: panic: <%s>\n", lua_tostring(vm, -1));
	ASSERT(false);
	return 0;
}


template <class SR, class OF>
void	*lua<SR,OF>::allocator(void *ud, void *ptr, size_t os, size_t ns)
{
#if defined(_DEBUG)
	if (ns > (32 * 1024) || os > (32 * 1024)) {
		TRACE("ptr=%p,os=%u,ns=%u\n", ptr, os, ns);
	}
#endif
	if (ns == 0) {
#if defined(_DEBUG)
		if (os > (32 * 1024)) {
			static char correct[32 * 1024];
			memset(correct, 'a', 32 * 1024);
			ASSERT(memcmp(((char *)ptr) + os, correct, 32 * 1024) == 0);
		}
#endif
		free(ptr);  /* ANSI define free for NULL ptr never causes any change */
		return NULL;
	}
	else {
		/* ANSI defines realloc(NULL,ns) == malloc(ns) */
#if defined(_DEBUG)
		void *p;
		if (ns > (32 * 1024) && os == 0) {
			size_t nns = ns + (32 * 1024);
			p = realloc(ptr, nns);
			memset(((char *)p) + ns, 'a', 32 * 1024);
		}
		else {
			p = realloc(ptr, ns);
		}
		return p;
#else
		return realloc(ptr, ns);
#endif
	}
}

template <class SR, class OF>
int 	lua<SR,OF>::get_object_method(VM vm, const object &o, const char *key)
{
	/* push stack to registry[uuid][key] */
	/* search object value table */
	lua_getfield(vm, LUA_REGISTRYINDEX, o.type());
	ASSERT(lua_istable(vm, -1));
	lua_getfield(vm, -1, key);
	ASSERT(lua_isnil(vm, -1) || lua_isfunction(vm, -1) ||
			lua_iscfunction(vm, -1) || lua_isuserdata(vm, -1));
	lua_remove(vm, -2); /* remove method table */
	/* finally stack size +1 */
	return 0;
}

template <class SR, class OF>
int 	lua<SR,OF>::set_object_method(
			VM vm, const object &o, const char *key, int index_from)
{
	ASSERT(index_from > 0);	/* should use lua_gettop or something */
	lock lk(m_lock, true);
	lua_getfield(vm, LUA_REGISTRYINDEX, o.type());
	lua_pushvalue(vm, index_from);
	lua_setfield(vm, -2, key);
	lua_pop(vm, 1);	/* remove method table from stack */
	/* finally stack size not changes */
	return 0;
}

template <class SR, class OF>
int 	lua<SR,OF>::get_object_value(VM vm, const object &o, const char *key)
{
	char buf[256];
	/* push stack to registry[uuid][key] */
	/* search object value table */
	lock lk(m_lock, false);		
	lua_getfield(vm, LUA_REGISTRYINDEX, o.uuid().to_s(buf, sizeof(buf)));
	ASSERT(lua_istable(vm, -1));
	lua_getfield(vm, -1, key);
	if (lua_isnil(vm, -1)) {	/* not exist? */
		lua_pop(vm, 1);
		get_object_method(vm, o, key);
	}
	lua_remove(vm, -2);	/* remove object table */
	return 0;
}

template <class SR, class OF>
int 	lua<SR,OF>::set_object_value(
			VM vm, const object &o, const char *key, int index_from)
{
	ASSERT(index_from > 0);	/* should use lua_gettop or something */
	char buf[256];
	/* do registery[uuid][key] = value */
	lock lk(m_lock, true);
	lua_getfield(vm, LUA_REGISTRYINDEX, o.uuid().to_s(buf, sizeof(buf)));
	lua_pushvalue(vm, index_from);
	lua_setfield(vm, -2, key);
	lua_pop(vm, 1);	/* remove object table */
	return 0;
}

template <class SR, class OF>
int 	lua<SR,OF>::pack_object_value(VM vm, const object &o, SR &sr)
{
	char buf[256];
	lua_getfield(vm, LUA_REGISTRYINDEX, o.uuid().to_s(buf, sizeof(buf)));
	return pack_lua_stack(sr, vm, -1);
}

template <class SR, class OF>
int 	lua<SR,OF>::unpack_object_value(CF &cf, const world_id &wid, 
		VM vm, const object &o, data &d)
{
	int r;
	char buf[256];
	if ((r = put_to_lua_stack(cf, wid, vm, d)) < 0) { return r; }
	lua_setfield(vm, LUA_REGISTRYINDEX, o.uuid().to_s(buf, sizeof(buf)));
	return NBR_OK;
}

template <class SR, class OF>
typename lua<SR,OF>::object *
lua<SR,OF>::object_new(CF &cf, const world_id &wid, VM vm, 
		UUID &uuid, SR *sr, bool local)
{
	object *o = OF::find(uuid);
	if (o) {
		/* required non-local object but already object exist locally */
		if (o->local() || o->local() == local) { return o; }
		else { ASSERT(false); return NULL; }
	}
	if (!(o = OF::create(uuid, local))) { return NULL; }
	char buf[256];
	const char *id = uuid.to_s(buf, sizeof(buf));
	TRACE("try to new object <%s>\n", id);
	lock lk(m_lock, true);
	vm = vm ? vm : m_vm;
	lua_getfield(vm, LUA_REGISTRYINDEX, id);
	if (lua_isnil(vm, -1)) {
		TRACE("create new table for <%s>\n", id);
		lua_newtable(vm);
		lua_setfield(vm, LUA_REGISTRYINDEX, id);
	}
	lua_pop(vm, 1);/* remove table(or nil) from lua stack */
	if (sr) {
		data d;
		if (sr->unpack(d) <= 0) {
			o->set_connection(NULL);	/* no unpack data specified */
			return o;
		}
		if (unpack_object(cf, wid, vm, d, *o) < 0) {
			OF::destroy(o);
			return NULL;
		}
	}
	return o;
}

template <class SR, class OF>
typename lua<SR,OF>::rpc *
lua<SR,OF>::rpc_new(VM lua, object *o, const char *name)
{
	USERDATA *ud = (USERDATA *)lua_newuserdata(lua, sizeof(USERDATA));
	if (!(ud->o = m_rpcs.create())) {
		return NULL;
	}
	((rpc *)ud->o)->set(o, name);
	ud->type = OT_RPC;
	lua_newtable(lua); /* this is metatable */
	lua_pushcfunction(lua, call);
	lua_setfield(lua, -2, "__call");
	lua_pushcfunction(lua, gc);
	lua_setfield(lua, -2, "__gc");
	lua_setmetatable(lua, -2);
	/* after this function finish, stack size increases by 1.(USERDATA *ud) */
	return (rpc *)ud->o;
}

template <class SR, class OF>
typename lua<SR,OF>::fiber *lua<SR,OF>::fiber_new(S *c, U32 msgid, U32 opt)
{
	fiber *fb = m_fibers.create();
	if (!fb) { ASSERT(false); return NULL; }
	lock lk(m_lock, true);
	if (fb->init_from_vm(m_vm, c, msgid, opt)) {
		return fb;
	}
	else {
		m_fibers.destroy(fb);
		return NULL;
	}
}

template <class SR, class OF>
void lua<SR,OF>::push_object(VM vm, object *o)
{
	USERDATA *p = (USERDATA *)lua_newuserdata(vm, sizeof(USERDATA));
	p->o = o;
	p->type = OT_PFM;
	/* create metatable */
	lua_newtable(vm); /* this is metatable */
	lua_pushcfunction(vm, index);
	lua_setfield(vm, -2, "__index");
	lua_pushcfunction(vm, newindex);
	lua_setfield(vm, -2, "__newindex");
	lua_pushcfunction(vm, gc);
	lua_setfield(vm, -2, "__gc");
	/* register metatable */
	lua_setmetatable(vm, -2);
	return;
}

template <class SR, class OF>
int lua<SR,OF>::pack_lua_stack(SR &sr, VM lua, int index)
{
	int r;
	ASSERT(index != 0);
//	TRACE("pack lua value on stack(%d:%d)\n", index, lua_type(lua, index));
	switch(lua_type(lua, index)) {
	case LUA_TNIL:
		sr.pushnil();
		break;
	case LUA_TNUMBER:
		sr << lua_tonumber(lua, index);
		break;
	case LUA_TBOOLEAN:
		sr << (lua_toboolean(lua, index) ? true : false);
		break;
	case LUA_TSTRING:
//		TRACE("type=str: %s\n", lua_tostring(lua, index));
		sr.push_string(lua_tostring(lua, index), lua_objlen(lua, index));
		break;
	case LUA_TTABLE: { /* = map {...} */
		/* convert to real index (because minus index change its meaning
		 * during below iteration) */
		index = (index < 0 ? lua_gettop(lua) + index + 1 : index);
		r = lua_objlen(lua, index);
		ASSERT(index < 30);
//		TRACE("lua: table at %d length %d\n", index, r);
		sr.push_map_len(r);
		r = lua_gettop(lua);	/* preserve current stack size to r */
		lua_pushnil(lua);	/* push first key (idiom, i think) */
		while(lua_next(lua, index)) {	/* put next key/value on stack */
			int top = lua_gettop(lua);	/* use absolute index */
			pack_lua_stack(sr, lua, top - 1);	/* pack table key */
			pack_lua_stack(sr, lua, top);	/* pack table value */
			lua_pop(lua, 1);	/* destroy value */
		}
		lua_settop(lua, r);	/* recover stack size */
		} break;
	case LUA_TFUNCTION:	/* = array ( LUA_TFUNCTION, binary_chunk ) */
		/* actually it dumps function which placed in stack top.
		but this pack routine calles top -> bottom and packed stack
		index is popped. so we can assure target function to dump
		is already on the top of stack here. */
		if (!lua_iscfunction(lua, index)) {
			sr.push_array_len(2);
			sr << ((U8)LUA_TFUNCTION);
			lua_pushvalue(lua, index);
			write_func_chunk wfc;
			wfc.sr = &sr;
			wfc.n_write = 0;
			int cp = sr.curpos();
			sr.push_raw_len(max_rpc_packlen);
			r = lua_dump(lua, chunk_sr_writer, &wfc);
			lua_pop(lua, 1);
			sr.rewind(sr.curpos() - cp);
			sr.push_raw_len(wfc.n_write);
			sr.skip(wfc.n_write);
			if (r < 0) { return NBR_EINVAL; }
		} break;
	case LUA_TUSERDATA: { /* = array ( LUA_TUSERDATA, UUID, objectdata ) */
		object *o = to_o(lua, index, false);
		if (o) {
			sr.push_array_len(3);
			sr << ((U8)LUA_TUSERDATA);
			sr.push_raw((char *)(&(o->uuid())), sizeof(UUID));
			/* not pack object value it self */
			if ((r = pack_object(sr, *o, false)) < 0) { return r; }
		}
		} break;
	case LUA_TTHREAD:
	case LUA_TLIGHTUSERDATA:
		//we never use it.
		ASSERT(false);
		return NBR_EINVAL;
	}
	return sr.len();
}

template <class SR, class OF>
bool lua<SR,OF>::unpack_lua_stack(CF &cf, const world_id &wid, SR &sr, VM lua)
{
	typename SR::data d;
	if (sr.unpack(d) <= 0) {
		return false;
	}
	put_to_lua_stack(cf, wid, lua, d);
	return true;
}
template <class SR, class OF>
int lua<SR,OF>::put_to_lua_stack(CF &cf, const world_id &wid, VM lua, data &d)
{
	switch(type(d)) {
	case LUA_TNIL:
		lua_pushnil(lua);
		break;
	case LUA_TNUMBER:
		lua_pushnumber(lua, to_i(d));
		break;
	case LUA_TBOOLEAN:
		lua_pushboolean(lua, to_b(d));
		break;
	case LUA_TSTRING:
		lua_pushstring(lua, to_s(d));
		break;
	case LUA_TTABLE:
		lua_newtable(lua);
		for (int i = 0; i < to_maplen(d); i++) {
			put_to_lua_stack(cf,wid,lua,key(d,i));
			put_to_lua_stack(cf,wid,lua,val(d,i));
			lua_settable(lua, -3);
		}
		break;
	case LUA_TFUNCTION:/* = array ( LUA_TFUNCTION, binary_chunk ) */ {
			read_func_chunk rfc;
			rfc.d = &(elem(d, 1));
			rfc.ret = 0;
			if (lua_load(lua, chunk_sr_reader, &rfc, "rfn") < 0) {
				TRACE("lua_load error <%s>\n", lua_tostring(lua, -1));
				return NBR_ESYSCALL;
			}
		} break;
	case LUA_TUSERDATA: {/* = array ( LUA_TUSERDATA, UUID, objectdata ) */
		object *o = object_new(cf, wid, lua, 
				*((UUID *)to_p(elem(d, 1))), NULL, false);
		if (o) { 
			if (o->remote() && unpack_object(cf, wid, lua, elem(d, 2), *o) < 0) {
				return NBR_EFORMAT;
			}
			push_object(lua, o); 
		}
		else { 
			return NBR_EEXPIRE; 
		}
		} break;
	case LUA_TTHREAD:
	case LUA_TLIGHTUSERDATA:                //we never use it.
		ASSERT(false);
		return NBR_EINVAL;
	}
	return NBR_OK;
}

template <class SR, class OF>
typename lua<SR,OF>::object *
lua<SR,OF>::to_o(VM lua, int index, bool abort)
{
	ASSERT(index != 0);
	USERDATA *ud = (USERDATA *)lua_touserdata(lua, index);
	if (!ud || ud->type != OT_PFM) {
		if (abort) {
			lua_pushstring(lua, "invalid arg");
			lua_error(lua);
		}
		return NULL;
	}
	return (object *)ud->o;
}

template <class SR, class OF>
typename lua<SR,OF>::rpc *
lua<SR,OF>::to_r(VM lua, int index, bool abort)
{
	ASSERT(index != 0);
	USERDATA *ud = (USERDATA *)lua_touserdata(lua, index);
	if (!ud || ud->type != OT_RPC) {
		if (abort) {
			lua_pushstring(lua, "invalid arg");
			lua_error(lua);
		}
		return NULL;
	}
	return (rpc *)ud->o;
}

template <class SR, class OF>
const char *lua<SR,OF>::to_k(VM lua, int index)
{
	ASSERT(index != 0);
	const char *k = lua_tostring(lua, index);
	if (strlen(k) >= max_symbol_len) {
		lua_pushfstring(lua, "too long string (%s)\n", k);
		lua_error(lua);
	}
	return k;
}

template <class SR, class OF>
const char *lua<SR,OF>::chunk_sr_reader(VM lua, void *p, size_t *l)
{
	read_func_chunk *rfc = (read_func_chunk *)p;
	if (rfc->ret) { 
		return NULL;/* already return all data */
	}
	*l = to_rawlen(*(rfc->d));
	return to_p(*(rfc->d));
}

template <class SR, class OF>
int      lua<SR,OF>::chunk_sr_writer(VM lua, const void *p, size_t l, void *ud)
{
	write_func_chunk *wfc = (write_func_chunk *)ud;
	if (wfc->sr->push_raw_onlydata((const char *)p, l) < 0) {
		return -1;
	}
	wfc->n_write += l;
	return 0;
}

template <class SR, class OF>
typename lua<SR,OF>::fiber *
lua<SR,OF>::vm_owner(VM lua)
{
	return fiber::get_owner(m_vm, lua);
}

template <class SR, class OF>
int	lua<SR,OF>::reply_result(S &c, VM vm, int n_basestksz, rpctype rt)
{
	ASSERT(n_basestksz >= 0);
	PREPARE_PACK((&c), max_rpc_packlen);
	SR &sr = c.sr();
	int top = lua_gettop(vm);
	for (int i = n_basestksz, r; i <= top; i++) {
		if ((r = pack_lua_stack(sr, vm, i)) < 0) {
			FINISH_PACK((&c));
			return r;
		}
	}
	fiber *f = vm_owner(vm);
	int r = c.reply_rpc(c, f->rmsgid(), sr.p(), sr.len(), rt);
	FINISH_PACK((&c));
	return r;
}

template <class SR, class OF>
int 	lua<SR,OF>::dispatch(S &c, VM vm, int n_args, bool from_resume, rpctype rt)
{
	int top = lua_gettop(vm);
	int stksz = (from_resume ? (top - n_args + 1) : (top - n_args));
	TRACE("%p: stksz = %u, top,nargs=%u,%u(f=%s)\n", (void *)(((U32)vm) - 40), 
			stksz, top, n_args, from_resume ? "resume" : "first");
	int r = lua_resume(vm, n_args);
	if (r == LUA_YIELD) {
		return NBR_OK /* successfully suspended */;
	}
	else {
		fiber *fb = vm_owner(vm);
		if (r != 0) {
			/* error happen */
			TRACE("fiber execution failure %d <%s>\n", r, lua_tostring(vm, -1));
			if (fb->need_reply()) {
				/* send only remote error message to caller */
				reply_result(c, vm, lua_gettop(vm) - 1, vmprotocol::rpct_error);
			}
			m_fibers.destroy(fb);
			return NBR_ESYSCALL;
		}
		else {
			/* sucessfully finished */
			r = fb->need_reply() ? reply_result(c, vm, stksz, rt) : NBR_OK;
			/* whatever result is, free fiber for this function call */
			m_fibers.destroy(fb);
		}
		return r;
	}
}

template <class SR, class OF>
int lua<SR,OF>::pack_object(SR &sr, const object &o, bool with_val)
{
	/* object data = ARRAY( ARRAY(host list), [MAP(object data) (optional)] ) */
	sr.push_array_len(with_val ? 2 : 1);	
	sr.push_string(o.type(), strlen(o.type()));
	if (with_val) {
		lock lk(m_lock, true);
		return pack_object_value(m_vm, o, sr);
	}
	return NBR_OK;
}

template <class SR, class OF>
int lua<SR,OF>::unpack_object(CF &cf, const world_id &wid, VM vm, data &d, object &o)
{
	if (to_alen(d) <= 0) { 
		ASSERT(false); 
		return NBR_EFORMAT; 
	}
	/* connect to assigned node */
	world *w = OF::find_world(wid);
	ASSERT(w);
	typename OF::conn *c = w->connect_assigned_node(cf, o.uuid());
	if (!c) { return NBR_ENOTFOUND; }
	o.set_connection(c);
	/* add object type */
	const char *t = add_type(to_s(elem(d, 0)));
	if (!t) { return NBR_EEXPIRE; }
	ASSERT(*t);
	o.set_type(t);
	TRACE("object type : <%s>\n", o.type());
	/* unpack object data */
	if (to_alen(d) > 1) {
		return unpack_object_value(cf, wid, m_vm, o, elem(d, 1));
	}
	return NBR_OK;
}

template <class SR, class OF>
int lua<SR,OF>::resume_create(S &c, CF &cf, const world_id &wid, VM vm, 
		UUID &uuid, SR &sr)
{
	/* create user object (remote) */
	object *o = object_new(cf, wid, vm, uuid, &sr, false);
	/* push to stack (with metamethod) */
	push_object(vm, o);
	/* yield! 
	 * TODO : need to pass correct rpctype? */
	return dispatch(c, vm, 1, true, vmprotocol::rpct_method);
}

template <class SR, class OF>
int	lua<SR,OF>::resume_proc(S &c, CF &cf, const world_id &wid, 
		VM vm, char *args, size_t alen, rpctype rt)
{
	c.sr().unpack_start(args, alen);
	int n_args = 0;
	lua_settop(vm, 0);
	switch(rt) {
	case vmprotocol::rpct_error:
		/* unpack error string on stack */
		lua_getfield(vm, LUA_GLOBALSINDEX, "remote_error");
		lua_pushstring(vm, (const char *)c.addr());
		unpack_lua_stack(cf, wid, c.sr(), vm);
		if (!lua_isnil(vm, -2)) {/* if error function is defined */
			lua_pcall(vm, 2, 0, 0);
		}
		return NBR_OK;
	case vmprotocol::rpct_getter:
		if (unpack_lua_stack(cf, wid, c.sr(), vm) < 0) {
			lua_pushnil(vm);
			return dispatch(c, vm, 1, true, rt);
		}
		/* if function type returned, return rpc object instead. */
		if (lua_type(vm, -1) == LUA_TFUNCTION) {
			lua_pop(vm, 1);
			data proc_id, uuid;
			if (c.sr().unpack(proc_id) <= 0 || c.sr().unpack(uuid) <= 0) {
				lua_pushnil(vm);
				return dispatch(c, vm, 1, lua_rpc_reply, rt);
			}
			object *o = OF::find(*((UUID *)to_p(uuid)));
			if (!o || !rpc_new(vm, o, to_s(proc_id))) {
				lua_pushnil(vm);	/* return nil */
			}
			else {	/* set rpc object to object data table. */
				/* duplicate rpc object for set_object_method */
				lua_pushvalue(vm, -1);	
				set_object_method(vm, *o, to_s(proc_id), lua_gettop(vm));/* set */
				/* here, duplicated rpc object is remove from stack but original 
				 * rpc object still remain. so can return rpc object to caller. */
			}
		}
		return dispatch(c, vm, 1, true, rt);
	case vmprotocol::rpct_setter:
	default:
		while(unpack_lua_stack(cf, wid, c.sr(), vm)) {
			n_args++;
			if (n_args > 100) {
				ASSERT(false);
				return NBR_EINVAL;
			}
		}
		return dispatch(c, vm, n_args, true, rt);
	}
}

template <class SR, class OF>
int lua<SR,OF>::call_proc(S &c, CF &cf, const world_id &wid, 
		U32 rmsgid, object &o, proc_id &p,
		char *args, size_t alen, rpctype rt, U32 rpcopt)
{
	data d;
	fiber *fb = fiber_new(&c, rmsgid, rpcopt);
	if (!fb) {
		return NBR_EEXPIRE;
	}
	int n_args = 0, n_basestk = 1, r;
	lua_settop(*fb, 0);
	c.sr().unpack_start(args, alen);
	switch(rt) {
	case vmprotocol::rpct_getter:
		TRACE("getter called\n");
		if (c.sr().unpack(d) <= 0) {
			lua_pushstring(*fb, "remote unpack error");
			return reply_result(c, *fb, 1, vmprotocol::rpct_error);
		}
		char b[256];
		TRACE("field name = %s.%s\n", o.uuid().to_s(b, sizeof(b)), to_s(d));
		get_object_value(*fb, o, to_s(d));
		if (lua_type(*fb, -1) == LUA_TFUNCTION) {
			lua_pushstring(*fb, p);
			lua_pushlstring(*fb, (char *)&(o.uuid()), sizeof(UUID));
		}
		r = reply_result(c, *fb, 1, rt);	/* return value only (except function)
							value, proc_id, uuid (when function)*/
		m_fibers.destroy(fb);
		return r;
	case vmprotocol::rpct_setter:
		TRACE("setter called\n");
		if (c.sr().unpack(d) <= 0) {
			lua_pushstring(*fb, "remote unpack error");
			ASSERT(lua_isstring(*fb, -1));
			return reply_result(c, *fb, 1, vmprotocol::rpct_error);
		}
		TRACE("field name = %s.%s\n", o.uuid().to_s(b, sizeof(b)), to_s(d));
		unpack_lua_stack(cf, wid, c.sr(), *fb);	/* object put on stack */
		set_object_value(*fb, o, to_s(d), lua_gettop(*fb)); /* put to object table. 
											but above object left on stack */
		r = reply_result(c, *fb, 1, rt);/* object returns */
		m_fibers.destroy(fb);
		return r;
	case vmprotocol::rpct_method:
		push_object(*fb, &o);	/* push object */
		n_basestk = 2;
		lua_getfield(*fb, -1, p);/* push function */
		ASSERT(lua_isfunction(*fb, -1));
		lua_pushvalue(*fb, -2);	/* copy object of this method */
		ASSERT(lua_isuserdata(*fb, -1));
		n_args = 1;
		break;
	case vmprotocol::rpct_global:
		lua_getfield(*fb, LUA_GLOBALSINDEX, p);
		ASSERT(lua_isfunction(*fb, -1));
		push_object(*fb, &o);	/* push object */
		n_args = 1;
		break;
	default:
		ASSERT(false);
		lua_pushfstring(*fb, "remote: invalid rpc type (%d)", rt);
		return reply_result(c, *fb, 1, vmprotocol::rpct_error);
	}
	while(unpack_lua_stack(cf, wid, c.sr(), *fb)) {
		n_args++;
		if (n_args > 100) {
			ASSERT(false);
			lua_pushstring(*fb, "remote: too many argument");
			return reply_result(c, *fb, 
					lua_gettop(*fb) - 1, vmprotocol::rpct_error);
		}
	}
	return dispatch(c, *fb, n_args, false, rt);
}

template <class SR, class OF>
int lua<SR,OF>::set_object_type(VM lua)
{
	object *o = to_o(lua, 1);
	if (!o || !lua_isstring(lua, 2)) {
		lua_pushstring(lua, "illegal argument");
		lua_error(lua);
	}
	const char *ts = add_type(lua_tostring(lua, 2));
	if (!ts) {
		lua_pushstring(lua, "add type fail");
		lua_error(lua);
	}
	o->set_type(ts);
	return 0;
}
	
template <class SR, class OF>
const char *lua<SR,OF>::add_type(const char *typestr)
{
	type_id *t = m_types.find(typestr);
	if (t) { return (const char *)*t; }
	else if (!(t = m_types.create(typestr))) {
		ASSERT(false);
		return NULL;
	}
	t->set_type(typestr);
	if (!t->has_table()) {
		lock lk(m_lock, true);
		lua_newtable(m_vm);
		lua_setfield(m_vm, LUA_REGISTRYINDEX, typestr);
		TRACE("create new method table <%s>\n", typestr);
		t->set_has_table();
	}
	return (const char *)t;
}

template <class SR, class OF>
int lua<SR,OF>::get_object_type(VM lua)
{
	object *o = to_o(lua, 1);
	if (!o) {
		lua_pushstring(lua, "illegal argument");
		lua_error(lua);
	}
	lua_pushstring(lua, o->type());
	return 1;
}

template <class SR, class OF>
int lua<SR,OF>::gc(VM lua)
{
	USERDATA *ud = (USERDATA *)lua_touserdata(lua, 0);
	if (!ud) {
		lua_pushstring(lua, "gc illegal object");
		lua_error(lua);
	}
	switch(ud->type) {
	case OT_PFM:
//		OF::erase((object *)ud->o);
		break;
	case OT_RPC:
		m_rpcs.erase(typename array<rpc>::iterator(
			(typename array<rpc>::element *)ud->o));
		break;
	case OT_SESSION:
		break;
	default:
		lua_pushfstring(lua, "gc illegal type (%d)", ud->type);
		lua_error(lua);
	}
	return 0;
}

template <class SR, class OF>
int lua<SR,OF>::create(VM lua)
{
	/* send object creation request to master */
	fiber *f = vm_owner(lua);
	S *c = f->connection();
	if (!c || !c->valid()) {
		lua_pushstring(lua, "invalid fiber : connection invalid");
		lua_error(lua);
	}
	PREPARE_PACK(c, max_rpc_packlen);
	SR &sr = c->sr();
	int top = lua_gettop(lua), r;
	for (int i = 1; i <= top; i++) {
		if ((r = pack_lua_stack(sr, lua, i)) < 0) {
			FINISH_PACK(c);
			lua_pushfstring(lua, "pack lua value fail (%d,%d)", i, r);
			lua_error(lua);
		}
	}
	typename S::querydata *q;
	UUID uuid = OF::new_id();
	if (c->load_or_create_object(0, NULL, uuid, 
		sr.p(), sr.len(), vmprotocol::load_purpose_create, &q) < 0) {
		FINISH_PACK(c);
		lua_pushstring(lua, "send object creation fail");
		lua_error(lua);
	}
	q->m_vm = lua;
	FINISH_PACK(c);
	return lua_yield(lua, 1);
}

template <class SR, class OF>
int lua<SR,OF>::index(VM lua)
{
	object *o = to_o(lua, 1);
	const char *k = to_k(lua, 2);
#if defined(_DEBUG)
	char b[256];
	TRACE("index : o[%s][%s]\n", o->uuid().to_s(b, sizeof(b)), k);
#endif
	if (o->local()) {
		get_object_value(lua, *o, k);
		return 1;
	}
	else {
		ASSERT(lua_type(lua, 3) == -1);
		get_object_value(lua, *o, k);
		USERDATA *ud = (USERDATA *)lua_touserdata(lua, 3);
		if (ud && (ud->type == OT_PFM || ud->type == OT_RPC)) {
			/* if pfm/rpc object set to remote object, then return it */
			return 1;
		}
#if !defined(ENABLE_GETTER)
		/* create rpc object on lua stack */
		rpc *r;
		if (!(r = rpc_new(lua, o, k))) {
			lua_pushstring(lua, "fail to create rpcobj");
			lua_error(lua);
		}
		return 1;	/* return this rpc object */
#else
		fiber *f = vm_owner(lua);
		S *sender = f->connection();
		conn *c = o->connection();
		if (!sender || !sender->valid()) {
			lua_pushstring(lua, "invalid fiber");
			lua_error(lua);
		}
		/* cannot use packer of c: another thread may use it */
		PREPARE_PACK(sender, max_rpc_packlen);
		SR &sr = sender->sr();
		/* pack k into sr() */
		if (pack_lua_stack(sender->sr(), lua, 2) < 0) {
			FINISH_PACK(sender);
			lua_pushstring(lua, "fail to pack lua value");
			lua_error(lua);
		}
		/* call rpc object:_getter_(k)
		function object._getter_(k) return self[k] end */
		ASSERT(strcmp(k, "attack_point") != 0);
		typename conn::querydata *q;
		if (c->send_rpc(*sender, o->uuid(), (proc_id &)*k, sr.p(), sr.len(),
			vmprotocol::rpct_getter, &q) < 0) {
			FINISH_PACK(sender);
			ASSERT(false);
			lua_pushstring(lua, "fail to rpc");
			lua_error(lua);
		}
		q->m_vm = lua;
		FINISH_PACK(sender);
		return lua_yield(lua, 1);
#endif
	}
	return 0;
}

template <class SR, class OF>
int lua<SR,OF>::newindex(VM lua)
{
	object *o = to_o(lua, 1);	/* userdata */
	const char *k = to_k(lua, 2);	/* k */
#if defined(_DEBUG)
	char b[256];
	TRACE("newindex : o[%s][%s] = v\n", o->uuid().to_s(b, sizeof(b)), k);
#endif
	/* object[k] = variable */
	if (o->local()) {
		set_object_value(lua, *o, k, 3);
		return 0;
	}
	/* object[k] = function */
	else if (lua_isfunction(lua, 3)) {
		/* create rpc object on lua stack */
		rpc *r;
		if (!(r = rpc_new(lua, o, k))) {
			lua_pushstring(lua, "fail to create rpcobj");
			lua_error(lua);
		}
		/* object[k] = rpc */
		set_object_method(lua, *o, k, 4/* rpc obj */);
		return 0;	/* no return value */
	}
	else {
		object *v = to_o(lua, 3, false);
		/* not pfm object and o is remote */
		if (!v) {
			/* call object._setter_(k,v)
			function obj:_setter_(k, v) self[k] = v end */
			fiber *f = vm_owner(lua);
			S *sender = f->connection();
			conn *c = o->connection();
			if (!sender || !sender->valid()) {
				lua_pushstring(lua, "invalid fiber");
				lua_error(lua);
			}
			/* cannot use packer of c: another thread may use it */
			PREPARE_PACK(sender, max_rpc_packlen);
			SR &sr = sender->sr();
			/* pack k and v into sr */
			if (pack_lua_stack(sr, lua, 2) < 0 || pack_lua_stack(sr, lua, 3) < 0) {
				FINISH_PACK(sender);
				lua_pushstring(lua, "fail to pack lua value");
				lua_error(lua);
			}
			proc_id id = "";
			typename conn::querydata *q;
			if (c->send_rpc(*sender, o->uuid(), id, sr.p(), sr.len(),
				vmprotocol::rpct_setter, &q) < 0) {
				ASSERT(false);
				FINISH_PACK(sender);
				lua_pushstring(lua, "fail to rpc");
				lua_error(lua);
			}
			q->m_vm = lua;
			FINISH_PACK(sender);
			return lua_yield(lua, 1);
		}
		else {
			TRACE("k=%s is pfm obj %s\n", k, v->uuid().to_s(b,sizeof(b)));
			/* pfm object set to remote object. */
			/* object[k] = pfmobj */
			set_object_value(lua, *o, k, 3);
		}
		return 0;	/* no return variable */
	}
}

template <class SR, class OF>
int lua<SR, OF>::call(VM lua)
{
	/* obj.k(a1,a2,...) -> convert to call(obj,k,a1,a2,....) */
	int n_arg = lua_gettop(lua);
	rpc *r = to_r(lua, 1);  			/* pfm object */
	object *o = r->obj();
	if (!o) {
		lua_pushstring(lua, "object already destroyed");
		lua_error(lua);
	}
	if (o->local()) {
		ASSERT(false);
		push_object(lua, o);			/* push obj */
		/* local object: call __k(obj,a1,a2,...)(= obj.k(a1,a2,...)) */
		lua_getfield(lua, -1, r->proc()); 	/* push obj.k */
		lua_pushvalue(lua, -2);			/* push obj */
		for (int i = 2; i <= n_arg; i++) {
			lua_pushvalue(lua, i);		/* push a1,a2,... */
		}
		n_arg = lua_gettop(lua);		/* preserve previous stack height */
		lua_pcall(lua, n_arg, LUA_MULTRET, 0);	/* function call */
		return lua_gettop(lua) - n_arg;		/* number of return value */
	}
	else {
		/* else: call rpc */
		conn *c = o->connection();		/* connection to real object */
		fiber *f = vm_owner(lua);
		S *sender = f->connection();
		if (!sender || !sender->valid()) {
			lua_pushstring(lua, "invalid fiber");
			lua_error(lua);
		}
		/* cannot use packer of c: another thread may use it */
		PREPARE_PACK(sender, max_rpc_packlen);
		SR &sr = sender->sr();
		/* index 2 is this object. so skip 2 */
		for(int i = 3, res; i <= n_arg; i++) {
			lua_pushvalue(lua, i);
			if ((res = pack_lua_stack(sr, lua, lua_gettop(lua))) < 0) {
				FINISH_PACK(sender);
				lua_pushfstring(lua, "pack lua value fail (%d)", res);
				lua_error(lua);
			}
			lua_pop(lua, 1);
		}
		typename conn::querydata *q;
		if (c->send_rpc(*sender, o->uuid(), r->proc(), sr.p(), sr.len(),
				vmprotocol::rpct_method, &q) < 0) {
			FINISH_PACK(sender);
			ASSERT(false);
			lua_pushstring(lua, "fail to rpc");
			lua_error(lua);
		}
		q->m_vm = lua;
		FINISH_PACK(sender);
		return lua_yield(lua, 1);
	}
}

