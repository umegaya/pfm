#include "sfc.hpp"
#include "nbr_pkt.h"

int session::factory::connect(const char *addr/*= NULL*/, void *p/* = NULL*/)
{
	SOCK sk = nbr_sockmgr_connect(m_skm, addr, p);
	return nbr_sock_valid(sk) ? NBR_OK : NBR_ECONNECT;
}

int session::factory::mcast(const char *addr, char *p, int l)
{
	return nbr_sockmgr_mcast(m_skm, addr, p, l);
}

int session::pingmgr::send(class session &s)
{
	/* disabled ping? */
	if (s.cfg().m_ping_timeo <= 0) { return NBR_OK; }
	if (m_last_msgid != 0) {
		return NBR_OK;	/* last ping not replied yet */
	}
	char work[64];
	PUSH_START(work, sizeof(work));
	PUSH_8((U8)0);
	m_last_msgid = s.msgid();
	PUSH_32(m_last_msgid);
	return s.send(work, PACKED_LEN());
}

int session::pingmgr::recv(class session &s, char *p, int l)
{
	if (*p != 0) {
		return NBR_OK;	/* not ping packet */
	}
	/* disabled ping? */
	if (s.cfg().m_ping_timeo <= 0) { return NBR_OK; }
	U8 cmd;
	U32 msgid;
	POP_START(p, l);
	POP_8(cmd)
	POP_32(msgid);
	if (msgid != m_last_msgid) {
		return NBR_EINVAL;
	}
	return NBR_OK;
}


int session::poll(UTIME ut)
{
	if (valid()) {
		if (cfg().m_ping_timeo > 0 && !ping().validate(*this, ut)) {
			close();
		}
	}
	else if (cfg().m_ld_wait <= 0) {
		return NBR_EINVAL;
	}
	else if ((ut - last_access()) > cfg().m_ld_wait) {
		if (connect(cfg().m_host, cfg().proto_p()) < 0) {
			return NBR_ECONNECT;
		}
		update_access();
	}
	return NBR_OK;
}


