#if !defined(__HTTPD_H__)
#define __HTTPD_H__

#include "http.h"

class get_request_session : public httpsession {
public:
	get_request_session() : httpsession() {}
	~get_request_session() {}
public:
	int send_request();
	int process(response &r);
};

class testhttpd : public daemon {
public:
	testhttpd() : daemon() {}
	session::factory 	*create_factory(const char *sname);
};

#endif //__HTTPD_H__
