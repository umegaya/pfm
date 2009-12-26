/***************************************************************
 * http.cpp : implementation of httpsession class (mainly HTTP state machine)
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
#include "http.h"
#include <ctype.h>
#include "nbr_pkt.h"
#include "common.h"
#include "str.h"
#include "mem.h"

/*-------------------------------------------------------------*/
/* sfc::httpsession::fsm									   */
/*-------------------------------------------------------------*/
void
httpsession::fsm::reset(const config &cfg)
{
	m_buf = m_p;
	m_len = 0;
	m_max = cfg.m_rbuf;
	m_ctx.version = version_1_1;
	m_ctx.n_hd = 0;
	m_ctx.state = state_recv_header;
}

httpsession::fsm::state
httpsession::fsm::append(char *b, int bl)
{
	TRACE("append %u byte\n", bl);
	state s = status();
	char *w = b;
	U32 limit = (m_max - 1);
	while (s != state_error && s != state_recv_finish) {
		if (m_len >= limit) {
			s = state_error;
			break;
		}
		m_p[m_len++] = *w++;
		m_p[m_len] = '\0';
		switch(s) {
		case state_recv_header:
			s = recv_header(); break;
		case state_recv_body:
			s = recv_body(); break;
		case state_recv_body_nochunk:
			s = recv_body_nochunk(); break;
		case state_recv_bodylen:
			s = recv_bodylen(); break;
		case state_recv_footer:
			s = recv_footer(); break;
		case state_recv_comment:
			s = recv_comment(); break;
		default:
			break;
		}
		if ((w - b) >= bl) { break; }
	}
	recvctx().state = (U16)s;
	return s;
}

char*
httpsession::fsm::hdrstr(const char *key, char *b, int l) const
{
	for (int i = 0; i < m_ctx.n_hd; i++) {
		const char *k = key;
		const char *p = m_ctx.hd[i];
		/* key name comparison by case non-sensitive */
		while (*k && tolower(*k) == tolower(*p)) {
			if ((k - key) > m_ctx.hl[i]) {
				ASSERT(false);
				return NULL;	/* key name too long */
			}
			k++; p++;
		}
		if (*k) {
			continue;	/* key name and header tag not match */
		}
		else {
			/* seems header is found */
			while (*p) {
				/* skip [spaces][:][spaces] between [tag] and [val] */
				if (*p == ' ' || *p == ':') { p++; }
				else { break; }
				if ((m_ctx.hd[i] - p) > m_ctx.hl[i]) {
					ASSERT(false);
					return NULL;	/* too long space(' ') */
				}
			}
			char *w = b;
			while (*p) {
				*w++ = *p++;
				if ((w - b) >= l) {
					ASSERT(false);
					return NULL;	/* too long header paramter */
				}
			}
			*w = 0;	/* null terminate */
			return b;
		}
	}
	return NULL;
}

int
httpsession::fsm::hdrint(const char *key, int &out) const
{
	char b[256];
	if (NULL != hdrstr(key, b, sizeof(b))) {
		int r;
		if ((r = nbr_str_atoi(b, &out, 256)) < 0) {
			return r;
		}
		return NBR_OK;
	}
	return NBR_ENOTFOUND;
}

int
httpsession::fsm::recv_lf() const
{
	const char *p = current();
//	if (m_len > 1) {
//		TRACE("now last 2byte=<%s:%u>%u\n", (p - 2), GET_16(p - 2), htons(crlf));
//	}
	if (m_len > 2 && GET_16(p - 2) == htons(crlf)) {
		return 2;
	}
	if (m_len > 1 && *(p - 1) == '\n') {
		return 1;
	}
	return 0;
}

int
httpsession::fsm::recv_lflf() const
{
	const char *p = current();
	if (m_len > 4 && GET_32(p - 4) == htonl(crlfcrlf)) {
		return 4;
	}
	if (m_len > 2 && GET_16(p - 2) == htons(lflf)) {
		return 2;
	}
	return 0;
}

httpsession::fsm::state
httpsession::fsm::recv_header()
{
	char *p = current();
	int nlf, tmp;
	if ((nlf = recv_lf())) {
		/* lf found but line is empty. means \n\n or \r\n\r\n */
		tmp = nlf;
		for (;tmp > 0; tmp--) {
			*(p - tmp) = '\0';
		}
		if ((p - nlf) == m_buf) {
			int cl; char tok[8];
			/* get result code */
			m_ctx.res = putrc();
			/* if content length is exist, no chunk encoding */
			if (hdrint("Content-Length", cl) >= 0) {
				recvctx().bd = p;
				recvctx().bl = cl;
				return state_recv_body_nochunk;
			}
			/* if chunk encoding, process as chunk */
			else if (hdrstr("Transfer-Encoding", tok, sizeof(tok)) != NULL &&
						nbr_mem_cmp(tok, "chunked", sizeof("chunked") - 1) == 0) {
				m_buf = recvctx().bd = p;
				return state_recv_bodylen;
			}
			else if (rc() == HRC_OK){
				return state_error;
			}
			else { return state_recv_finish; }
		}
		/* lf found. */
		else if (recvctx().n_hd < MAX_HEADER) {
			recvctx().hd[recvctx().n_hd] = m_buf;
			recvctx().hl[recvctx().n_hd] = (p - m_buf) - nlf;
			m_buf = p;
			recvctx().n_hd++;
		}
		else {	/* too much header. */
			return state_error;
		}
	}
	return state_recv_header;
}

httpsession::fsm::state
httpsession::fsm::recv_body()
{
	int nlf;
	if ((nlf = recv_lf())) {
		/* some stupid web server contains \n in its response...
		 * so we check actual length is received */
		int n_diff = (recvctx().bd + recvctx().bl) - (m_p + m_len - nlf);
		if (n_diff > 0) {
			/* maybe \r\n will come next */
			return state_recv_body;
		}
		else if (n_diff < 0) {
			/* it should not happen even if \n is contained */
			return state_error;
		}
		m_len -= nlf;
		m_buf = current();
		return state_recv_bodylen;
	}
	return state_recv_body;
}

httpsession::fsm::state
httpsession::fsm::recv_body_nochunk()
{
	int nlf;
	if ((nlf = recv_lflf())) {
		char *p = current();
		for (;nlf > 0; nlf--) {
			*(p - nlf) = '\0';
		}
		return state_recv_finish;
	}
	return state_recv_body_nochunk;
}

httpsession::fsm::state
httpsession::fsm::recv_bodylen()
{
	char *p = current();
	state s = state_recv_bodylen;

	int nlf;
	if ((nlf = recv_lf())) {
		s = state_recv_body;
	}
	else if (*p == ';') {
		/* comment is specified after length */
		nlf = 1;
		s = state_recv_comment;
	}
	if (s != state_recv_bodylen) {
		int cl;
		for (;nlf > 0; nlf--) {
			*(p - nlf) = '\0';
		}
		if (nbr_str_htoi(m_buf, &cl, (p - m_buf)) < 0) {
			return state_error;
		}
		/* 0-length chunk means chunk end -> next footer */
		if (cl == 0) {
			m_buf = p;
			return state_recv_footer;
		}
		recvctx().bl += cl;
		m_len -= (p - m_buf);
	}
	return s;
}

httpsession::fsm::state
httpsession::fsm::recv_footer()
{
	char *p = current();
	int nlf, tmp;
	if ((nlf = recv_lf())) {
		tmp = nlf;
		for (;tmp > 0; tmp--) {
			*(p - tmp) = '\0';
		}
		/* lf found but line is empty. means \n\n or \r\n\r\n */
		if ((p - nlf) == m_buf) {
			return state_recv_finish;
		}
		/* lf found. */
		else if (recvctx().n_hd < MAX_HEADER) {
			recvctx().hd[recvctx().n_hd] = m_buf;
			recvctx().hl[recvctx().n_hd] = (p - m_buf) - nlf;
			*p = '\0';
			m_buf = p;
			recvctx().n_hd++;
		}
		else {	/* too much footer + header. */
			return state_error;
		}
	}
	return state_recv_footer;
}

httpsession::fsm::state
httpsession::fsm::recv_comment()
{
	int nlf;
	if ((nlf = recv_lf())) {
		char *p = current();
		m_len -= (p - m_buf);
		return state_recv_body;
	}
	return state_recv_comment;
}

int
httpsession::fsm::bodysent(int len)
{
	if ((int)m_len < len) {
		len = m_len;
		m_len = 0;
	}
	else {
		m_len -= len;
	}
	m_buf += len;
	return m_len;
}

void
httpsession::fsm::setrc_from_close_reason(int reason)
{
	switch(reason) {
	case CLOSED_BY_INVALID:
		setrc(HRC_SERVER_ERROR); break;
	case CLOSED_BY_REMOTE:
		break;	/* may success? */
	case CLOSED_BY_APPLICATION:
		setrc(HRC_BAD_REQUEST); break;
	case CLOSED_BY_ERROR:
		setrc(HRC_BAD_REQUEST); break;
	case CLOSED_BY_TIMEOUT:
		setrc(HRC_REQUEST_TIMEOUT); break;
	default:
		ASSERT(false);
		break;
	}
}


httprc
httpsession::fsm::putrc()
{
	const char *w = m_ctx.hd[0], *s = w;
	w += 5;	/* skip first 5 character (HTTP/) */
	if (nbr_mem_cmp(w, "1.1", sizeof("1.1") - 1) == 0) {
		m_ctx.version = 11;
		w += 3;
	}
	else if (nbr_mem_cmp(w, "1.0", sizeof("1.0") - 1) == 0) {
		m_ctx.version = 10;
		w += 3;
	}
	else {
		return HRC_ERROR;
	}
	char tok[256];
	char *t = tok;
	while(*w) {
		w++;
		if (*w != ' ') { break; }
		if ((w - s) > m_ctx.hl[0]) {
			return HRC_ERROR;
		}
	}
	while(*w) {
		if (*w == ' ') { break; }
		*t++ = *w++;
		if ((w - s) > m_ctx.hl[0]) {
			return HRC_ERROR;
		}
		if ((unsigned int )(t - tok) >= sizeof(tok)) {
			return HRC_ERROR;
		}
	}
	int sc;
	*t = '\0';
	if (nbr_str_atoi(tok, &sc, sizeof(tok)) < 0) {
		return HRC_ERROR;
	}
	return (httprc)sc;
}



/*-------------------------------------------------------------*/
/* sfc::httpsession											   */
/*-------------------------------------------------------------*/
void
httpsession::fin()
{
}

int
httpsession::poll(UTIME nt, bool from_worker)
{
	if (from_worker && m_fsm.status() == fsm::state_send_body) {
		if ((nt - last_access()) >= (10 * 1000)) {
			int r = send_body();
			if (r < 0) {
				if (send_result_code(HRC_SERVER_ERROR, 1) < 0) {
					close();
				}
			}
			else if (r == 0) {
				m_fsm.reset(m_f->cfg());
			}
			update_access();
		}
	}
	return session::poll(nt, from_worker);
}

int
httpsession::on_open(const config &cfg)
{
	return NBR_OK;
}

int
httpsession::on_close(int r)
{
	return NBR_OK;
}

char*
httpsession::host(char *buff, int len) const
{
	char addr[256];
	const char *p = remoteaddr(addr, sizeof(addr));
	if (*p == '\0') {
		return "";
	}
	if (!nbr_str_divide_tag_and_val(':', addr, buff, len)) {
		return "";
	}
	return buff;
}

int
httpsession::get(const char *url, const char *hd[], const char *hv[],
		int n_hd, int chunked)
{
	return send_request_common("GET", url, "", 0, hd, hv, n_hd, chunked);
}

int
httpsession::post(const char *url, const char *hd[], const char *hv[],
		const char *body, int blen, int n_hd, int chunked/* = 0 */)
{
	return send_request_common("POST", url, body, blen, hd, hv, n_hd, chunked);
}

int
httpsession::send_request_common(const char *method,
		const char *url, const char *body, int blen,
		const char *hd[], const char *hv[], int n_hd, int chunked/* = 0 */)
{
	char data[1024], host[1024], path[1024];
	U16 port;
	int r, len;
	if ((r = nbr_str_parse_url(url, sizeof(host), host, &port, path)) < 0) {
		return r;
	}
	if (path[0] == '\0') {
		len = nbr_str_printf(data, sizeof(data),
				"%s %s HTTP/1.1\r\n"
				"Host: %s\r\n",
				method, path, remoteaddr(host, sizeof(host)));
	}
	else {
		len = nbr_str_printf(data, sizeof(data),
				"%s %s HTTP/1.1\r\n"
				"Host: %s:%hu\r\n",
				method, path, host, port);
	}
	if ((len = send(data, len)) < 0) {
		log(LOG_ERROR, "send request fail (%d)\n", len);
		return len;
	}
	TRACE("request header: <%s", data);
	if ((r = send_header(hd, hv, n_hd, chunked)) < 0) {
		log(LOG_ERROR, "send emptyline1 fail (%d)\n", r);
		return r;
	}
	len += r;
	if ((r = send("\r\n", 2)) < 0) {
		return r;
	}
	len += r;
	TRACE(":%u>\n", len);
	if (chunked) {
		TRACE("body transfer as chunk (%d)\n", blen);
		m_fsm.set_send_body(body, blen);
		return len + blen;
	}
	else if (blen > 0) {
		if ((r = send(body, blen)) < 0) {
			log(LOG_ERROR, "send body fail (%d)\n", r);
			return r;
		}
		len += r;
		if ((r = send("\r\n\r\n", 4)) < 0) {
			log(LOG_ERROR, "send last crlfcrlf fail (%d)\n", r);
			return r;
		}
		len += r;
	}
	return len;
}

int
httpsession::send_body()
{
	char length[16];
	int n_send = m_fsm.send_body_len();
	int n_limit = (writable()
			- 8/* chunk max is FFFFFFFF */ - 2/* for last \r\n */);
	if (n_send > n_limit) {
		n_send = n_limit;
	}
	int n_length = nbr_str_printf(length,
			sizeof(length) - 1, "%08x\r\n", n_send);
	if ((n_length = send(length, n_length)) < 0) {
		return n_length;
	}
	if (n_send <= 0) { /* chunk transfer but body is 0 byte */
		if ((n_send = send("\r\n", 2)) < 0) {/* send last empty line */
			return n_send;
		}
		return 0;
	}
	if ((n_send = send(m_fsm.send_body_ptr(), n_send)) < 0) {
		return n_send;
	}
	m_fsm.bodysent(n_send);
	if ((n_send = send("\r\n", 2)) < 0) {
		return n_send;
	}
	if (m_fsm.bodylen() == 0) {
		/* send last 0 byte chunk and empty line */
		if ((n_send = send("0\r\n\r\n", 5)) < 0) {
			return n_send;
		}
		return 0;
	}
	return m_fsm.bodylen();
}

int
httpsession::send_header(const char *hd[], const char *hv[], int n_hd, int chunked)
{
	int r, len = 0, hl;
	char header[4096];
	if (chunked) {
		const char chunk_header[] = "Transfer-Encoding: chunked\r\n";
		if ((r = send(chunk_header, sizeof(chunk_header) - 1)) < 0) {
			return r;
		}
		TRACE("%s", chunk_header);
	}
	for (int i = 0; i < n_hd; i++) {
		hl = nbr_str_printf(header, sizeof(header) - 1,
				"%s: %s\r\n", hd[i], hv[i]);
		if ((r = send(header, hl)) < 0) {
			return r;
		}
		TRACE("%s", header);
		len += r;
	}
	return len;
}

int
httpsession::send_result_code(httprc rc, int cf)
{
	char buffer[1024];
	int len = sprintf(buffer, "HTTP/%u.%u %03u\r\n",
			m_fsm.version() / 10, m_fsm.version() % 10, rc);
	int r = send(buffer, len);
	if (r > 0 && cf) {
		close();
	}
	return r;

}

int
httpsession::send_result_and_body(httprc rc,
		char *b, int bl, const char *mime)
{
	char hd[1024], *phd = hd;
	int hl = 0, r, tl = 0;
	hl += snprintf(phd + hl, sizeof(hd) - hl, "HTTP/%u.%u %03u\r\n",
			m_fsm.version() / 10, m_fsm.version() % 10, rc);
	hl += snprintf(phd + hl, sizeof(hd) - hl, "Content-Length: %u\r\n", bl);
	hl += snprintf(phd + hl, sizeof(hd) - hl, "Content-Type: %s\r\n\r\n", mime);
	int limit = writable();
	if (limit < hl) {
		return NBR_ESHORT;
	}
	if ((r = send(hd, hl)) < 0) { return r; }
	tl += r;
	if (limit >= (hl + bl)) {
		if ((r = send(b, bl)) < 0) { return r; }
		tl += r;
		close();
	}
	else {
		m_fsm.set_send_body(b, bl);
		return tl + bl;
	}
	return tl;
}
