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
	state s = status();
	char *w = b;
	while (s != state_error && s != state_recv_finish) {
		if (m_len >= m_max) {
			s = state_error;
			break;
		}
		m_p[m_len++] = *w++;
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
	return (m_len > 0 && *p == '\n') ||
			(m_len > 1 && GET_16(p - 1) == crlf);
}

int
httpsession::fsm::recv_lflf() const
{
	const char *p = current();
	return (m_len > 1 && GET_16(p - 1) == lflf) ||
			(m_len > 3 && GET_32(p - 3) == crlfcrlf);
}

httpsession::fsm::state
httpsession::fsm::recv_header()
{
	char *p = current();
	if (recv_lf()) {
		/* lf found but line is empty. means \n\n or \r\n\r\n */
		if (p == m_buf) {
			int cl; char tok[8];
			/* if content length is exist, no chunk encoding */
			if (hdrint("Content-Length", cl) >= 0) {
				recvctx().bd = (p + 1);
				recvctx().bl = cl;
				return state_recv_body_nochunk;
			}
			/* if chunk encoding, process as chunk */
			else if (hdrstr("Transfer-Encoding", tok, sizeof(tok)) != NULL &&
						nbr_str_cmp(tok, sizeof(tok), "chunked", 7)) {
				return state_recv_bodylen;
			}
			else {
				return state_error;
			}
		}
		/* lf found. */
		else if (recvctx().n_hd < MAX_HEADER) {
			recvctx().hd[recvctx().n_hd] = m_buf;
			recvctx().hl[recvctx().n_hd] = (p - m_buf);
			*p = '\0';
			m_buf = (p + 1);
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
	if (recv_lf()) {
		if ((recvctx().bd + recvctx().bl) != (m_p + m_len)) {
			return state_error;
		}
		m_buf = current() + 1;
		return state_recv_bodylen;
	}
	return state_recv_body;
}

httpsession::fsm::state
httpsession::fsm::recv_body_nochunk()
{
	if (recv_lflf()) {
		return state_recv_finish;
	}
	return state_recv_body_nochunk;
}

httpsession::fsm::state
httpsession::fsm::recv_bodylen()
{
	char *p = current();
	state s = state_recv_bodylen;

	if (recv_lf()) {
		s = state_recv_body;
	}
	if (*p == ';') {
		/* comment is specified after length */
		s = state_recv_comment;
	}
	if (s != state_recv_bodylen) {
		int cl;
		*p = '\0';
		if (nbr_str_atoi(m_buf, &cl, (p - m_buf)) < 0) {
			return state_error;
		}
		/* 0-length chunk means chunk end -> next footer */
		if (cl == 0) {
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
	if (recv_lf()) {
		/* lf found but line is empty. means \n\n or \r\n\r\n */
		if (p == m_buf) {
			return state_recv_finish;
		}
		/* lf found. */
		else if (recvctx().n_hd < MAX_HEADER) {
			recvctx().hd[recvctx().n_hd] = m_buf;
			recvctx().hl[recvctx().n_hd] = (p - m_buf);
			*p = '\0';
			m_buf = (p + 1);
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
	if (recv_lf()) {
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



/*-------------------------------------------------------------*/
/* sfc::httpsession											   */
/*-------------------------------------------------------------*/
void
httpsession::fin()
{
}

void
httpsession::poll(UTIME nt)
{
	if (m_fsm.status() == fsm::state_send_body) {
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
	session::poll(nt);
}

int
httpsession::on_open(const config &cfg)
{
	m_fsm.reset(cfg);
	return NBR_OK;
}

int
httpsession::on_close(int r)
{
	return NBR_OK;
}

int
httpsession::get(const char *url, char *hd[], int hl[], int n_hd)
{
	char data[1024], host[1024];
	int r, len = nbr_str_printf(data, sizeof(data),
		"GET %s HTTP/1.1\r\n"
		"Host: %s\r\n\r\n",
		url, remoteaddr(host, sizeof(host)));
	if ((r = send(data, len)) < 0) {
		return r;
	}
	len = r;
	if ((r = send_header(hd, hl, n_hd)) < 0) {
		return r;
	}
	len += r;
	return len;
}

int
httpsession::post(const char *url, const char *body, int blen,
		char *hd[], int hl[], int n_hd)
{
	char data[1024], host[1024];
	int r, len = nbr_str_printf(data, sizeof(data),
		"GET %s HTTP/1.1\r\n"
		"Host: %s\r\n\r\n",
		url, remoteaddr(host, sizeof(host)));
	if ((r = send(data, len)) < 0) {
		return r;
	}
	len = r;
	if ((r = send_header(hd, hl, n_hd)) < 0) {
		return r;
	}
	len += r;
	if (writable() < blen) {
		m_fsm.set_send_body(body, blen);
		return len + blen;
	}
	else if ((r = send(body, blen)) < 0) {
		return r;
	}
	return len + r;
}

int
httpsession::send_body()
{
	char length[16];
	int n_send = m_fsm.send_body_len();
	int n_length = nbr_str_printf(length,
			sizeof(length) - 1, "%08x\r\n", n_send);
	int n_limit = (writable() - n_length - 2/* for last \r\n */);
	if (n_send > n_limit) {
		n_send = n_limit;
	}
	if ((n_length = send(length, n_length)) < 0) {
		return n_length;
	}
	if ((n_send = send(m_fsm.send_body_ptr(), n_send)) < 0) {
		return n_send;
	}
	m_fsm.bodysent(n_send);
	if ((n_send = send("\r\n", 2)) < 0) {
		return n_send;
	}
	return m_fsm.bodylen();
}

int
httpsession::send_header(char *hd[], int hl[], int n_hd)
{
	int r, len = 0;
	for (int i = 0; i < n_hd; i++) {
		if ((r = send(hd[i], hl[i])) < 0) {
			return r;
		}
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
