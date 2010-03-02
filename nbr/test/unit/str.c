/****************************************************************
 * str.c : for testing core/str.c
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
#include "tutil.h"

BOOL
nbr_str_test()
{
#define LAST_SIGN "BEER"
#define LEN_LAST_SIGN (sizeof(LAST_SIGN) - 1)
#define RF_STR "\r\n"
#define LEN_RF_STR (sizeof(RF_STR) - 1)
#define TESTHOST "10.25.251.65"
#define TESTPORT 8099
#define TESTPORT_STR "8099"
#define TESTPATH "wpxy?index=300"
#define TESTURL TESTHOST":"TESTPORT_STR
#define TESTURL2 TESTURL"/"TESTPATH
#define LEN_OF_(str)	(sizeof(str) - 1)

	char buffer[256], buffer2[256], host[256], url[256];
	int len, len2;
	U64	big_num = nbr_rand64(), big_num2;
	U32	num = nbr_rand32(), num2;
	U16 port;
	const char *p;

	nbr_str_printf(buffer, sizeof(buffer), "%llu", big_num);
	if (nbr_str_atobn(buffer, (S64 *)&big_num2, sizeof(buffer)) != NBR_OK) {
		TRACE("nbr_str_atobn: convert fail: %s\n", buffer);
		return FALSE;
	}
	if (big_num != big_num2) {
		TRACE("nbr_str_atobn: result incorrect: %llu, %llu\n", big_num, big_num2);
		return FALSE;
	}

	nbr_str_printf(buffer, sizeof(buffer), "%llx", big_num);
	if (nbr_str_htobn(buffer, (S64 *)&big_num2, sizeof(buffer)) != NBR_OK) {
		TRACE("nbr_str_htobn: convert fail: %s\n", buffer);
		return FALSE;
	}
	if (big_num != big_num2) {
		TRACE("nbr_str_atohn: result incorrect: %llx, %llx\n", big_num, big_num2);
		return FALSE;
	}

	nbr_str_printf(buffer, sizeof(buffer), "%u", num);
	if (nbr_str_atoi(buffer, (S32 *)&num2, sizeof(buffer)) != NBR_OK) {
		TRACE("nbr_str_atoi: convert fail: %s\n", buffer);
		return FALSE;
	}
	if (num != num2) {
		TRACE("nbr_str_atoi: result incorrect: %u, %u\n", num, num2);
		return FALSE;
	}

	nbr_str_printf(buffer, sizeof(buffer), "%x", num);
	if (nbr_str_htoi(buffer, (S32 *)&num2, sizeof(buffer)) != NBR_OK) {
		TRACE("nbr_str_htoi: convert fail: %s\n", buffer);
		return FALSE;
	}
	if (num != num2) {
		TRACE("nbr_str_htoi: result incorrect: %x, %x\n", num, num2);
		return FALSE;
	}

	write_random_string(buffer, (nbr_rand32() % (sizeof(buffer) - 1)) + 1);
	toupper_string(buffer, buffer2);
	if (0 != nbr_str_cmp_nocase(buffer, buffer2, sizeof(buffer))) {
		TRACE("nbr_str_cmp_nocase: unmatch %s %s\n", buffer, buffer2);
		return FALSE;
	}

	len = ((nbr_rand32() % (sizeof(buffer) - 1)) + 1);
	if (len > LEN_LAST_SIGN) {
		write_random_string(buffer, len - LEN_LAST_SIGN);
	}
	else {
		len = LEN_LAST_SIGN + 1;
	}
	memcpy(&(buffer[len - LEN_LAST_SIGN - 1]), LAST_SIGN, (LEN_LAST_SIGN + 1));

	len2 = ((nbr_rand32() % (sizeof(buffer2) - 1)) + 1);
	if (len2 > LEN_LAST_SIGN) {
		write_random_string(buffer2, len2 - LEN_LAST_SIGN);
	}
	else {
		len2 = LEN_LAST_SIGN + 1;
	}
	memcpy(&(buffer2[len2 - LEN_LAST_SIGN - 1]), LAST_SIGN, (LEN_LAST_SIGN + 1));

	if (0 != nbr_str_cmp_tail(buffer, buffer2, LEN_LAST_SIGN, sizeof(buffer))) {
		TRACE("nbr_str_cmp_tail: unmatch <%s> <%s>\n", buffer, buffer2);
		return FALSE;
	}

	len = ((nbr_rand32() % (sizeof(buffer) - 1)) + 1);
	if (len > LEN_RF_STR) {
		write_random_string(buffer, len - LEN_RF_STR);
	}
	memcpy(&(buffer[len - LEN_RF_STR - 1]), RF_STR, (LEN_RF_STR + 1));
	p = nbr_str_chop(buffer);
	if (0 == nbr_str_cmp_tail(buffer, RF_STR, sizeof(buffer), LEN_RF_STR)) {
		TRACE("nbr_str_chop: unmatch %s\n", buffer);
		return FALSE;
	}

	if (nbr_str_parse_url(TESTURL, LEN_OF_(TESTURL), host, &port, url) != NBR_OK) {
		TRACE("nbr_str_parse_url: fail (%s=>(%s:%u:%s))\n", TESTURL, host, port, url);
		return FALSE;
	}
	if (port != TESTPORT || nbr_str_cmp(host, sizeof(host), TESTHOST, LEN_OF_(TESTHOST)) != 0) {
		TRACE("nbr_str_parse_url: fail2 (%s=>(%s:%u:%s))\n", TESTURL, host, port, url);
		return FALSE;
	}

	if (nbr_str_parse_url(TESTURL2, LEN_OF_(TESTURL2), host, &port, url) != NBR_OK) {
		TRACE("nbr_str_parse_url: fail3 (%s=>(%s:%u:%s))\n", TESTURL, host, port, url);
		return FALSE;
	}
	if (port != TESTPORT ||
		nbr_str_cmp(host, sizeof(host), TESTHOST, LEN_OF_(TESTHOST)) != 0 ||
		nbr_str_cmp(url, sizeof(url), "/"TESTPATH, LEN_OF_("/"TESTPATH)) != 0) {
		TRACE("nbr_str_parse_url: fail4 (%s=>(%s:%u:%s))\n", TESTURL2, host, port, url);
		return FALSE;
	}

	p = nbr_str_divide_tag_and_val('/', TESTURL2, buffer, sizeof(buffer));
	if (p == NULL) {
		TRACE("nbr_str_divide_tag_and_val: fail (%s=>(%s:%s))\n", TESTURL2, p, buffer);
		return FALSE;
	}
	if (nbr_str_cmp(p, sizeof(buffer), TESTPATH, LEN_OF_(TESTPATH)) != 0) {
		TRACE("nbr_str_divide_tag_and_val: fail2 (%s=>(%s:%s))\n", TESTURL2, p, buffer);
		return FALSE;
	}
	return TRUE;
}
