/***************************************************************
 * session.h : template implementation part of sfc::session
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

/*-------------------------------------------------------------*/
/* sfc::finder											   	   */
/*-------------------------------------------------------------*/
/* protocol */
template <class S>
int finder_protocol_impl<S>::on_recv(S &s, char *p, int l)
{
	char sym[SYM_SIZE];
	U8 is_reply;
	POP_START(p, l);
	POP_8(is_reply);
	POP_STR(sym, (int)sizeof(sym));
	if (is_reply) {
		return s.on_reply(sym, POP_BUF(), POP_REMAIN());
	}
	else {
		return s.on_inquiry(sym, POP_BUF(), POP_REMAIN());
	}
}

/* factory */
template <class F>
int finder_factory::on_recv(SOCK sk, char *p, int l)
{
	factory *fc =
		(factory *)nbr_sockmgr_get_data(nbr_sock_get_mgr(sk));
	F fdr(sk, fc);
	return finder_protocol_impl<F>::on_recv(fdr, p, l);
}

