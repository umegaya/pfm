#include "fiber.h"
#include "world.h"
#include "object.h"

using namespace pfm;

NBR_TLS ll *ffutil::m_vm = NULL;
NBR_TLS serializer *ffutil::m_sr = NULL;

/* ffutil */
int ffutil::init(int max_node, int max_replica) {
	m_max_node = max_node;
	m_max_replica = max_replica;
	return m_fm.init(m_max_rpc, m_max_rpc, -1, opt_threadsafe | opt_expandable) ? 
		NBR_OK : NBR_EMALLOC;
}

bool ffutil::init_tls()
{
	if (m_vm) { return true; }
	if (!(m_sr = new serializer)) {
		fin_tls();
		ASSERT(false);
		return false;
	}
	if (!(m_vm = new ll(m_of, m_wf, *m_sr, m_seed))) {
		fin_tls();
		ASSERT(false);
		return false;
	}
	if (m_vm->init(m_max_rpc) < 0) {
		fin_tls();
		ASSERT(false);
		return false;
	}
	TRACE("ffutil:vm=%p/sr=%p\n", m_vm, m_sr);
	return true;
}

void ffutil::fin()
{
	m_fm.fin();
}

void ffutil::fin_tls()
{
	if (m_vm) {
		delete m_vm;
		m_vm = NULL;
	}
	if (m_sr) {
		delete m_sr;
		m_sr = NULL;
	}
}

world *ffutil::world_new(const rpc::create_world_request &req)
{
	world *w = m_wf.create(req.wid(), m_max_node, m_max_replica);
	if (!w) { return NULL; }
	if (m_vm->init_world(req.wid(), req.from(), req.srcfile()) < 0) {
		world_destroy(w);
		return NULL;
	}
	w->set_world_object_uuid(req.world_object_id());
	for (int i = 0; i < req.n_node(); i++) {
		if (!w->add_node(req.addr(i))) {
			world_destroy(w);
			return NULL;
		}
		LOG("add node (%s) for (%s)\n", (const char *)req.addr(i), w->id());
	}
	return w;
}

void ffutil::world_destroy(const class world *w)
{
	m_vm->fin_world(w->id());
	m_wf.destroy(w->id());
}

world *ffutil::find_world(world_id wid)
{
	return m_wf.find(wid);
}


/* fiber (servant mode) */
int fiber::call_create_world(rpc::request &req)
{
	int r = NBR_OK;
	world *w;
	PREPARE_PACK(ff().sr());
	switch(m_status) {
	case start: {
		rpc::create_world_request &cw = rpc::create_world_request::cast(req);
		if (!(w = ff().world_new(cw))) {
			r = NBR_EEXPIRE;
			break;
		}
		MSGID msgid = new_msgid();
		if (msgid == INVALID_MSGID) {
			r = NBR_EEXPIRE;
			ff().world_destroy(w);
			break;
		}
		if ((r = rpc::create_object_request::pack_header(
			ff().sr(), msgid, cw.world_object_id(),
			ll::world_klass_name, ll::world_klass_name_len,
			cw.wid(), cw.wid().len(), 0)) < 0) {
			ff().world_destroy(w);
			break;
		}
		if ((r = ff().of().request_create(cw.world_object_id(), ff().sr())) < 0) {
			ff().world_destroy(w);
			break;
		}}
		break;
	case create_world_initialize:
		break;
	}
	return r;
}

int fiber::resume_create_world(rpc::response &res)
{
	int r;
	world *w; object *o; ll::coroutine *co;
	switch(m_status) {
	case start: {
		rpc::create_object_response &cor = rpc::create_object_response::cast(res);
		if (!cor.success() || !(w = ff().find_world(wid()))) {
			r = w ? (int)cor.err() : NBR_ENOTFOUND;
		}
		else {
			PREPARE_PACK(ff().sr());
			/* add this object as global variable */
			if (!(o = ff().of().load(w->world_object_uuid(),
				w, ff().vm(), ll::world_klass_name))) {
				r = NBR_EEXPIRE;
				break; 
			}
			ll::coroutine *co = ff().co_create(this);
			if (!co) { r = NBR_EEXPIRE; break; }
			else if ((r = co->push_world_object(o)) < 0) {
				break;
			}
			ff().vm()->co_destroy(co);
			rpc::create_world_response::pack_header(ff().sr(), m_msgid,
				wid(), nbr_str_length(wid(), max_wid), w->world_object_uuid());
			if ((r = respond(false, ff().sr())) < 0) {
				break;
			}
			finish();
			return NBR_OK;
		} }
		break;
	case create_world_initialize:
		break;
	}
	if (r < 0) {
		send_error(r);
		finish();
		if (w) { ff().world_destroy(w); }
		if (o) { ff().of().destroy(o->uuid()); }
		if (co) { ff().vm()->co_destroy(co); }
	}
	return r;
}
