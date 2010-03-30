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
array<typename lua<SR,OF>::rpc> 			lua<SR,OF>::m_rpcs;
template <class SR, class OF> 
map<typename lua<SR,OF>::type_id, char *> 	lua<SR,OF>::m_types;
template <class SR, class OF> 
map<typename lua<SR,OF>::fiber *, U64> 		lua<SR,OF>::m_fbmap;
#if defined(_DEBUG)
template <class SR, class OF>
map<typename lua<SR,OF>::thent, U64>		lua<SR,OF>::m_mmap;
template <class SR, class OF>
RWLOCK						lua<SR,OF>::m_mlk = NULL;
#endif

/* fiber */
template <class SR, class OF>
void lua<SR,OF>::fiber::set_sender(VMMSG &v, typename exit_fn<VMMSG>::type f) 
{
	m_type = (U8)io_type_channel;
	if (!m_sender.vmm) {
		m_sender.vmm = new VMMSG(NULL, NULL, 0);
	}
	*(m_sender.vmm) = v;
	m_exit_fn.vmm = f; 
}

/* lua */
template <class SR, class OF>
int lua<SR,OF>::init(int max_rpc_entry, int max_rpc_ongoing, int n_wkr)
{
	if (!m_rpcs.initialized()) {
		if (!m_rpcs.init(max_rpc_entry, -1, opt_threadsafe | opt_expandable)) {
			return NBR_EMALLOC;
		}
		if (!m_types.init(256, 256, -1, opt_threadsafe | opt_expandable)) {
			return NBR_EMALLOC;
		}
		if (!m_fbmap.init(max_rpc_ongoing * n_wkr, 
				(n_wkr * max_rpc_ongoing) / 10, -1, 
			opt_threadsafe | opt_expandable)) {
			return NBR_EMALLOC;
		}
#if defined(_DEBUG)
		if (!m_mmap.init(10000, 100, -1, opt_threadsafe | opt_expandable)) {
			return NBR_EMALLOC;
		}
		if (!(m_mlk = nbr_rwlock_create())) {
			return NBR_EPTHREAD;
		}
#endif
	}
	if (!m_fibers.init(max_rpc_ongoing, -1, opt_threadsafe)) {
		return NBR_EMALLOC;
	}
	if (!(m_vm = lua_newstate(allocator, this))) {
		return NBR_ESYSCALL;
	}
	lua_settop(m_vm, 0);
	/* load built in library */
	lua_pushcfunction(m_vm, luaopen_base);
	lua_pcall(m_vm, 0, 0, 0);
	/* set panic callback */
	lua_atpanic(m_vm, panic);
	/* load basic library */
	lua_pushcfunction(m_vm, luaopen_base);
	if (0 != lua_pcall(m_vm, 0, 0, 0)) {
		return NBR_ESYSCALL;
	}
	return NBR_OK;
}

template <class SR, class OF>
void lua<SR,OF>::fin()
{
	if (m_vm) {
		lua_close(m_vm);
		m_vm = NULL;
	}
	m_fibers.fin();
	m_fbmap.fin();
	m_rpcs.fin();
	m_types.fin();
}

template <class SR, class OF>
int lua<SR,OF>::init_world(const world_id &wid, const world_id &from, const char *srcfile)
{
	TRACE("%p:%p:init VM for world(%s)\n", this, thread(), wid);
	lua_settop(m_vm, 0);
	lua_getfield(m_vm, LUA_REGISTRYINDEX, wid);
	if (!lua_isnil(m_vm, -1)) {
		lua_pop(m_vm, 1);
		TRACE("%p:%p: already initialized\n", this, thread());
		return NBR_OK;	/* already initialized */
	}
	lua_pop(m_vm, 1);
	if (*from) { /* reuse existing table */
		lua_getfield(m_vm, LUA_REGISTRYINDEX, from); 
		if (!lua_isnil(m_vm, -1)) {
			lua_setfield(m_vm, LUA_REGISTRYINDEX, wid);
			return NBR_OK;
		}
		lua_pop(m_vm, 1);	/* remove nil object */
	}
	/* initialize new env */
	lua_newtable(m_vm);
	/* create basic method table */
	lua_newtable(m_vm);
	lua_setfield(m_vm, -2, "_generic_");
	/* load pfm library table */
	lua_newtable(m_vm);
	/* add API */
	lua_pushcfunction(m_vm, create);
	lua_setfield(m_vm, -2, "new");
	lua_pushcfunction(m_vm, set_object_type);
	lua_setfield(m_vm, -2, "bless");
	lua_pushcfunction(m_vm, get_object_type);
	lua_setfield(m_vm, -2, "typeof");		
	/* register to new world env table */
	lua_setfield(m_vm, -2, "pfm");
	/* copy built in library into this table */
	if (copy_table(m_vm, LUA_GLOBALSINDEX, lua_gettop(m_vm), LUA_TNIL) < 0) {
		return NBR_ESYSCALL;
	}
	/* set metatable */
	lua_newtable(m_vm);
	lua_pushcfunction(m_vm, global_newindex);
	lua_setfield(m_vm, -2, "__newindex");
	lua_setmetatable(m_vm, -2);
	/* register this world env */
	ASSERT(lua_istable(m_vm, -1));
	lua_setfield(m_vm, LUA_REGISTRYINDEX, wid);
	/* load initial module */
	if (load(wid, srcfile) < 0) {
		return NBR_ESYSCALL;
	}
	/* init_object defined? */
	lua_getfield(m_vm, LUA_REGISTRYINDEX, wid);
	lua_getfield(m_vm, -1, "init_object");
	if (!lua_isfunction(m_vm, -1)) {
		ASSERT(false);
		return NBR_EINVAL;
	}
	lua_pop(m_vm, 1);
	lua_getfield(m_vm, -1, "load_player");
	if (!lua_isfunction(m_vm, -1)) {
		ASSERT(false);
		return NBR_EINVAL;
	}
	return NBR_OK;
}

template <class SR, class OF>
int	lua<SR,OF>::add_global_object(CF &cf, const world_id &wid, char *p, size_t l)
{
	data d;
	SR &sr = serializer();
	sr.unpack_start(p, l);
	lua_getfield(m_vm, LUA_REGISTRYINDEX, wid); 
	if (unpack_lua_stack(cf, wid, sr, m_vm, this) < 0) {
		return NBR_EFORMAT;
	}
	ASSERT(lua_isstring(m_vm, -1));
	if (unpack_lua_stack(cf, wid, sr, m_vm, this) < 0) {
		return NBR_EFORMAT;
	}
	ASSERT(lua_isuserdata(m_vm, -1) || lua_isfunction(m_vm, -1));
	TRACE("%p: set %u to <%s>:tbl=%p\n", thread(), 
		lua_type(m_vm, -1), lua_tostring(m_vm, -2), lua_topointer(m_vm, -3));
	/* if global function, its environment will be the table relate with 'wid' */
	if (lua_isfunction(m_vm, -1)) {
		TRACE("set world env table(%s) to global function(%s)\n", 
			wid, lua_tostring(m_vm, -2));
		lua_pushvalue(m_vm, -3);
		lua_setfenv(m_vm, -2);
	}
	/* because if not rawset, it calls global_newindex again. */
	lua_rawset(m_vm, -3);
	lua_pop(m_vm, 1);	/* remove table object */
	return NBR_OK;
}

template <class SR, class OF>
bool lua<SR,OF>::load(const world_id &wid, const char *srcfile)
{
	/* temporary remove metatable from world env */
	lua_getfield(m_vm, LUA_REGISTRYINDEX, wid);	/* 1:world env */
	lua_getmetatable(m_vm, -1);			/* 2:metatable of world env */
	lua_newtable(m_vm);				/* 3:{} */
	lua_setmetatable(m_vm, -3);			/* 3->removed */
	 /* loadfile only load file into lua stack (thus soon it loses)
	  * so need to call this chunk. */        
	if (luaL_loadfile(m_vm, srcfile) != 0) {	/* 3:srcfile func */
		lua_setmetatable(m_vm, -2);
		return NBR_ESYSCALL;
	}
	lua_pushvalue(m_vm, -3);			/* 4:world env */
	lua_setfenv(m_vm, -2);				/* 4->removed */
	if (lua_pcall(m_vm, 0, 0, 0) != 0) {		/* 3->removed */
		/* recover metatable */
		lua_setmetatable(m_vm, -2);
		return NBR_ESYSCALL;
	}
	lua_setmetatable(m_vm, -2);			/* 2->removed */
	ASSERT(lua_gettop(m_vm) == 1);
	return NBR_OK;
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
	lua<SR,OF> *scp = (lua<SR,OF> *)ud;
	if (scp->thread()) {
		if (nbr_thread_get_curid() != nbr_thread_get_id(scp->thread())) {
			TRACE("different thread gc runs %08x, %08x\n", 
				nbr_thread_get_curid(),
				nbr_thread_get_id(scp->thread()));
			ASSERT(false);
		}
	}
	lock lk(m_mlk, false);
//	if (ns > (32 * 1024) || os > (32 * 1024)) {
		TRACE("%08x,%08x: ptr=%p,os=%u,ns=%u", nbr_thread_get_curid(), 
			scp->thread() ? nbr_thread_get_id(scp->thread()) : 0, ptr, os, ns);
//	}
#endif
	if (ns == 0) {
#if defined(_DEBUG)
		if (os > (32 * 1024)) {
			static char correct[32 * 1024];
			memset(correct, 'a', 32 * 1024);
			ASSERT(memcmp(((char *)ptr) + os, correct, 32 * 1024) == 0);
		}
#if 1
		if (scp->thread() && ptr) {
		thent *the = scp->mmap().find(((U64)(U32)ptr));
		THREAD th = the->th;
		ASSERT(!th || ((th == scp->thread()) && (nbr_thread_get_id(th) == nbr_thread_get_curid())));
		}
		if (ptr) {
			scp->mmap().erase(((U64)(U32)ptr));
		}
#endif
#endif
		free(ptr);  /* ANSI define free for NULL ptr never causes any change */
		TRACE("->free %p\n", ptr);
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
		TRACE("->returns %p\n", p);
#if 1
		thent the = { scp->thread() };
		if (!ptr && os == 0) {
			TRACE("check memory address %p,%p\n", ptr, p);
			thent *pthe = m_mmap.find(((U64)(U32)p));
			if (pthe) {
				void *pp = p;
				p = realloc(NULL, ns);
				TRACE("avoid used address: %p->%p\n", pp, p);
			}
			ASSERT(m_mmap.insert(the, ((U64)(U32)p)) != m_mmap.end());
		}
		else if (p != ptr) {
			TRACE("realloced: remove %p, insert %p\n", ptr, p);
			m_mmap.erase(((U64)(U32)ptr));
			ASSERT(!m_mmap.find(((U64)(U32)ptr)));
			ASSERT(m_mmap.insert(the, ((U64)(U32)p)) != m_mmap.end());
		}
#endif
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
	lua_getfield(vm, LUA_ENVIRONINDEX, o.type());
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
	lua_getfield(vm, LUA_ENVIRONINDEX, o.type());
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
#if defined(_DEBUG)
	fiber *f = vm_owner(vm);
	TRACE("get object value : %p, %p, %08x, %08x, %08x, <%s:%s>\n", vm, &o, nbr_thread_get_curid(), nbr_thread_get_id(f->scp()->thread()), nbr_thread_get_id(o.thread()), o.uuid().to_s(buf, sizeof(buf)), key);
	ASSERT(nbr_thread_is_current(f->scp()->thread()));
#endif
	/* push stack to registry[uuid][key] */
	/* search object value table */
	lua_getfield(vm, LUA_REGISTRYINDEX, o.uuid().to_s(buf, sizeof(buf)));
	ASSERT(lua_istable(vm, -1));
#if defined(_DEBUG)
	dump_table(vm, lua_gettop(vm));
#endif
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
	char buf[256];
#if defined(_DEBUG)
        fiber *f = vm_owner(vm);
        TRACE("set object value : %p, %p, %08x, %08x, %08x, <%s:%s>\n", vm, &o, nbr_thread_get_curid(), nbr_thread_get_id(f->scp()->thread()), nbr_thread_get_id(o.thread()), o.uuid().to_s(buf, sizeof(buf)), key);
        ASSERT(nbr_thread_is_current(f->scp()->thread()));
#endif
	ASSERT(index_from > 0);	/* should use lua_gettop or something */
	/* do registery[uuid][key] = value */
	lua_getfield(vm, LUA_REGISTRYINDEX, o.uuid().to_s(buf, sizeof(buf)));
	lua_pushvalue(vm, index_from);
	lua_setfield(vm, -2, key);
	lua_pop(vm, 1);	/* remove object table */
	return 0;
}

template <class SR, class OF>
int		lua<SR,OF>::copy_table(VM vm, int from, int to, int type)
{
	TRACE("copy_table(%u) : %d -> %d\n", type, from, to);
	int cnt = 0;
	ASSERT(lua_istable(vm, from) && lua_istable(vm, to));
	lua_pushnil(vm);	/* push first key (idiom, i think) */
	while(lua_next(vm, from)) {	/* put next key/value on stack */
		const char *k = lua_tostring(vm, -2);
		if (!k) {
			return NBR_EINVAL;
		}
		if (type > 0 && lua_type(vm, -1) != type) {
			continue;
		}
//		TRACE("add element[%s]:%u:%u\n", k, lua_type(vm, -1), lua_gettop(vm));
		lua_setfield(vm, to, k);
		cnt++;
	}
	return cnt;
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
		VM vm, lua<SR,OF> *scp, const object &o, data &d)
{
	int r;
	char buf[256];
	if ((r = put_to_lua_stack(cf, wid, vm, scp, d)) < 0) { return r; }
	lua_setfield(vm, LUA_REGISTRYINDEX, o.uuid().to_s(buf, sizeof(buf)));
	return NBR_OK;
}

template <class SR, class OF>
typename lua<SR,OF>::object *
lua<SR,OF>::unpack_object(SR &sr)
{
	data d;
	if (sr.unpack(d) <= 0) { return NULL; }
	return OF::find(*((UUID *)to_p(elem(d, 1))));
}
	
template <class SR, class OF>
typename lua<SR,OF>::object *
lua<SR,OF>::object_new(CF &cf, const world_id *w, 
		VM vm, lua<SR,OF> *scp, UUID &uuid, SR *sr, bool local)
{
	object *o = OF::find(uuid);
	if (o) {
		/* required non-local object but already object exist locally */
		if (o->local() || o->local() == local) { return o; }
		else { ASSERT(false); return NULL; }
	}
	ASSERT(w && (*w)[0]);
	ASSERT(nbr_thread_is_current(scp->thread()));
	if (!(o = OF::create(uuid, local))) { return NULL; }
	o->set_thread(scp->thread());
	o->set_wid(w);
	TRACE("object_new : thread = %p\n", o->thread());
	char buf[256];
	const char *id = uuid.to_s(buf, sizeof(buf));
	TRACE("try to new object <%s>\n", id);
	vm = vm ? vm : scp->vm();
	lua_getfield(vm, LUA_REGISTRYINDEX, id);
	if (lua_isnil(vm, -1)) {
		static int cnt = 0;
		cnt++;
		lua_newtable(vm);
		lua_setfield(vm, LUA_REGISTRYINDEX, id); /* 2nd table remove from stack */
		lua_getfield(vm, LUA_REGISTRYINDEX, id);
		const void *ptr = lua_topointer(vm, -1);
		TRACE("create new table for <%s>:%u:%p\n", id, cnt, ptr);
		lua_pop(vm, 1);
	}
	lua_pop(vm, 1);/* remove table(or nil) from lua stack */
	if (sr) {
		data d;
		if (sr->unpack(d) <= 0) {
			o->set_connection(NULL);	/* no unpack data specified */
			return o;
		}
		if (unpack_object(cf, *w, vm, scp, d, *o) < 0) {
			OF::destroy(o);
			return NULL;
		}
	}
	return o;
}

template <class SR, class OF>
typename lua<SR,OF>::rpc *
lua<SR,OF>::rpc_new(VM vm, object *o, const char *name)
{
	USERDATA *ud = (USERDATA *)lua_newuserdata(vm, sizeof(USERDATA));
	if (!(ud->o = m_rpcs.create())) {
		return NULL;
	}
	((rpc *)ud->o)->set(o, name);
	ud->type = OT_RPC;
	lua_newtable(vm); /* this is metatable */
	lua_pushcfunction(vm, call);
	lua_setfield(vm, -2, "__call");
	lua_pushcfunction(vm, gc);
	lua_setfield(vm, -2, "__gc");
	lua_setmetatable(vm, -2);
	/* after this function finish, stack size increases by 1.(USERDATA *ud) */
	return (rpc *)ud->o;
}

template <class SR, class OF>
void lua<SR,OF>::fiber_delete(fiber *fb)
{
	ASSERT(fb->scp()->check_fiber(*fb));
	fb->scp()->fibers().destroy(fb);
}

template <class SR, class OF>
template <class V>
typename lua<SR,OF>::fiber *lua<SR,OF>::fiber_new(V &v, rpctype rt,  
		U32 msgid, const world_id *wid, typename fiber::template exit_fn<V>::type fn)
{
	fiber *fb = m_fibers.create();
	if (!fb) { ASSERT(false); return NULL; }
	bool _init = !(fb->operator VM());
	lua_settop(m_vm, 0);
	if (fb->init_from_vm(this, wid, rt, v, msgid, fn)) {
		if (_init) {
			TRACE("insert fbmap: %p,%llx\n", fb, fb->fbkey());
			m_fbmap.insert(fb, fb->fbkey());
			ASSERT(((VM)m_fbmap.find(fb->fbkey())) == (VM)fb);
#if defined(_DEBUG)
			lua_pushthread(*fb);
			lua_newtable(*fb);
			lua_pushcfunction(*fb, gc_fiber);
			lua_setfield(*fb, -2, "__gc");
			lua_setmetatable(*fb, -2);
			lua_settop(*fb, 0);
#endif
		}
		return fb;
	}
	else {
		ASSERT(check_fiber(*fb));
		fiber_delete(fb);
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
int lua<SR,OF>::pack_lua_stack(SR &sr, VM vm, int index)
{
	int r;
	ASSERT(index != 0);
//	TRACE("pack lua value on stack(%d:%d)\n", index, lua_type(vm, index));
	switch(lua_type(vm, index)) {
	case LUA_TNIL:
		sr.pushnil();
		break;
	case LUA_TNUMBER:
		sr << lua_tonumber(vm, index);
		break;
	case LUA_TBOOLEAN:
		sr << (lua_toboolean(vm, index) ? true : false);
		break;
	case LUA_TSTRING:
//		TRACE("type=str: %s\n", lua_tostring(vm, index));
		sr.push_string(lua_tostring(vm, index), lua_objlen(vm, index));
		break;
	case LUA_TTABLE: { /* = map {...} */
		/* convert to real index (because minus index change its meaning
		 * during below iteration) */
		index = (index < 0 ? lua_gettop(vm) + index + 1 : index);
		r = lua_objlen(vm, index);
		ASSERT(index < 30);
		sr.push_map_len(r);
		r = lua_gettop(vm);	/* preserve current stack size to r */
		lua_pushnil(vm);	/* push first key (idiom, i think) */
		while(lua_next(vm, index)) {	/* put next key/value on stack */
			int top = lua_gettop(vm);	/* use absolute index */
			pack_lua_stack(sr, vm, top - 1);	/* pack table key */
			pack_lua_stack(sr, vm, top);	/* pack table value */
			lua_pop(vm, 1);	/* destroy value */
		}
		lua_settop(vm, r);	/* recover stack size */
		} break;
	case LUA_TFUNCTION:	/* = array ( LUA_TFUNCTION, binary_chunk ) */
		/* actually it dumps function which placed in stack top.
		but this pack routine calles top -> bottom and packed stack
		index is popped. so we can assure target function to dump
		is already on the top of stack here. */
		if (!lua_iscfunction(vm, index)) {
			sr.push_array_len(2);
			sr << ((U8)LUA_TFUNCTION);
			lua_pushvalue(vm, index);
			write_func_chunk wfc;
			wfc.sr = &sr;
			wfc.n_write = 0;
			int cp = sr.curpos();
			sr.push_raw_len(max_rpc_packlen);
			r = lua_dump(vm, chunk_sr_writer, &wfc);
			lua_pop(vm, 1);
			sr.rewind(sr.curpos() - cp);
			sr.push_raw_len(wfc.n_write);
			sr.skip(wfc.n_write);
			if (r < 0) { return NBR_EINVAL; }
		} break;
	case LUA_TUSERDATA: { /* = array ( LUA_TUSERDATA, UUID, objectdata ) */
		object *o = to_o(vm, index, false);
		if (o) {
			sr.push_array_len(3);
			sr << ((U8)LUA_TUSERDATA);
			sr.push_raw((char *)(&(o->uuid())), sizeof(UUID));
			/* not pack object value it self */
			if ((r = pack_object(vm, sr, *o, false)) < 0) { return r; }
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
bool lua<SR,OF>::unpack_lua_stack(CF &cf, const world_id &wid, SR &sr, VM vm, 
		lua<SR,OF> *scp)
{
	typename SR::data d;
	TRACE("unpack_lua_stack: before: %u\n", sr.unpack_remain());
	if (sr.unpack(d) <= 0) {
		return false;
	}
	TRACE("unpack_lua_stack: %p,%p: type(d)=%u,%u\n", vm, scp, type(d),
		sr.unpack_remain());
	put_to_lua_stack(cf, wid, vm, scp, d);
	return true;
}
template <class SR, class OF>
int lua<SR,OF>::put_to_lua_stack(CF &cf, const world_id &wid, VM vm, 
			lua<SR,OF> *scp, data &d)
{
	switch(type(d)) {
	case LUA_TNIL:
		lua_pushnil(vm);
		break;
	case LUA_TNUMBER:
		lua_pushnumber(vm, to_i(d));
		break;
	case LUA_TBOOLEAN:
		lua_pushboolean(vm, to_b(d));
		break;
	case LUA_TSTRING:
		lua_pushstring(vm, to_s(d));
		break;
	case LUA_TTABLE:
		lua_newtable(vm);
		for (int i = 0; i < to_maplen(d); i++) {
			put_to_lua_stack(cf,wid,vm,scp,key(d,i));
			put_to_lua_stack(cf,wid,vm,scp,val(d,i));
			lua_settable(vm, -3);
		}
		break;
	case LUA_TFUNCTION:/* = array ( LUA_TFUNCTION, binary_chunk ) */ {
			read_func_chunk rfc;
			rfc.d = &(elem(d, 1));
			rfc.ret = 0;
			if (lua_load(vm, chunk_sr_reader, &rfc, "rfn") < 0) {
				TRACE("lua_load error <%s>\n", lua_tostring(vm, -1));
				return NBR_ESYSCALL;
			}
		} break;
	case LUA_TUSERDATA: {/* = array ( LUA_TUSERDATA, UUID, objectdata ) */
		object *o = object_new(cf, &wid, vm, scp,  
				*((UUID *)to_p(elem(d, 1))), NULL, false);
		if (o) { 
			if (o->remote() && 
				unpack_object(cf, wid, vm, scp, elem(d, 2), *o) < 0) {
				return NBR_EFORMAT;
			}
			push_object(vm, o); 
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
lua<SR,OF>::to_o(VM vm, int index, bool abort)
{
	ASSERT(index != 0);
	USERDATA *ud = (USERDATA *)lua_touserdata(vm, index);
	if (!ud || ud->type != OT_PFM) {
		if (abort) {
			lua_pushstring(vm, "invalid arg");
			lua_error(vm);
		}
		return NULL;
	}
	return (object *)ud->o;
}

template <class SR, class OF>
typename lua<SR,OF>::rpc *
lua<SR,OF>::to_r(VM vm, int index, bool abort)
{
	ASSERT(index != 0);
	USERDATA *ud = (USERDATA *)lua_touserdata(vm, index);
	if (!ud || ud->type != OT_RPC) {
		if (abort) {
			lua_pushstring(vm, "invalid arg");
			lua_error(vm);
		}
		return NULL;
	}
	return (rpc *)ud->o;
}

template <class SR, class OF>
const char *lua<SR,OF>::to_k(VM vm, int index)
{
	ASSERT(index != 0);
	const char *k = lua_tostring(vm, index);
	if (strlen(k) >= max_symbol_len) {
		lua_pushfstring(vm, "too long string (%s)\n", k);
		lua_error(vm);
	}
	return k;
}

template <class SR, class OF>
const char *lua<SR,OF>::chunk_sr_reader(VM vm, void *p, size_t *l)
{
	read_func_chunk *rfc = (read_func_chunk *)p;
	if (rfc->ret) { 
		return NULL;/* already return all data */
	}
	*l = to_rawlen(*(rfc->d));
	return to_p(*(rfc->d));
}

template <class SR, class OF>
int      lua<SR,OF>::chunk_sr_writer(VM vm, const void *p, size_t l, void *ud)
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
lua<SR,OF>::vm_owner(VM vm)
{
//	return fiber::get_owner(m_vm, lua);
	fiber *f = m_fbmap.find(fiber::fbkey_from(vm));
	ASSERT(f && f->operator VM() == vm);
	return f;
}

template <class SR, class OF>
template <class SNDR>
void lua<SR,OF>::fiber_exit_call(fiber &fb, int basestk, int r, SNDR &s, 
			rpctype rt/* rpct_invalid */)
{
	ASSERT(basestk >= 0);
	SR &sr = fb.scp()->serializer();
	dump_stack(fb);
	PREPARE_PACK(sr, max_rpc_packlen);
	int top = lua_gettop(fb);
	ASSERT(top <= 30);
	for (int i = basestk; i <= top; i++) {
		if (pack_lua_stack(sr, fb, i) < 0) {
			break;
		}
	}
	rt = ((rt == vmprotocol::rpct_invalid) ? fb.rt() : rt);
	fb.call_exit(r, s, rt, sr.p(), sr.len());
	FINISH_PACK(sr);
	return;
}


template <class SR, class OF>
int	lua<SR,OF>::reply_result(fiber &fb, int n_basestksz, rpctype rt/* rpc_invalid */)
{
	ASSERT(n_basestksz >= 0);
	dump_stack(fb);
	lua<SR,OF> *scp = fb.scp();
	ASSERT(scp);
	SR &sr = scp->serializer();
	PREPARE_PACK(sr, max_rpc_packlen);
	rt = ((rt == vmprotocol::rpct_invalid) ? fb.rt() : rt);
	int top = lua_gettop(fb);
	for (int i = n_basestksz, r; i <= top; i++) {
		if ((r = pack_lua_stack(sr, fb, i)) < 0) {
			FINISH_PACK(sr);
			return r;
		}
	}
	int r;
	switch(fb.iot()) {
	case fiber::io_type_socket:
		r = fb.connection()->reply_rpc(*(fb.connection()),fb.rmsgid(),sr.p(),sr.len(),rt);
		break;
	case fiber::io_type_channel:
		r = fb.vmmsg().reply_rpc(*(fb.connection()),fb.rmsgid(),sr.p(),sr.len(),rt);
		break;
	}
	FINISH_PACK(sr);
	return r;
}

template <class SR, class OF>
template <class SNDR>
int 	lua<SR,OF>::dispatch(SNDR &c, fiber &fb, 
				int n_args, bool from_resume)
{
	int top = lua_gettop(fb);
	int stksz = (from_resume ? (top - n_args + 1) : (top - n_args));
	TRACE("th=%p:%p:stksz=%u,top,nargs=%u,%u(f=%s),rt=%u\n", 
		fb.scp()->thread(), &fb, stksz, top, n_args, 
		from_resume ? "resume" : "first",fb.rt());
	dump_stack(fb);
	ASSERT(top != 0 || n_args != 0);
	int r = lua_resume(fb, n_args);
	if (r == LUA_YIELD) {
		return NBR_OK /* successfully suspended */;
	}
	else {
		if (r != 0) {
			/* error happen */
			TRACE("fiber execution failure %d <%s>\n", r, lua_tostring(fb, -1));
			if (fb.need_reply()) {
				/* send only remote error message to caller */
				reply_result(fb, lua_gettop(fb), vmprotocol::rpct_error);
			}
			else {
				fiber_exit_call(fb, stksz, NBR_ESYSCALL, c);
			}
			fiber_delete(&fb);
			return NBR_ESYSCALL;
		}
		else {
			/* sucessfully finished */
			if (fb.need_reply()) {
				r = reply_result(fb, stksz);
			}
			else {
				fiber_exit_call(fb, stksz, NBR_OK, c);
				r = NBR_OK;
			}
			TRACE("fiber end : %p\n", &fb);
			/* whatever result is, free fiber for this function call */
			fiber_delete(&fb);
		}
		return r;
	}
}

template <class SR, class OF>
int lua<SR,OF>::pack_object(VM vm, SR &sr, const object &o, bool with_val)
{
	/* object data = ARRAY( STRING(typename), [MAP(object data) (optional)] ) */
	sr.push_array_len(with_val ? 2 : 1);	
	sr.push_string(o.type(), strlen(o.type()));
	if (with_val) {
		return pack_object_value(vm, o, sr);
	}
	return NBR_OK;
}

template <class SR, class OF>
int lua<SR,OF>::unpack_object(CF &cf, const world_id &wid, VM vm, lua<SR,OF> *scp, 
		data &d, object &o)
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
	const char *t = add_type(vm, &wid, to_s(elem(d, 0)));
	if (!t) { return NBR_EEXPIRE; }
	ASSERT(*t);
	o.set_type(t);
	TRACE("object type : <%s>\n", o.type());
	/* unpack object data */
	if (to_alen(d) > 1) {
		return unpack_object_value(cf, wid, vm, scp, o, elem(d, 1));
	}
	return NBR_OK;
}

template <class SR, class OF>
template <class SNDR>
int lua<SR,OF>::resume_create(SNDR &c, CF &cf, int r, const world_id &wid, fiber &fb, 
		UUID &uuid, SR &sr)
{
	if (r < 0) {
		lua_pushfstring(fb, "object create fail (%d)", r);
		return reply_result(fb, lua_gettop(fb), vmprotocol::rpct_error);
	}
	/* create user object (remote) */
	object *o = object_new(cf, &wid, fb, fb.scp(), uuid, &sr, false);
	/* push to stack (with metamethod) */
	push_object(fb, o);
	/* yield! */ 
	return dispatch(c, fb, 1, true);
}

template <class SR, class OF>
template <class SNDR>
int	lua<SR,OF>::resume_proc(SNDR &c, CF &cf, const world_id &wid, 
		fiber &fb, char *args, size_t alen, rpctype rt)
{
	ASSERT(nbr_thread_is_current(m_thrd));
	SR &sr = serializer();
	sr.unpack_start(args, alen);
	int n_args = 0;
	VM vm = (VM)fb;
	switch(rt) {
	case vmprotocol::rpct_error:
		/* unpack error string on stack */
		/* seems vm's GLOBAL == ENVIRONMENT (but now when no function starts,
		 * ENVIRONINDEX is not valid. */
		lua_getfield(vm, LUA_GLOBALSINDEX, "remote_error");
		lua_pushstring(vm, (const char *)c.addr());
		unpack_lua_stack(cf, wid, sr, fb);
		if (!lua_isnil(vm, -2)) {/* if error function is defined */
			lua_pcall(vm, 2, 0, 0);
		}
		return NBR_OK;
	case vmprotocol::rpct_getter:
		if (unpack_lua_stack(cf, wid, sr, fb) < 0) {
			lua_pushnil(vm);
			return dispatch(c, fb, 1, true);
		}
		/* if function type returned, return rpc object instead. */
		if (lua_type(vm, -1) == LUA_TFUNCTION) {
			lua_pop(vm, 1);
			data proc_id, uuid;
			if (sr.unpack(proc_id) <= 0 || sr.unpack(uuid) <= 0) {
				lua_pushnil(vm);
				return dispatch(c, fb, 1, lua_rpc_reply);
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
		return dispatch(c, fb, 1, true);
	case vmprotocol::rpct_method_fw:
	case vmprotocol::rpct_global_fw:
	case vmprotocol::rpct_call:
	case vmprotocol::rpct_call_global: {
		while(unpack_lua_stack(cf, wid, sr, fb)) {
			n_args++;
			if (n_args > 100) {
				ASSERT(false);
				return NBR_EINVAL;
			}
		}
		ASSERT(rt != fb.rt());
		if (fb.need_reply()) {
			return reply_result(fb, 1);
		}
		else {
			fiber_exit_call(fb, 1, NBR_OK, c);
			return NBR_OK;
		}
	}
	case vmprotocol::rpct_setter:
	default:
		while(unpack_lua_stack(cf, wid, sr, fb)) {
			n_args++;
			if (n_args > 100) {
				ASSERT(false);
				return NBR_EINVAL;
			}
		}
		return dispatch(c, fb, n_args, true);
	}
}

template <class SR, class OF>
int lua<SR,OF>::rpc_recver_hook(S &c, U32 rmsgid, const proc_id &pid,
			fiber &fb, object &o, char *p, size_t pl, rpctype rt)
{
	const char *_pid = pid;
	call_attr ca = call_attribute(true, pid, _pid);
	if (ca == CALL_PROTECTED && !c.trust()) {
		char b[256];
		lua_pushfstring(fb, "untrust node call protected routine <%s:%s>",
			(const char *)c.addr(), o.uuid().to_s(b, sizeof(b)));
		reply_result(fb, lua_gettop(fb), vmprotocol::rpct_error);
		return NBR_OK;
	}
	else if (ca == CALL_CLIENT) {
		/* forward this rpc to client which relate with o */
		S *s = c.from_object(o);
		if (!s) {
			lua_pushstring(fb, "client rpc: player already gone");
			reply_result(fb, lua_gettop(fb), vmprotocol::rpct_error);
			return NBR_OK;			
		}
		typename S::querydata *q;
		rpctype rtfw;
		switch(rt) {
		case vmprotocol::rpct_method: 
			rtfw = vmprotocol::rpct_method_fw; break;
		case vmprotocol::rpct_global: 
			rtfw = vmprotocol::rpct_global_fw; break;
		default: ASSERT(false); 
			lua_pushfstring(fb, "client rpc: rpctype invalid (%d)", rt);
			reply_result(fb, lua_gettop(fb), vmprotocol::rpct_error);
			return NBR_OK;
		}
		if (s->send_rpc(c, o.uuid(), (const proc_id &)*_pid, p, pl, rtfw, &q) < 0) {
			lua_pushstring(fb, "forward rpc to client fail");
			reply_result(fb, lua_gettop(fb), vmprotocol::rpct_error);
			return NBR_OK;			
		}
		q->m_fb = &fb;
		ASSERT(fb.scp()->check_fiber(fb));
		return NBR_OK;
	}
	return NBR_ENOTFOUND;
}

template <class SR, class OF>
int lua<SR,OF>::local_call(VMMSG &c, CF &cf, U32 rmsgid, 
		object &o, proc_id &pid, char *p, size_t l, rpctype rt)
{
	data d;
	char b[256];
	fiber *fb;
	int n_args = 0, r;
	ASSERT(nbr_thread_is_current(m_thrd));
	SR &sr = serializer();
	S *rsndr = NULL;
	const world_id *wid = o.wid();
	sr.unpack_start(p, l);
	switch(rt) {
	case vmprotocol::rpct_getter: {
		TRACE("getter called\n");
		ASSERT(o.thread_current());
		if (sr.unpack(d) <= 0) {
			lua_pushstring(m_vm, "remote unpack error");
			goto error;
		}
		TRACE("field name = %s.%s\n", o.uuid().to_s(b, sizeof(b)), to_s(d));
		get_object_value(m_vm, o, to_s(d));
		PREPARE_PACK(sr, max_rpc_packlen);
		if ((r = pack_lua_stack(sr, m_vm, -1)) < 0) {
			FINISH_PACK(sr);
			lua_pushstring(m_vm, "remote pack error");
			goto error;
		}
		r = c.reply_rpc(*rsndr, rmsgid, sr.p(), sr.len(), rt);
		FINISH_PACK(sr);
		return r;
	}
	case vmprotocol::rpct_setter: {
		TRACE("setter called\n");
		ASSERT(o.thread_current());
		if (sr.unpack(d) <= 0) {
			lua_pushstring(m_vm, "remote unpack error");
			goto error;
		}
		TRACE("field name = %s.%s\n", o.uuid().to_s(b, sizeof(b)), to_s(d));
		unpack_lua_stack(cf, *wid, sr, m_vm, this);/* object put on stack */
		set_object_value(m_vm, o, to_s(d), lua_gettop(m_vm)); /* put to object table. 
						but above object left on stack */
		PREPARE_PACK(sr, max_rpc_packlen);
		if ((r = pack_lua_stack(sr, m_vm, -1)) < 0) {
			FINISH_PACK(sr);
			lua_pushstring(m_vm, "remote pack error");
			goto error;
		}
		r = c.reply_rpc(*rsndr, rmsgid, sr.p(), sr.len(), rt);
		FINISH_PACK(sr);
		return r;
	}
	case vmprotocol::rpct_call:
	case vmprotocol::rpct_call_global:
		if (!(fb = fiber_new(c, rt, rmsgid, wid, NULL))) {
			return NBR_EEXPIRE;
		}
		ASSERT(o.thread_current());
		if (rt == vmprotocol::rpct_call) {
			push_object(*fb, &o);		/* push obj */
			lua_getfield(*fb, -1, pid);	/* push method */
			lua_pushvalue(*fb, -2);		/* copy obj */
		}
		else {
			lua_getfield(*fb, LUA_GLOBALSINDEX, pid);
			push_object(*fb, &o);
		}
		TRACE("local_call %s:%p\n", pid, fb);
		n_args = 1;
		while(unpack_lua_stack(cf, *wid, sr, *fb)) {
			n_args++;
			if (n_args > 100) {
				ASSERT(false);
				lua_pushstring(*fb, "remote: too many argument");
				return reply_result(*fb, 
					lua_gettop(*fb), vmprotocol::rpct_error);
			}
		}
		return dispatch(c, *fb, n_args, false);
	default:
		ASSERT(false);
		lua_pushfstring(m_vm, "remote: invalid rpc type (%d)", rt);
	}
error:
	if ((r = pack_lua_stack(sr, m_vm, -1)) < 0) {
		ASSERT(false);
		lua_pushstring(m_vm, "remote pack error");
		return r;
	}
	return c.reply_rpc(*rsndr, rmsgid, sr.p(), sr.len(), vmprotocol::rpct_error);
}

template <class SR, class OF>
int lua<SR,OF>::call_proc(S &c, CF &cf, U32 rmsgid, object &o, proc_id &p,
		char *args, size_t alen, rpctype rt, typename fiber::template exit_fn<S>::type fn)
{
	data d;
	const world_id *wid = o.wid();
	fiber *fb = fiber_new(c, rt, rmsgid, wid, fn);
	if (!fb) {
		return NBR_EEXPIRE;
	}
	int n_args = 0;
	if (rpc_recver_hook(c, rmsgid, p, *fb, o, args, alen, rt) >= 0) {
		return NBR_OK;
	}
	/* if different thread has o, then forward to this thread */
	if (!o.thread_current()) {
		VMMSG vmm(this, thread(), o.thread());
		typename S::querydata *q;
		rpctype lrt;
		switch(rt) {
		case vmprotocol::rpct_method: lrt = vmprotocol::rpct_call; break;
		case vmprotocol::rpct_global: lrt = vmprotocol::rpct_call_global; break;
		default: ASSERT(false);
			lua_pushfstring(*fb, "invalid local call %d", rt);
			return reply_result(*fb, 1, vmprotocol::rpct_error);
		}
		if (vmm.send_rpc(c, o.uuid(), p, args, alen, lrt, &q) < 0) {
			lua_pushstring(*fb, "forward to another thread fail");
			return reply_result(*fb, 1, vmprotocol::rpct_error);
		}
		q->m_fb = fb;
		ASSERT(fb->scp()->check_fiber(*fb));
		//TRACE("forward rpc to another vm (%s:%p:%u)\n", p, fb, rmsgid);
		return NBR_OK;
	}
	ASSERT(nbr_thread_is_current(m_thrd));
	SR &sr = serializer();
	sr.unpack_start(args, alen);
	switch(rt) {
	case vmprotocol::rpct_method:
	case vmprotocol::rpct_method_fw:
		push_object(*fb, &o);	/* push object */
		lua_getfield(*fb, -1, p);/* push function */
		ASSERT(lua_isfunction(*fb, -1));
		lua_pushvalue(*fb, -2);	/* copy object of this method */
		ASSERT(lua_isuserdata(*fb, -1));
		n_args = 1;
		break;
	case vmprotocol::rpct_global:
	case vmprotocol::rpct_global_fw:
		/* seems *fb's GLOBAL == ENVIRONMENT (but now when no function starts,
		 * ENVIRONINDEX is not valid. */
		lua_getfield(*fb, LUA_GLOBALSINDEX, p);
		ASSERT(lua_isfunction(*fb, -1));
		push_object(*fb, &o);	/* push object */
		n_args = 1;
		break;
	default:
		ASSERT(false);
		lua_pushfstring(*fb, "remote: invalid rpc type (%d)", rt);
		return reply_result(*fb, 1, vmprotocol::rpct_error);
	}
	while(unpack_lua_stack(cf, *wid, sr, *fb)) {
		n_args++;
		if (n_args > 100) {
			ASSERT(false);
			lua_pushstring(*fb, "remote: too many argument");
			return reply_result(*fb, 
					lua_gettop(*fb), vmprotocol::rpct_error);
		}
	}
	return dispatch(c, *fb, n_args, false);
}

template <class SR, class OF>
int lua<SR,OF>::set_object_type(VM vm)
{
	object *o = to_o(vm, 1);
	if (!o || !lua_isstring(vm, 2)) {
		lua_pushstring(vm, "illegal argument");
		lua_error(vm);
	}
	const char *ts = add_type(vm, NULL, lua_tostring(vm, 2));
	if (!ts) {
		lua_pushstring(vm, "add type fail");
		lua_error(vm);
	}
	o->set_type(ts);
	return 0;
}
	
template <class SR, class OF>
const char *lua<SR,OF>::add_type(VM vm, const world_id *wid, const char *typestr)
{
	ASSERT(vm || wid);
	type_id *t = m_types.find(typestr);
	if (t) { return (const char *)*t; }
	else if (!(t = m_types.create(typestr))) {
		ASSERT(false);
		return NULL;
	}
	t->set_type(typestr);
	if (!t->has_table()) {
		if (wid) {
			lua_getfield(vm, LUA_REGISTRYINDEX, *wid);
			lua_newtable(vm);
			lua_setfield(vm, -2, typestr);
		}
		else {
			lua_newtable(vm);
			lua_setfield(vm, LUA_ENVIRONINDEX, typestr);
		}
		TRACE("create new method table <%s>\n", typestr);
		t->set_has_table();
	}
	return (const char *)t;
}

template <class SR, class OF>
int lua<SR,OF>::get_object_type(VM vm)
{
	object *o = to_o(vm, 1);
	if (!o) {
		lua_pushstring(vm, "illegal argument");
		lua_error(vm);
	}
	lua_pushstring(vm, o->type());
	return 1;
}

#if defined(_DEBUG)
template <class SR, class OF>
int lua<SR,OF>::gc_fiber(VM)
{
	ASSERT(false);
}
template <class SR, class OF>
void lua<SR,OF>::dump_table(VM vm, int index)
{	
	lua_pushnil(vm);        /* push first key (idiom, i think) */
	printf("table ptr = %p\n", lua_topointer(vm, index));
	while(lua_next(vm, index)) {     /* put next key/value on stack */
		printf("table[%s]=%u\n", lua_tostring(vm, -2), lua_type(vm, -1));
		lua_pop(vm, 1);
	}
}
template <class SR, class OF>
void lua<SR,OF>::dump_stack(VM vm)
{
	int top = lua_gettop(vm);
	TRACE("stack size = %u\n",top);
	for (int i = 1; i <= top; i++) {
		TRACE("[%u]:%u:%p\n", i, lua_type(vm, i), lua_topointer(vm, i));
	}
}
#endif

template <class SR, class OF>
int lua<SR,OF>::gc(VM vm)
{
	USERDATA *ud = (USERDATA *)lua_touserdata(vm, 1);
	if (!ud) {
		lua_pushstring(vm, "gc illegal object");
		lua_error(vm);
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
		lua_pushfstring(vm, "gc illegal type (%d)", ud->type);
		lua_error(vm);
	}
	return 0;
}

template <class SR, class OF>
int lua<SR,OF>::create(VM vm)
{
	/* send object creation request to master */
	fiber *f = vm_owner(vm);
	lua<SR,OF> *scp = f->scp();
	if (!scp) {
		lua_pushstring(vm, "invalid interpreter");
		lua_error(vm);
	}
	SR &sr = scp->serializer();
	PREPARE_PACK(sr, max_rpc_packlen);
	int top = lua_gettop(vm), r;
	for (int i = 1; i <= top; i++) {
		if ((r = pack_lua_stack(sr, vm, i)) < 0) {
			FINISH_PACK(sr);
			lua_pushfstring(vm, "pack lua value fail (%d,%d)", i, r);
			lua_error(vm);
		}
	}
	typename S::querydata *q;
	UUID uuid = OF::new_id();
	switch (f->iot()) {
	case fiber::io_type_socket:
		r = f->connection()->load_or_create_object(0, NULL, uuid, 
			sr.p(), sr.len(), vmprotocol::load_purpose_create, &q);
		break;
	case fiber::io_type_channel:
		r = f->vmmsg().load_or_create_object(0, &(f->wid()), uuid,
			sr.p(), sr.len(), vmprotocol::load_purpose_create, &q);
		break;
	default: ASSERT(false); r = NBR_ENOTFOUND;
		break;
	}
	if (r < 0) {
		FINISH_PACK(sr);
		lua_pushstring(vm, "send object creation fail");
		lua_error(vm);
	}
	q->m_fb = f;
	ASSERT(f->scp()->check_fiber(*f));
	FINISH_PACK(sr);
	return lua_yield(vm, 1);
}

template <class SR, class OF>
int lua<SR,OF>::index(VM vm)
{
	object *o = to_o(vm, 1);
	const char *k = to_k(vm, 2);
#if defined(_DEBUG)
	char b[256];
	TRACE("index : o[%s][%s]\n", o->uuid().to_s(b, sizeof(b)), k);
#endif
	if (o->local()) {
		if (o->thread_current()) {
			get_object_value(vm, *o, k);
			return 1;
		}
		else {
			/* need to call in-memory rpc getter */
			fiber *f = vm_owner(vm);
			lua<SR,OF> *scp = f->scp();
			S *sender = f->connection();
			if (!sender || !sender->valid()) {
				lua_pushstring(vm, "invalid fiber");
				lua_error(vm);
			}
			/* cannot use packer of c: another thread may use it */
			SR &sr = scp->serializer();
			PREPARE_PACK(sr, max_rpc_packlen);
			/* pack k into sr() */
			if (pack_lua_stack(sr, vm, 2) < 0) {
				FINISH_PACK(sr);
				lua_pushstring(vm, "fail to pack lua value");
				lua_error(vm);
			}
			/* call rpc object:_getter_(k)
			function object._getter_(k) return self[k] end */
			ASSERT(strcmp(k, "attack_point") != 0);
			typename conn::querydata *q;
			VMMSG c(scp, scp->thread(), o->thread());
			if (c.send_rpc(*sender, o->uuid(), (proc_id &)*k, sr.p(), sr.len(),
				vmprotocol::rpct_getter, &q) < 0) {
				FINISH_PACK(sr);
				ASSERT(false);
				lua_pushstring(vm, "fail to rpc");
				lua_error(vm);
			}
			q->m_fb = f;
			ASSERT(f->scp()->check_fiber(*f));
			FINISH_PACK(sr);
			return lua_yield(vm, 1);
		}
	}
	else {
		ASSERT(lua_type(vm, 3) == -1);
		get_object_value(vm, *o, k);
		if (!lua_isnil(vm, 3)) {
			/* already have the element which name is k 
			 * we prefer to use it than do rpc */
			return 1;
		}
		else {
			/* create rpc object on lua stack */
			rpc *r;
			if (!(r = rpc_new(vm, o, k))) {
				lua_pushstring(vm, "fail to create rpcobj");
				lua_error(vm);
			}
			set_object_method(vm, *o, k, 4/* rpc object */);
			return 1;	/* return this rpc object */
		}
	}
	return 0;
}

template <class SR, class OF>
int lua<SR,OF>::global_newindex(VM vm)
{
	/* should call from environ table */
	object *o;
	fiber *f = vm_owner(vm);
	lua<SR,OF> *scp = f->scp();
	const char *k = lua_tostring(vm, 2);
	if (!(o = to_o(vm, 3, false))) {
		TRACE("not pfm object. set locally\n");
		lua_rawset(vm, 1);
		return 0;
	}
	SR &sr = scp->serializer();
	PREPARE_PACK(sr, max_rpc_packlen);
	/* TODO : should not use strlen */
	sr.push_string(k, strlen(k));
	if (pack_lua_stack(sr, vm, 3) < 0) {
		FINISH_PACK(sr);
		lua_pushstring(vm, "pack object error");
		lua_error(vm);
	}
	VMMSG c(scp, scp->thread(), NULL/* bcast to all thread */);
	if (c.notify_add_global_object(*(f->connection()), 
			f->wid(), sr.p(), sr.len()) < 0) {
		FINISH_PACK(sr);
		lua_pushstring(vm, "error send packet");
		lua_error(vm);
	}
	/* first set in this vm (need to use rawset so that global_newindex 
	will not call again) */
	lua_rawset(vm, 1);
	FINISH_PACK(sr);
	return 0;
}

template <class SR, class OF>
int lua<SR,OF>::newindex(VM vm)
{
	object *o = to_o(vm, 1);	/* userdata */
	const char *k = to_k(vm, 2);	/* k */
#if defined(_DEBUG)
	char b[256];
	TRACE("newindex : o[%s][%s] = v\n", o->uuid().to_s(b, sizeof(b)), k);
#endif
	if (o->local()) {
		if (o->thread_current()) {
			/* object[k] = variable */
			set_object_value(vm, *o, k, 3);
		}
		else {
			/* if object is assigned to different thread, use in-memory rpc */
			fiber *f = vm_owner(vm);
			lua<SR,OF> *scp = f->scp();
			S *sender = f->connection();
			/* if sender == null, means call from local rpc */
			/* cannot use packer of c: another thread may use it */
			SR &sr = scp->serializer();
			PREPARE_PACK(sr, max_rpc_packlen);
			/* pack k and v into sr */
			if (pack_lua_stack(sr, vm, 2) < 0 || pack_lua_stack(sr, vm, 3) < 0) {
				FINISH_PACK(sr);
				lua_pushstring(vm, "fail to pack lua value");
				lua_error(vm);
			}
			VMMSG c(scp, scp->thread(), o->thread());
			proc_id id = "";
			typename conn::querydata *q;
			if (c.send_rpc(*sender, o->uuid(), id, sr.p(), sr.len(),
				vmprotocol::rpct_setter, &q) < 0) {
				ASSERT(false);
				FINISH_PACK(sr);
				lua_pushstring(vm, "fail to rpc");
				lua_error(vm);
			}
			q->m_fb = f;
			ASSERT(f->scp()->check_fiber(*f));
			FINISH_PACK(sr);
			return lua_yield(vm, 1);
		}
	}
	/* object[k] = function */
	else if (lua_isfunction(vm, 3)) {
		/* we can define function on remote object 
		 * (mainly for server -> client rpc) */
		set_object_method(vm, *o, k, 3);
	}
	else {
		set_object_value(vm, *o, k, 3);
	}
	return 0;
}

template <class SR, class OF>
template <class SNDR>
int lua<SR, OF>::rpc_sender_hook(SNDR &c, const proc_id &p,
			fiber &fb, object &o, SR &sr, rpctype rt)
{
	const char *_p = p;
	call_attr ca = call_attribute(false, p, _p);
	typename conn::querydata *q;
	if (c.send_rpc(*(fb.connection()), 
		o.uuid(), (const proc_id &)*_p, sr.p(), sr.len(), rt, &q) < 0) {
		return NBR_ESEND;
	}
	if (ca == CALL_NOTIFICATION) {
		q->m_fb = NULL;
		lua_pushnil(fb);	/* return nil */
		return 1;
	}
	q->m_fb = &fb;
	ASSERT(fb.scp()->check_fiber(fb));
	return 0;
}

template <class SR, class OF>
int lua<SR, OF>::call(VM vm)
{
	/* obj.k(a1,a2,...) -> convert to call(obj,k,a1,a2,....) */
	int n_arg = lua_gettop(vm);
	rpc *r = to_r(vm, 1);  			/* pfm object */
	object *o = r->obj();
	if (!o) {
		lua_pushstring(vm, "object already destroyed");
		lua_error(vm);
	}
	if (o->local() && o->thread_current()) {
		ASSERT(false);
		push_object(vm, o);			/* push obj */
		/* local object: call __k(obj,a1,a2,...)(= obj.k(a1,a2,...)) */
		lua_getfield(vm, -1, r->proc()); 	/* push obj.k */
		lua_pushvalue(vm, -2);			/* push obj */
		for (int i = 2; i <= n_arg; i++) {
			lua_pushvalue(vm, i);		/* push a1,a2,... */
		}
		n_arg = lua_gettop(vm);		/* preserve previous stack height */
		lua_pcall(vm, n_arg, LUA_MULTRET, 0);	/* function call */
		return lua_gettop(vm) - n_arg;		/* number of return value */
	}
	else {
		/* else: call rpc */
		fiber *f = vm_owner(vm);
		lua<SR,OF> *scp = f->scp();
		/* cannot use packer of c: another thread may use it */
		SR &sr = scp->serializer();
		PREPARE_PACK(sr, max_rpc_packlen);
		/* index 2 is this object. so skip 2 */
		int result;
		for(int i = 3; i <= n_arg; i++) {
			lua_pushvalue(vm, i);
			if ((result = pack_lua_stack(sr, vm, lua_gettop(vm))) < 0) {
				FINISH_PACK(sr);
				lua_pushfstring(vm, "pack lua value fail (%d)", result);
				lua_error(vm);
			}
			lua_pop(vm, 1);
		}
		if (o->local()) {
			VMMSG c(scp, scp->thread(), o->thread());
			result = rpc_sender_hook(c, 
					r->proc(), *f, *o, sr, vmprotocol::rpct_call);
		}
		else {
			result = rpc_sender_hook(*(o->connection()), 
					r->proc(), *f, *o, sr, vmprotocol::rpct_method);
		}
		FINISH_PACK(sr);
		if (result < 0) {
			lua_pushfstring(vm, "rpc_sender fail(%d)", result);
			lua_error(vm);
		}
		else if (result > 0) { return result; }
		return lua_yield(vm, 1);
	}
}

