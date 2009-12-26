/***************************************************************
 * httpd.h : testing suite of http.h/cpp
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
#if !defined(__HTTPD_H__)
#define __HTTPD_H__

#include "http.h"

class get_request_session : public httpsession {
	const char *m_url;
public:
	get_request_session() : httpsession(), m_url(NULL) {}
	~get_request_session() {}
	void seturl(const char *url) { m_url = url; }
public:
	int send_request();
	int on_close(int r);
	int process(response &r);
};

class testhttpd : public daemon {
public:
	testhttpd() : daemon() {}
	session::factory 	*create_factory(const char *sname);
	int					create_config(config* cl[], int size);
	int					boot(int argc, char *argv[]);
};

#endif //__HTTPD_H__
