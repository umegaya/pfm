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
inline lua_Integer
lua_convtype::to_i(mp::data &d)
{
	return d.via.i64;
}

inline lua_Boolean
lua_convtype::to_b(mp::data &d)
{
	return d.via.boolean;
}

inline const char*
lua_convtype::to_s(mp::data &d)
{
	ASSERT(*(d.via.raw.ptr) != LUA_TSTRING);
	return d.via.raw.ptr + 1;
}

inline const char*
lua_convtype::to_p(mp::data &d)
{
	ASSERT(*(d.via.raw.ptr) != LUA_TUSERDATA);
	return d.via.raw.ptr + 1;
}

inline int
lua_convtype::to_rawlen(mp::data &d)
{
	return d.via.raw.size - 1;
}

inline int
lua_convtype::to_maplen(mp::data &d)
{
	return d.via.map.size;
}

inline mp::data &
lua_convtype::key(mp::data &d, int i)
{
	ASSERT(d.via.map.size > ((size_t)i));
	return d.via.map.ptr[i].key;
}

inline mp::data &
lua_convtype::val(mp::data &d, int i)
{
	ASSERT(d.via.map.size > ((size_t)i));
	return d.via.map.ptr[i].val;
}

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
		return LUA_TNUMBER;
	case msgpack::type::MAP:
		return LUA_TTABLE;
	case msgpack::type::RAW:
		// LUA_TFUNCTION, LUA_TSTRING, LUA_TUSERDATA
		return *(d.via.raw.ptr);
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
map<typename lua<SR,OF>::fiber,U32> 	lua<SR,OF>::m_fibers;
template <class SR, class OF> 
lua_State				*lua<SR,OF>::m_vm = NULL;
template <class SR, class OF>
RWLOCK					lua<SR,OF>::m_lock = NULL;
/* lua */
template <class SR, class OF>
int lua<SR,OF>::init(int max_rpc_entry, int max_rpc_ongoing)
{
	if (!m_rpcs.init(max_rpc_entry, opt_threadsafe | opt_expandable)) {
		return NBR_EMALLOC;
	}
	if (!m_fibers.init(max_rpc_ongoing, max_rpc_ongoing / 10, 
		opt_threadsafe | opt_expandable)) {
		return NBR_EMALLOC;
	}
	if (!(m_vm = lua_newstate(allocator, NULL))) {
		return NBR_ESYSCALL;
	}
	if (!(m_lock = nbr_rwlock_create())) {
		return NBR_EPTHREAD;
	}
	lua_settop(m_vm, 0);
	lua_getglobal(m_vm, "pfm");
	lua_pushcfunction(m_vm, create);
	lua_setfield(m_vm, -2, "new");
	return NBR_OK;
}

template <class SR, class OF>
void	*lua<SR,OF>::allocator(void *ud, void *ptr, size_t os, size_t ns)
{
	if (ns == 0) {
		free(ptr);  /* ANSI define free for NULL ptr never causes any change */
		return NULL;
	}
	else {
		/* ANSI defines realloc(NULL,ns) == malloc(ns) */
		return realloc(ptr, ns);
	}
}

template <class SR, class OF>
int 	lua<SR,OF>::get_object_value(VM vm, const object &o, const char *key)
{
	char buf[256];
	lua_getfield(vm, LUA_REGISTRYINDEX, o.uuid().to_s(buf, sizeof(buf)));
	lua_getfield(vm, -1, key);
	return 0;
}

template <class SR, class OF>
int 	lua<SR,OF>::put_object_value(VM vm, const object &o, const char *key, int index_from)
{
	char buf[256];
	lua_getfield(vm, LUA_REGISTRYINDEX, o.uuid().to_s(buf, sizeof(buf)));
	lua_pushvalue(vm, index_from);
	lua_setfield(vm, -2, key);
	return 0;
}

template <class SR, class OF>
int 	lua<SR,OF>::pack_object_value(VM vm, const object &o, SR &sr)
{
	char buf[256];
	lua_getfield(vm, LUA_REGISTRYINDEX, o.uuid().to_s(buf, sizeof(buf)));
	return pack_lua_value(sr, vm, -1);
}

template <class SR, class OF>
int 	lua<SR,OF>::unpack_object_value(S &s, VM vm, const object &o, SR &sr)
{
	char buf[256];
	lua_getfield(vm, LUA_REGISTRYINDEX, o.uuid().to_s(buf, sizeof(buf)));
	return unpack_lua_value(s, sr, vm);
}

template <class SR, class OF>
typename lua<SR,OF>::object *
lua<SR,OF>::object_new(S &s, VM vm, UUID &uuid, SR *sr)
{
	object *o = OF::create(uuid, NULL);
	if (!o) { return NULL; }
	char buf[256];
	const char *id = uuid.to_s(buf, sizeof(buf));
	lock lk(m_lock, true);
	vm = vm ? vm : m_vm;
	lua_getfield(vm, LUA_REGISTRYINDEX, id);
	if (lua_isnil(vm, -1)) {
		lua_newtable(vm);
		lua_setfield(vm, LUA_REGISTRYINDEX, id);
	}
	lua_pop(vm, 1);
	if (sr && unpack_object(s, vm, *sr, *o) < 0) {
		OF::destroy(o);
		return NULL;
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
	lua_pushcfunction(lua, call);
	lua_setfield(lua, -2, "__call");
	lua_pushcfunction(lua, gc);
	lua_setfield(lua, -2, "__gc");
	lua_setmetatable(lua, -2);
	/* after this function finish, stack size increases by 1.(USERDATA *ud) */
	return (rpc *)ud->o;
}

template <class SR, class OF>
typename lua<SR,OF>::fiber *lua<SR,OF>::fiber_new(S *c, U32 msgid)
{
	ASSERT(m_fibers.find(msgid) == NULL);
	fiber *fb = m_fibers.create(msgid);
	if (!fb) { ASSERT(false); return NULL; }
	lock lk(m_lock, true);
	if (fb->init_from_vm(m_vm, c, msgid)) {
		return fb;
	}
	else {
		m_fibers.erase(msgid);
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
int lua<SR,OF>::pack_lua_value(SR &sr, VM lua, int index)
{
	switch(lua_type(lua, index)) {
	case LUA_TNIL:
		sr.pushnil();
		break;
	case LUA_TNUMBER:
		sr << lua_tointeger(lua, index);
		break;
	case LUA_TBOOLEAN:
		sr << (lua_toboolean(lua, index) ? true : false);
		break;
	case LUA_TSTRING:
		sr.push_raw_len(lua_objlen(lua, index) + sizeof(U8));
		sr << ((U8)LUA_TSTRING);
		sr.push_raw(lua_tostring(lua, index), lua_objlen(lua, index));
		break;
	case LUA_TTABLE:
		sr.push_map_len(lua_objlen(lua, index));
		while(lua_next(lua, index)) {
			pack_lua_value(sr, lua, -2);	/* pack table key */
			pack_lua_value(sr, lua, -1);	/* pack table value */
			lua_pop(lua, 2);	/* destroy key/value */
		}
		break;
	case LUA_TFUNCTION:
		/* actually it dumps function which placed in stack top.
		but this pack routine calles top -> bottom and packed stack
		index is popped. so we can assure target function to dump
		is already on the top of stack here. */
		if (!lua_iscfunction(lua, index)) {
			sr.push_raw_len(1);
			sr << ((U8)LUA_TFUNCTION);
			write_chunk wc;
			wc.sr = &sr;
			wc.n_write = 0;
			if (lua_dump(lua, chunk_sr_writer, &sr) < 0) { return -2; }
			sr.rewind(wc.n_write);
			sr.push_raw_len(sizeof(U8) + wc.n_write);
			sr.skip(wc.n_write);
		} break;
	case LUA_TUSERDATA: {
		object *o = to_o(lua, index, false);
		if (o) {
			int r;
			if ((r = pack_object(sr, *o)) < 0) {
				return r;
			}
			sr.push_raw_len(sizeof(UUID) + sr.len() + sizeof(U8));
			sr << ((U8)LUA_TUSERDATA);
			sr.push_raw_onlydata((char *)(&(o->uuid())), sizeof(UUID));
			sr.push_raw_onlydata(sr.p(), sr.len());
		}
		} break;
	case LUA_TTHREAD:
	case LUA_TLIGHTUSERDATA:
		//we never use it.
		ASSERT(false);
		return -1;
	}
	return sr.len();
}

template <class SR, class OF>
int lua<SR,OF>::unpack_lua_value(S &s, SR &sr, VM lua)
{
	typename SR::data d;
	if (sr.unpack(d) <= 0) {
		return 0;
	}
	put_lua_stack(s, lua, d);
	return 1;
}
template <class SR, class OF>
int lua<SR,OF>::put_lua_stack(S &s, VM lua, data &d)
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
			put_lua_stack(s,lua,key(d,i));
			put_lua_stack(s,lua,val(d,i));
			lua_settable(lua, -3);
		}
		break;
	case LUA_TFUNCTION:
		if (lua_load(lua, chunk_sr_reader, &d, "srfunc") < 0) {
			return -1;
		}
		break;
	case LUA_TUSERDATA: {
		SR sr;
		sr.unpack_start(to_p(d) + sizeof(UUID), to_rawlen(d) - sizeof(UUID));
		object *o = object_new(s, lua, *((UUID *)to_p(d)), &sr);
		if (o) { push_object(lua, o); }
		else { return -2; }
		} break;
	case LUA_TTHREAD:
	case LUA_TLIGHTUSERDATA:                //we never use it.
		ASSERT(false);
		return -1;
	}
	return 0;
}

template <class SR, class OF>
typename lua<SR,OF>::object *
lua<SR,OF>::to_o(VM lua, int index, bool abort)
{
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
	USERDATA *ud = (USERDATA *)lua_touserdata(lua, index);
	if (!ud || ud->type != OT_RPC) {
		if (abort) {
			lua_pushstring(lua, "invalid arg");
			lua_error(lua);
		}
	}
	return (rpc *)ud->o;
}

template <class SR, class OF>
const char *lua<SR,OF>::to_k(VM lua, int index)
{
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
	data *d = (data *)p;
	*l = to_rawlen(*d);
	return to_p(*d);
}

template <class SR, class OF>
int      lua<SR,OF>::chunk_sr_writer(VM lua, const void *p, size_t l, void *ud)
{
	write_chunk *wc = (write_chunk *)ud;
	wc->n_write += l;
	return wc->sr->push_raw_onlydata((const char *)p, l);
}

template <class SR, class OF>
typename lua<SR,OF>::fiber *
lua<SR,OF>::vm_owner(VM lua)
{
	lua_getfield(lua, LUA_ENVIRONINDEX, "_fiber_");
	fiber *fb = lua_islightuserdata(lua, lua_gettop(lua)) ? 
		(fiber *)lua_touserdata(lua, lua_gettop(lua)) : NULL;
	lua_pop(lua, 1);
	return fb;
}

template <class SR, class OF>
int	lua<SR,OF>::reply_result(S &c, VM vm, int n_args)
{
	PREPAIR_PACK((&c));
	SR &sr = c.sr();
	int top = lua_gettop(vm);
	for (int i = n_args, r; i < top; i++) {
		if ((r = pack_lua_value(sr, vm, i)) < 0) {
			return r;
		}
	}
	fiber *f = vm_owner(vm);
	return c.reply_rpc(c, f->rmsgid(), sr.p(), sr.len(), vmprotocol::rpct_rpc);
}

template <class SR, class OF>
int 	lua<SR,OF>::dispatch(S &c, VM vm, int n_args, bool need_reply)
{
	int r = lua_resume(vm, n_args);
	if (r == LUA_YIELD) {
		return NBR_OK /* successfully suspended */;
	}
	else {
		fiber *fb = vm_owner(vm);
		if (r != 0) {
			/* error happen */
			m_fibers.erase(fb->rmsgid());
			return NBR_ESYSCALL;
		}
		else {
			/* sucessfully finished */
			r = need_reply ? reply_result(c, vm, n_args) : NBR_OK;
			/* whatever result is, free fiber for this function call */
			m_fibers.erase(fb->rmsgid());
		}
		return r;
	}
}

template <class SR, class OF>
int lua<SR,OF>::pack_object(SR &sr, const object &o)
{
	int r;
	//if ((r = o.connection()->pack(sr, o)) < 0) { return r; }
	const typename OF::chain *c = o.connection()->chain();
	while (c) {
		if ((r = sr.push_raw((const char *)c->m_s->addr(), 
				c->m_s->addr().len())) < 0) {
			return r;
		}
	}
	lock lk(m_lock, true);
	return pack_object_value(m_vm, o, sr);
}

template <class SR, class OF>
int lua<SR,OF>::unpack_object(S &s, VM vm, SR &sr, object &o)
{
	address a;
	typename SR::data d;
	typename OF::conn *c;
	lock lk(m_lock, true);
	while(sr.unpack(d)) {
		a.from(d.via.raw.ptr, d.via.raw.size);
		if ((c = sf(s)->connect(o.uuid(), a))) {
			o.set_connection(c);
		}
		else {
			return NBR_EINVAL;
		}
	}
	return unpack_object_value(s, m_vm, o, sr);
}

template <class SR, class OF>
int lua<SR,OF>::resume_create(S &c, VM vm, UUID &uuid, SR &sr)
{
	/* create user object */
	object *o = object_new(c, vm, uuid, &sr);
	/* push to stack (with metamethod) */
	push_object(vm, o);
	/* yield! */
	return dispatch(c, vm, 1, false);
}

template <class SR, class OF>
int	lua<SR,OF>::resume_proc(S &c, VM vm, char *args, size_t alen, rpctype rt)
{
	c.sr().unpack_start(args, alen);
	int n_args = 0;
	lua_settop(vm, 0);
	switch(rt) {
	case vmprotocol::rpct_getter:
		if (unpack_lua_value(c, c.sr(), vm) < 0) {
			lua_pushnil(vm);
			return dispatch(c, vm, 1, true);
		}
		/* if function type returned, return rpc object instead. */
		if (lua_type(vm, -1) == LUA_TFUNCTION) {
			data proc_id, uuid;
			if (c.sr().unpack(proc_id) < 0 || c.sr().unpack(uuid) < 0) {
				lua_pushnil(vm);
				return dispatch(c, vm, 1, true);
			}
			object *o = OF::find(*((UUID *)to_p(uuid)));
			if (!o) {
				lua_pushnil(vm);
				return dispatch(c, vm, 1, true);
			}
			if (!rpc_new(vm, o, to_s(proc_id))) {
				lua_pushnil(vm);
			}
		}
		return dispatch(c, vm, 1, true);
	case vmprotocol::rpct_setter:
	default:
		while(unpack_lua_value(c, c.sr(), vm)) {
			n_args++;
			if (n_args > 100) {
				ASSERT(false);
				return NBR_EINVAL;
			}
		}
		return dispatch(c, vm, n_args, true);
	}
}

template <class SR, class OF>
int lua<SR,OF>::call_proc(S &c, U32 rmsgid, object &o, proc_id &p,
		char *args, size_t alen, rpctype rt)
{
	fiber *fb = fiber_new(&c, rmsgid);
	if (!fb) {
		return NBR_EEXPIRE;
	}
	c.sr().unpack_start(args, alen);
	switch(rt) {
	case vmprotocol::rpct_getter: {
			data d;
			push_object(*fb, &o);	/* push object */
			if (c.sr().unpack(d) < 0) {
				return NBR_EFORMAT;
			}
			lua_getfield(*fb, -1, to_s(d));
			if (lua_type(*fb, -1) == LUA_TFUNCTION) {
				lua_pushstring(*fb, p);
				lua_pushvalue(*fb, 1);	/* copy object on top of stack */
				return reply_result(c, *fb, 3);
			}
			return reply_result(c, *fb, 1);	/* return value only (except function)
							value, proc_id, uuid (when function)*/
		} break;
	case vmprotocol::rpct_setter: {
			data d;
			push_object(*fb, &o);	/* push object */
			unpack_lua_value(c, c.sr(), *fb);
			if (c.sr().unpack(d) < 0) {
				return NBR_EFORMAT;
			}
			lua_setfield(*fb, -2, to_s(d));
			return reply_result(c, *fb, 1);
		} break;
	default:
		lua_pushstring(*fb, p);	/* push function name */
		push_object(*fb, &o);	/* push object */
		int n_args = 1;
		while(unpack_lua_value(c, c.sr(), *fb)) {
			n_args++;
			if (n_args > 100) {
				ASSERT(false);
				return NBR_EINVAL;
			}
		}
		return dispatch(c, *fb, n_args, true);
	}
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
	typename conn::querydata *q;
	if (c->backend_conn()->send_new_object(*c, "", 0, OF::new_id(), &q) < 0) {
		lua_pushstring(lua, "send object creation fail");
		lua_error(lua);
	}
	q->m_vm = lua;
	return lua_yield(lua, 1);
}

template <class SR, class OF>
int lua<SR,OF>::index(VM lua)
{
	object *o = to_o(lua, 0);
	const char *k = to_k(lua, 1);
	if (o->local()) {
		get_object_value(lua, *o, k);
		return 1;
	}
	else {
		lua_getfield(lua, 0, k);
		USERDATA *ud = (USERDATA *)lua_touserdata(lua, 2);
		if (ud && (ud->type == OT_PFM || ud->type == OT_RPC)) {
			/* if pfm/rpc object set to remote object, then return it */
			return 1;
		}
		else {/* pop normal object value */
			lua_pop(lua, 1);
		}
		fiber *f = vm_owner(lua);
		S *sender = f->connection();
		conn *c = o->connection();
		if (!sender || sender->valid()) {
			lua_pushstring(lua, "invalid fiber");
			lua_error(lua);
		}
		/* cannot use packer of c: another thread may use it */
		PREPAIR_PACK(sender);
		SR &sr = sender->sr();
		if (pack_lua_value(sender->sr(), lua, 1) < 0) {
			lua_pushstring(lua, "fail to pack lua value");
			lua_error(lua);
		}
		/* call rpc object:_getter_(k)
		function object._getter_(k) return self[k] end */
		proc_id id = "";
		typename conn::querydata *q;
		if (c->send_rpc(*sender, o->uuid(), id, sr.p(), sr.len(),
			vmprotocol::rpct_getter, &q) < 0) {
			ASSERT(false);
			lua_pushstring(lua, "fail to rpc");
			lua_error(lua);
		}
		q->m_vm = lua;
		return lua_yield(lua, 1);
	}
	return 0;
}

template <class SR, class OF>
int lua<SR,OF>::newindex(VM lua)
{
	object *o = to_o(lua, 0);	/* userdata */
	const char *k = to_k(lua, 1);	/* k */
	if (o->local()) {
		put_object_value(lua, *o, k, 2);
		return 0;
	}/* object[k] = function */
	else if (lua_isfunction(lua, 2)) {
		/* create rpc object on lua stack */
		rpc *r;
		if (!(r = rpc_new(lua, o, k))) {
			lua_pushstring(lua, "fail to create rpcobj");
			lua_error(lua);
		}
		/* object[k] = rpc */
		lua_setfield(lua, -4, k);
		return 0;	/* no return value */
	}
	/* object[k] = variable */
	else {
		object *v = to_o(lua, 2, false);
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
			PREPAIR_PACK(sender);
			SR &sr = sender->sr();
			/* recv side of rpc unpack variables invert order,
			 * so pack order should inverted */
			if (pack_lua_value(sr, lua, 2) < 0 || pack_lua_value(sr, lua, 1) < 0) {
				lua_pushstring(lua, "fail to pack lua value");
				lua_error(lua);
			}
			proc_id id = "";
			typename conn::querydata *q;
			if (c->send_rpc(*sender, o->uuid(), id, sr.p(), sr.len(),
				vmprotocol::rpct_setter, &q) < 0) {
				ASSERT(false);
				lua_pushstring(lua, "fail to rpc");
				lua_error(lua);
			}
			q->m_vm = lua;
			return lua_yield(lua, 1);
		}
		else {
			/* pfm object set to remote object. */
			lua_setfield(lua, -3, k);
		}
		return 0;	/* no return variable */
	}
}

template <class SR, class OF>
int lua<SR, OF>::call(VM lua)
{
	/* obj.k(a1,a2,...) -> convert to call(obj,k,a1,a2,....) */
	int n_arg = lua_gettop(lua);
	rpc *r = to_r(lua, 0);  			/* pfm object */
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
		push_object(lua, o);			/* push obj */
		for (int i = 1; i < n_arg; i++) {
			lua_pushvalue(lua, i);		/* push a1,a2,... */
		}
		n_arg = lua_gettop(lua);		/* preserve previous stack height */
		lua_call(lua, n_arg, LUA_MULTRET);	/* function call */
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
		PREPAIR_PACK(sender);
		SR &sr = sender->sr();
		for(int i = 1, res; i < n_arg; i++) {
			lua_pushvalue(lua, i);
			if ((res = pack_lua_value(sr, lua, lua_gettop(lua))) < 0) {
				lua_pushfstring(lua, "pack lua value fail (%d)", r);
				lua_error(lua);
			}
			lua_pop(lua, 1);
		}
		typename conn::querydata *q;
		if (c->send_rpc(*sender, o->uuid(), r->proc(), sr.p(), sr.len(),
				vmprotocol::rpct_rpc, &q) < 0) {
			ASSERT(false);
			lua_pushstring(lua, "fail to rpc");
			lua_error(lua);
		}
		q->m_vm = lua;
		return lua_yield(lua, 1);
	}
}

