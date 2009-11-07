/****************************************************************
 * ssl.c : openssl base secure connection plugin
 * 2009/10/24 iyatomi : create
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
#include "common.h"
#include "proto.h"
#include "osdep.h"
#include "nbr_plg.h"

#include <openssl/ssl.h>
#include <openssl/err.h>

/*-------------------------------------------------------------*/
/* macro													   */
/*-------------------------------------------------------------*/
#define SSL_ERROUT		NBR_ERROUT



/*-------------------------------------------------------------*/
/* constant													   */
/*-------------------------------------------------------------*/



/*-------------------------------------------------------------*/
/* internal types											   */
/*-------------------------------------------------------------*/
typedef struct sslfd {
	SSL *ssl;
	int fd;
} sslfd_t;



/*-------------------------------------------------------------*/
/* static variable											   */
/*-------------------------------------------------------------*/
static	struct {
	PROTOCOL 	*proto;
	ARRAY		*socks;
	SSL_CTX		*client, *server;
	char 		*pubkey, *privkey;
}	g_ssl  = {
				NULL,
				NULL,
				NULL, NULL,
				NULL, NULL
};



/*-------------------------------------------------------------*/
/* internal methods											   */
/*-------------------------------------------------------------*/
NBR_INLINE SSL_METHOD *
ssl_get_method(int type)
{
	switch(type) {
	default:
	case NBR_SSL_METHOD_INVALID:
		ASSERT(FALSE);
		return NULL;
	case NBR_SSL_METHOD_SERVER_V23:
		return SSLv23_server_method();
	case NBR_TLS_METHOD_SERVER_V1:
		return TLSv1_server_method();
	case NBR_SSL_METHOD_CLIENT_V23:
		return SSLv23_client_method();
	case NBR_TLS_METHOD_CLIENT_V1:
		return TLSv1_client_method();
	}
}

NBR_INLINE void
ssl_free_fd(sslfd_t *sfd)
{
	if (sfd) {
		if (sfd->ssl) {
			SSL_shutdown(sfd->ssl);
			SSL_free(sfd->ssl);
			sfd->ssl = NULL;
		}
		if (sfd->fd != INVALID_FD) {
			nbr_osdep_tcp_close(sfd->fd);
			sfd->fd = INVALID_FD;
		}
		nbr_array_free(g_ssl.socks, sfd);
	}
}

#if defined(_DEBUG)
static void ssl_info_callback(SSL *ssl, int st, int err)
{
//	TRACE("%u: ssl=%p,%u,%u\n", getpid(), ssl, st, err);
	if (st == SSL_CB_CONNECT_EXIT) {
		if(SSL_ERROR_SSL == SSL_get_error(ssl, err)) {
			ERR_print_errors_fp(stderr);
			ASSERT(FALSE);
		}
	}
}
#endif


/*-------------------------------------------------------------*/
/* external methods											   */
/*-------------------------------------------------------------*/
int
nbr_ssl_init(void *ctx)
{
	SSLCONF *sslc = ctx;
	SSL_library_init();
#if defined(_DEBUG)
	SSL_load_error_strings();
#endif
	if (!(g_ssl.socks = nbr_array_create(
		sslc->max_socket, sizeof(sslfd_t), NBR_PRIM_EXPANDABLE)) < 0) {
		return NBR_EEXPIRE;
	}
	if (sslc->client_method != NBR_SSL_METHOD_INVALID &&
		!(g_ssl.client = SSL_CTX_new(ssl_get_method(sslc->client_method)))) {
		SSL_ERROUT(ERROR,INVAL,"ssl context %d fail", sslc->client_method);
		return LASTERR;
	}
	if (sslc->server_method != NBR_SSL_METHOD_INVALID &&
		!(g_ssl.server = SSL_CTX_new(ssl_get_method(sslc->server_method)))) {
		SSL_ERROUT(ERROR,INVAL,"ssl context %d fail", sslc->server_method);
		return LASTERR;
	}
	if (!(g_ssl.pubkey = sslc->pubkey)) {
		SSL_ERROUT(ERROR,INVAL,"public key path is NULL");
		return NBR_EINVAL;
	}
	if (!(g_ssl.privkey = sslc->privkey)) {
		SSL_ERROUT(ERROR,INVAL,"private key path is NULL");
		return NBR_EINVAL;
	}
	return NBR_OK;
}

int
nbr_ssl_fin(void *ctx)
{
	sslfd_t *sfd, *psfd;
	if (g_ssl.socks) {
		sfd = nbr_array_get_first(g_ssl.socks);
		while((psfd = sfd)) {
			sfd = nbr_array_get_next(g_ssl.socks, sfd);
			ssl_free_fd(psfd);
		}
		nbr_array_destroy(g_ssl.socks);
		g_ssl.socks = NULL;
	}
	if (g_ssl.server) {
		SSL_CTX_free(g_ssl.server);
		g_ssl.server = NULL;
	}
	if (g_ssl.client) {
		SSL_CTX_free(g_ssl.client);
		g_ssl.client = NULL;
	}
	g_ssl.proto = NULL;
	return NBR_OK;
}

int
nbr_ssl_fd(DSCRPTR fd)
{
	sslfd_t *sfd = (sslfd_t *)fd;
	return sfd->fd;
}

DSCRPTR
nbr_ssl_socket(char *addr, SKCONF *cfg)
{
	sslfd_t *sfd = nbr_array_alloc(g_ssl.socks);
	if (!sfd) {
		SSL_ERROUT(ERROR,EXPIRE,"sfd alloc fail");
		goto bad;
	}
	sfd->ssl = NULL;
	if ((sfd->fd = nbr_osdep_tcp_socket(addr, cfg)) == INVALID_FD) {
		SSL_ERROUT(ERROR,SOCKET,"sfd socket fail");
		goto bad;
	}
	return (DSCRPTR)sfd;
bad:
	ssl_free_fd(sfd);
	return INVALID_FD;
}

int
nbr_ssl_connect(DSCRPTR fd, void *addr, int alen)
{
	int r, ret;
	sslfd_t *sfd = (sslfd_t *)fd;
	if (nbr_osdep_tcp_connect(sfd->fd, addr, alen) < 0) {
		SSL_ERROUT(ERROR,CONNECT,"base connect fail");
		return LASTERR;
	}
	if (!(sfd->ssl = SSL_new(g_ssl.client))) {
		SSL_ERROUT(ERROR,MALLOC,"ssl new fail (client)");
		return LASTERR;
	}
#if defined(_DEBUG)
	sfd->ssl->info_callback = (void (*)())ssl_info_callback;
#endif
	SSL_set_fd(sfd->ssl, sfd->fd);
	if ((r = SSL_connect(sfd->ssl)) <= 0) {
		ret = SSL_get_error(sfd->ssl, r);
		if (ret == SSL_ERROR_WANT_READ || ret == SSL_ERROR_WANT_WRITE) {
			return 0;	/* connection pending */
		}
		ERR_print_errors_fp(stderr);
		SSL_ERROUT(ERROR,CONNECT,"ssl_connect fail (%d/%d)", r, ret);
		return LASTERR;
	}
	TRACE("%u: handshake success\n", getpid());
	return NBR_OK;
}

int
nbr_ssl_handshake(DSCRPTR fd, int r, int w)
{
	int ret, ret2;
	sslfd_t *sfd = (sslfd_t *)fd;
	if (!r && !w) { return 0; }
	ret = SSL_do_handshake(sfd->ssl);
	if (ret <= 0) {
		ret2 = SSL_get_error(sfd->ssl, ret);
		if (ret2 == SSL_ERROR_WANT_READ || ret2 == SSL_ERROR_WANT_WRITE) {
			return 0;	/* connection pending */
		}
		ERR_print_errors_fp(stderr);
		SSL_ERROUT(ERROR,CONNECT,"ssl_handshake fail (%d/%d)", ret, ret2);
		ASSERT(FALSE);
		return LASTERR;
	}
	TRACE("%u: handshake success\n", getpid());
	return 1;
}

DSCRPTR
nbr_ssl_accept(DSCRPTR fd, void *addr, int *alen, SKCONF *cfg)
{
	int r, ret;
	sslfd_t *sfd = nbr_array_alloc(g_ssl.socks), *afd = (sslfd_t *)fd;
	if (!sfd) {
		SSL_ERROUT(ERROR,EXPIRE,"sfd alloc fail");
		goto bad;
	}
	if ((sfd->fd = nbr_osdep_tcp_accept(afd->fd, addr, alen, cfg)) == INVALID_FD) {
		SSL_ERROUT(ERROR,ACCEPT,"ssl accept fail");
		goto bad;
	}
	if (!(sfd->ssl = SSL_new(g_ssl.server))) {
		SSL_ERROUT(ERROR,EXPIRE,"ssl new fail");
		goto bad;
	}
#if defined(_DEBUG)
	sfd->ssl->info_callback = (void (*)())ssl_info_callback;
#endif
	if (!g_ssl.pubkey || SSL_use_certificate_file(sfd->ssl, g_ssl.pubkey, SSL_FILETYPE_PEM) < 0) {
		SSL_ERROUT(ERROR,EXPIRE,"ssl SSL_use_certificate_file fail");
		goto bad;
	}
	if (!g_ssl.privkey || SSL_use_PrivateKey_file(sfd->ssl, g_ssl.privkey, SSL_FILETYPE_PEM) < 0) {
		SSL_ERROUT(ERROR,EXPIRE,"ssl SSL_use_PrivateKey_file fail");
		goto bad;
	}
	SSL_set_fd(sfd->ssl, sfd->fd);
	if ((r = SSL_accept(sfd->ssl)) <= 0) {
		ret = SSL_get_error(sfd->ssl, ret);
		if (ret == SSL_ERROR_WANT_READ || ret == SSL_ERROR_WANT_WRITE) {
			return 0;	/* connection pending */
		}
		ERR_print_errors_fp(stderr);
		SSL_ERROUT(ERROR,ACCEPT,"ssl_accept fail (%d/%d)", r, ret);
		goto bad;
	}
	return (DSCRPTR)sfd;
bad:
	ssl_free_fd(sfd);
	return INVALID_FD;
}

int
nbr_ssl_close(DSCRPTR fd)
{
	sslfd_t *sfd = (sslfd_t *)fd;
	ssl_free_fd(sfd);
	return 0;
}

int
nbr_ssl_recv(DSCRPTR fd, void *p, size_t l, int flag)
{
	sslfd_t *sfd = (sslfd_t *)fd;
	return SSL_read( sfd->ssl, p, l );
}

int
nbr_ssl_send(DSCRPTR fd, const void *p, size_t l, int flag)
{
	sslfd_t *sfd = (sslfd_t *)fd;
	return SSL_write( sfd->ssl, p, l );
}

/* register */
PROTOCOL *
nbr_proto_ssl(void *p)
{
	PROTOCOL	ssl = {
		"SSL",
		p,
		0,		/* stream socket */
		nbr_ssl_init,
		nbr_ssl_fin,
		nbr_ssl_fd,
		nbr_osdep_tcp_str2addr,
		nbr_osdep_tcp_addr2str,
		nbr_ssl_socket,
		nbr_ssl_connect,
		nbr_ssl_handshake,
		nbr_ssl_accept,
		nbr_ssl_close,
		(RECVFUNC)nbr_ssl_recv,
		(SENDFUNC)nbr_ssl_send,
	};
	if (!g_ssl.proto) {
		if (!(g_ssl.proto = nbr_proto_regist(ssl))) {
			SSL_ERROUT(ERROR,EXPIRE,"%s:protocol register fail",ssl.name);
			return NULL;
		}
	}
	return g_ssl.proto;
}
