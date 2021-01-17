#include <rte_ether.h>
#include "parser.h"


/*
 * Update destination ethaddr for the port.
 */
int
add_dst_ethaddr(uint16_t port, const struct rte_ether_addr *addr)
{
	if (port >= RTE_DIM(ethaddr_tbl))
		return -EINVAL;

	ethaddr_tbl[port].dst = ETHADDR_TO_UINT64(addr);
	return 0;
}


//////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////



/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2016 Intel Corporation
 */
#include <rte_common.h>
#include <rte_crypto.h>
#include <rte_string_fns.h>

#include <cmdline_parse_string.h>
#include <cmdline_parse_num.h>
#include <cmdline_parse_ipaddr.h>
#include <cmdline_socket.h>
#include <cmdline.h>

// #include "ipsec.h"
// #include "parser.h"

#define PARSE_DELIMITER		" \f\n\r\t\v"
static int
parse_tokenize_string(char *string, char *tokens[], uint32_t *n_tokens)
{
	uint32_t i;

	if ((string == NULL) ||
		(tokens == NULL) ||
		(*n_tokens < 1))
		return -EINVAL;

	for (i = 0; i < *n_tokens; i++) {
		tokens[i] = strtok_r(string, PARSE_DELIMITER, &string);
		if (tokens[i] == NULL)
			break;
	}

	if ((i == *n_tokens) &&
		(NULL != strtok_r(string, PARSE_DELIMITER, &string)))
		return -E2BIG;

	*n_tokens = i;
	return 0;
}

#define INADDRSZ 4
#define IN6ADDRSZ 16

/* int
 * inet_pton4(src, dst)
 *      like inet_aton() but without all the hexadecimal and shorthand.
 * return:
 *      1 if `src' is a valid dotted quad, else 0.
 * notice:
 *      does not touch `dst' unless it's returning 1.
 * author:
 *      Paul Vixie, 1996.
 */
static int
inet_pton4(const char *src, unsigned char *dst)
{
	static const char digits[] = "0123456789";
	int saw_digit, octets, ch;
	unsigned char tmp[INADDRSZ], *tp;

	saw_digit = 0;
	octets = 0;
	*(tp = tmp) = 0;
	while ((ch = *src++) != '\0') {
		const char *pch;

		pch = strchr(digits, ch);
		if (pch != NULL) {
			unsigned int new = *tp * 10 + (pch - digits);

			if (new > 255)
				return 0;
			if (!saw_digit) {
				if (++octets > 4)
					return 0;
				saw_digit = 1;
			}
			*tp = (unsigned char)new;
		} else if (ch == '.' && saw_digit) {
			if (octets == 4)
				return 0;
			*++tp = 0;
			saw_digit = 0;
		} else
			return 0;
	}
	if (octets < 4)
		return 0;

	memcpy(dst, tmp, INADDRSZ);
	return 1;
}

/* int
 * inet_pton6(src, dst)
 *      convert presentation level address to network order binary form.
 * return:
 *      1 if `src' is a valid [RFC1884 2.2] address, else 0.
 * notice:
 *      (1) does not touch `dst' unless it's returning 1.
 *      (2) :: in a full address is silently ignored.
 * credit:
 *      inspired by Mark Andrews.
 * author:
 *      Paul Vixie, 1996.
 */
static int
inet_pton6(const char *src, unsigned char *dst)
{
	static const char xdigits_l[] = "0123456789abcdef",
		xdigits_u[] = "0123456789ABCDEF";
	unsigned char tmp[IN6ADDRSZ], *tp = 0, *endp = 0, *colonp = 0;
	const char *xdigits = 0, *curtok = 0;
	int ch = 0, saw_xdigit = 0, count_xdigit = 0;
	unsigned int val = 0;
	unsigned dbloct_count = 0;

	memset((tp = tmp), '\0', IN6ADDRSZ);
	endp = tp + IN6ADDRSZ;
	colonp = NULL;
	/* Leading :: requires some special handling. */
	if (*src == ':')
		if (*++src != ':')
			return 0;
	curtok = src;
	saw_xdigit = count_xdigit = 0;
	val = 0;

	while ((ch = *src++) != '\0') {
		const char *pch;

		pch = strchr((xdigits = xdigits_l), ch);
		if (pch == NULL)
			pch = strchr((xdigits = xdigits_u), ch);
		if (pch != NULL) {
			if (count_xdigit >= 4)
				return 0;
			val <<= 4;
			val |= (pch - xdigits);
			if (val > 0xffff)
				return 0;
			saw_xdigit = 1;
			count_xdigit++;
			continue;
		}
		if (ch == ':') {
			curtok = src;
			if (!saw_xdigit) {
				if (colonp)
					return 0;
				colonp = tp;
				continue;
			} else if (*src == '\0') {
				return 0;
			}
			if (tp + sizeof(int16_t) > endp)
				return 0;
			*tp++ = (unsigned char) ((val >> 8) & 0xff);
			*tp++ = (unsigned char) (val & 0xff);
			saw_xdigit = 0;
			count_xdigit = 0;
			val = 0;
			dbloct_count++;
			continue;
		}
		if (ch == '.' && ((tp + INADDRSZ) <= endp) &&
		    inet_pton4(curtok, tp) > 0) {
			tp += INADDRSZ;
			saw_xdigit = 0;
			dbloct_count += 2;
			break;  /* '\0' was seen by inet_pton4(). */
		}
		return 0;
	}
	if (saw_xdigit) {
		if (tp + sizeof(int16_t) > endp)
			return 0;
		*tp++ = (unsigned char) ((val >> 8) & 0xff);
		*tp++ = (unsigned char) (val & 0xff);
		dbloct_count++;
	}
	if (colonp != NULL) {
		/* if we already have 8 double octets, having a colon
		 * means error */
		if (dbloct_count == 8)
			return 0;

		/*
		 * Since some memmove()'s erroneously fail to handle
		 * overlapping regions, we'll do the shift by hand.
		 */
		const int n = tp - colonp;
		int i;

		for (i = 1; i <= n; i++) {
			endp[-i] = colonp[n - i];
			colonp[n - i] = 0;
		}
		tp = endp;
	}
	if (tp != endp)
		return 0;
	memcpy(dst, tmp, IN6ADDRSZ);
	return 1;
}

int
parse_ipv4_addr(const char *token, struct in_addr *ipv4, uint32_t *mask)
{
	char ip_str[INET_ADDRSTRLEN] = {0};
	char *pch;

	pch = strchr(token, '/');
	if (pch != NULL) {
		strlcpy(ip_str, token,
			RTE_MIN((unsigned int long)(pch - token + 1),
			sizeof(ip_str)));
		pch += 1;
		if (is_str_num(pch) != 0)
			return -EINVAL;
		if (mask)
			*mask = atoi(pch);
	} else {
		strlcpy(ip_str, token, sizeof(ip_str));
		if (mask)
			*mask = 0;
	}
	if (strlen(ip_str) >= INET_ADDRSTRLEN)
		return -EINVAL;

	if (inet_pton4(ip_str, (unsigned char *)ipv4) != 1)
		return -EINVAL;

	return 0;
}

int
parse_ipv6_addr(const char *token, struct in6_addr *ipv6, uint32_t *mask)
{
	char ip_str[256] = {0};
	char *pch;

	pch = strchr(token, '/');
	if (pch != NULL) {
		strlcpy(ip_str, token,
			RTE_MIN((unsigned int long)(pch - token + 1),
					sizeof(ip_str)));
		pch += 1;
		if (is_str_num(pch) != 0)
			return -EINVAL;
		if (mask)
			*mask = atoi(pch);
	} else {
		strlcpy(ip_str, token, sizeof(ip_str));
		if (mask)
			*mask = 0;
	}

	if (strlen(ip_str) >= INET6_ADDRSTRLEN)
		return -EINVAL;

	if (inet_pton6(ip_str, (unsigned char *)ipv6) != 1)
		return -EINVAL;

	return 0;
}

int
parse_range(const char *token, uint16_t *low, uint16_t *high)
{
	char ch;
	char num_str[20];
	uint32_t pos;
	int range_low = -1;
	int range_high = -1;

	if (!low || !high)
		return -1;

	memset(num_str, 0, 20);
	pos = 0;

	while ((ch = *token++) != '\0') {
		if (isdigit(ch)) {
			if (pos >= 19)
				return -1;
			num_str[pos++] = ch;
		} else if (ch == ':') {
			if (range_low != -1)
				return -1;
			range_low = atoi(num_str);
			memset(num_str, 0, 20);
			pos = 0;
		}
	}

	if (strlen(num_str) == 0)
		return -1;

	range_high = atoi(num_str);

	*low = (uint16_t)range_low;
	*high = (uint16_t)range_high;

	return 0;
}

/*
 * helper function for parse_mac, parse one section of the ether addr.
 */
static const char *
parse_uint8x16(const char *s, uint8_t *v, uint8_t ls)
{
	char *end;
	unsigned long t;

	errno = 0;
	t = strtoul(s, &end, 16);
	if (errno != 0 || end[0] != ls || t > UINT8_MAX)
		return NULL;
	v[0] = t;
	return end + 1;
}

static int
parse_mac(const char *str, struct rte_ether_addr *addr)
{
	uint32_t i;

	static const uint8_t stop_sym[RTE_DIM(addr->addr_bytes)] = {
		[0] = ':',
		[1] = ':',
		[2] = ':',
		[3] = ':',
		[4] = ':',
		[5] = 0,
	};

	for (i = 0; i != RTE_DIM(addr->addr_bytes); i++) {
		str = parse_uint8x16(str, addr->addr_bytes + i, stop_sym[i]);
		if (str == NULL)
			return -EINVAL;
	}

	return 0;
}

/** sp add parse */
struct cfg_sp_add_cfg_item {
	cmdline_fixed_string_t sp_keyword;
	cmdline_multi_string_t multi_string;
};

static void
cfg_sp_add_cfg_item_parsed(void *parsed_result,
	__rte_unused struct cmdline *cl, void *data)
{
	struct cfg_sp_add_cfg_item *params = parsed_result;
	char *tokens[32];
	uint32_t n_tokens = RTE_DIM(tokens);
	struct parse_status *status = (struct parse_status *)data;

	APP_CHECK((parse_tokenize_string(params->multi_string, tokens,
		&n_tokens) == 0), status, "too many arguments");

	if (status->status < 0)
		return;

	if (strcmp(tokens[0], "ipv4") == 0) {
		parse_sp4_tokens(tokens, n_tokens, status);
		if (status->status < 0)
			return;
	} 
	// rider added 2020-12-4 disable
	// else if (strcmp(tokens[0], "ipv6") == 0) {
	// 	parse_sp6_tokens(tokens, n_tokens, status);
	// 	if (status->status < 0)
	// 		return;
	// } 
	else {
		APP_CHECK(0, status, "unrecognizable input %s\n",
			tokens[0]);
		return;
	}
	
}

static cmdline_parse_token_string_t cfg_sp_add_sp_str =
	TOKEN_STRING_INITIALIZER(struct cfg_sp_add_cfg_item,
		sp_keyword, "sp");

static cmdline_parse_token_string_t cfg_sp_add_multi_str =
	TOKEN_STRING_INITIALIZER(struct cfg_sp_add_cfg_item, multi_string,
		TOKEN_STRING_MULTI);

cmdline_parse_inst_t cfg_sp_add_rule = {
	.f = cfg_sp_add_cfg_item_parsed,
	.data = NULL,
	.help_str = "",
	.tokens = {
		(void *) &cfg_sp_add_sp_str,
		(void *) &cfg_sp_add_multi_str,
		NULL,
	},
};

/* sa add parse */
struct cfg_sa_add_cfg_item {
	cmdline_fixed_string_t sa_keyword;
	cmdline_multi_string_t multi_string;
};

static void
cfg_sa_add_cfg_item_parsed(void *parsed_result,
	__rte_unused struct cmdline *cl, void *data)
{
	struct cfg_sa_add_cfg_item *params = parsed_result;
	char *tokens[32];
	uint32_t n_tokens = RTE_DIM(tokens);
	struct parse_status *status = (struct parse_status *)data;

	APP_CHECK(parse_tokenize_string(params->multi_string, tokens,
		&n_tokens) == 0, status, "too many arguments\n");

	parse_sa_tokens(tokens, n_tokens, status);
}

static cmdline_parse_token_string_t cfg_sa_add_sa_str =
	TOKEN_STRING_INITIALIZER(struct cfg_sa_add_cfg_item,
		sa_keyword, "sa");

static cmdline_parse_token_string_t cfg_sa_add_multi_str =
	TOKEN_STRING_INITIALIZER(struct cfg_sa_add_cfg_item, multi_string,
		TOKEN_STRING_MULTI);

cmdline_parse_inst_t cfg_sa_add_rule = {
	.f = cfg_sa_add_cfg_item_parsed,
	.data = NULL,
	.help_str = "",
	.tokens = {
		(void *) &cfg_sa_add_sa_str,
		(void *) &cfg_sa_add_multi_str,
		NULL,
	},
};

/* rt add parse */
struct cfg_rt_add_cfg_item {
	cmdline_fixed_string_t rt_keyword;
	cmdline_multi_string_t multi_string;
};

static void
cfg_rt_add_cfg_item_parsed(void *parsed_result,
	__rte_unused struct cmdline *cl, void *data)
{
	// roder added 2020-9-28 disable 
	// struct cfg_rt_add_cfg_item *params = parsed_result;
	// char *tokens[32];
	// uint32_t n_tokens = RTE_DIM(tokens);
	// struct parse_status *status = (struct parse_status *)data;

	// APP_CHECK(parse_tokenize_string(
	// 	params->multi_string, tokens, &n_tokens) == 0,
	// 	status, "too many arguments\n");
	// if (status->status < 0)
	// 	return;

	// parse_rt_tokens(tokens, n_tokens, status);
	//
}

static cmdline_parse_token_string_t cfg_rt_add_rt_str =
	TOKEN_STRING_INITIALIZER(struct cfg_rt_add_cfg_item,
		rt_keyword, "rt");

static cmdline_parse_token_string_t cfg_rt_add_multi_str =
	TOKEN_STRING_INITIALIZER(struct cfg_rt_add_cfg_item, multi_string,
		TOKEN_STRING_MULTI);

cmdline_parse_inst_t cfg_rt_add_rule = {
	.f = cfg_rt_add_cfg_item_parsed,
	.data = NULL,
	.help_str = "",
	.tokens = {
		(void *) &cfg_rt_add_rt_str,
		(void *) &cfg_rt_add_multi_str,
		NULL,
	},
};

/* neigh add parse */
struct cfg_neigh_add_item {
	cmdline_fixed_string_t neigh;
	cmdline_fixed_string_t pstr;
	uint16_t port;
	cmdline_fixed_string_t mac;
};

static void
cfg_parse_neigh(void *parsed_result, __rte_unused struct cmdline *cl,
	void *data)
{
	int32_t rc;
	struct cfg_neigh_add_item *res;
	struct parse_status *st;
	struct rte_ether_addr mac;

	st = data;
	res = parsed_result;
	rc = parse_mac(res->mac, &mac);
	APP_CHECK(rc == 0, st, "invalid ether addr:%s", res->mac);
	rc = add_dst_ethaddr(res->port, &mac);
	APP_CHECK(rc == 0, st, "invalid port numer:%hu", res->port);
	if (st->status < 0)
		return;
}

cmdline_parse_token_string_t cfg_add_neigh_start =
	TOKEN_STRING_INITIALIZER(struct cfg_neigh_add_item, neigh, "neigh");
cmdline_parse_token_string_t cfg_add_neigh_pstr =
	TOKEN_STRING_INITIALIZER(struct cfg_neigh_add_item, pstr, "port");
cmdline_parse_token_num_t cfg_add_neigh_port =
	TOKEN_NUM_INITIALIZER(struct cfg_neigh_add_item, port, UINT16);
cmdline_parse_token_string_t cfg_add_neigh_mac =
	TOKEN_STRING_INITIALIZER(struct cfg_neigh_add_item, mac, NULL);

cmdline_parse_inst_t cfg_neigh_add_rule = {
	.f = cfg_parse_neigh,
	.data = NULL,
	.help_str = "",
	.tokens = {
		(void *)&cfg_add_neigh_start,
		(void *)&cfg_add_neigh_pstr,
		(void *)&cfg_add_neigh_port,
		(void *)&cfg_add_neigh_mac,
		NULL,
	},
};

/** set of cfg items */
cmdline_parse_ctx_t ipsec_ctx[] = {
	(cmdline_parse_inst_t *)&cfg_sp_add_rule,
	(cmdline_parse_inst_t *)&cfg_sa_add_rule,
	(cmdline_parse_inst_t *)&cfg_rt_add_rule,
	(cmdline_parse_inst_t *)&cfg_neigh_add_rule,
	NULL,
};

int
parse_cfg_file(const char *cfg_filename)
{
	struct cmdline *cl = cmdline_stdin_new(ipsec_ctx, "");
	FILE *f = fopen(cfg_filename, "r");
	char str[1024] = {0}, *get_s = NULL;
	uint32_t line_num = 0;
	struct parse_status status = {0};

	if (f == NULL) {
		rte_panic("Error: invalid file descriptor %s\n", cfg_filename);
		goto error_exit;
	}

	if (cl == NULL) {
		rte_panic("Error: cannot create cmdline instance\n");
		goto error_exit;
	}

	cfg_sp_add_rule.data = &status;
	cfg_sa_add_rule.data = &status;
	cfg_rt_add_rule.data = &status;
	cfg_neigh_add_rule.data = &status;

	do {
		char oneline[1024];
		char *pos;
		get_s = fgets(oneline, 1024, f);

		if (!get_s)
			break;

		line_num++;

		if (strlen(oneline) > 1022) {
			rte_panic("%s:%u: error: "
				"the line contains more characters the parser can handle\n",
				cfg_filename, line_num);
			goto error_exit;
		}

		/* process comment char '#' */
		if (oneline[0] == '#')
			continue;

		pos = strchr(oneline, '#');
		if (pos != NULL)
			*pos = '\0';

		/* process line concatenator '\' */
		pos = strchr(oneline, 92);
		if (pos != NULL) {
			if (pos != oneline+strlen(oneline) - 2) {
				rte_panic("%s:%u: error: "
					"no character should exist after '\\'\n",
					cfg_filename, line_num);
				goto error_exit;
			}

			*pos = '\0';

			if (strlen(oneline) + strlen(str) > 1022) {
				rte_panic("%s:%u: error: "
					"the concatenated line contains more characters the parser can handle\n",
					cfg_filename, line_num);
				goto error_exit;
			}

			strcpy(str + strlen(str), oneline);
			continue;
		}

		/* copy the line to str and process */
		if (strlen(oneline) + strlen(str) > 1022) {
			rte_panic("%s:%u: error: "
				"the line contains more characters the parser can handle\n",
				cfg_filename, line_num);
			goto error_exit;
		}
		strcpy(str + strlen(str), oneline);

		str[strlen(str)] = '\n';
		if (cmdline_parse(cl, str) < 0) {
			rte_panic("%s:%u: error: parsing \"%s\" failed\n",
				cfg_filename, line_num, str);
			goto error_exit;
		}

		if (status.status < 0) {
			rte_panic("%s:%u: error: %s", cfg_filename,
				line_num, status.parse_msg);
			goto error_exit;
		}

		memset(str, 0, 1024);
	} while (1);

	cmdline_stdin_exit(cl);
	fclose(f);

	return 0;

error_exit:
	if (cl)
		cmdline_stdin_exit(cl);
	if (f)
		fclose(f);

	return -1;
}



//////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////




//////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////



/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2016-2017 Intel Corporation
 */

/*
 * Security Associations
 */
#include <sys/types.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/ip6.h>

#include <rte_memzone.h>
#include <rte_crypto.h>
#include <rte_security.h>
#include <rte_cryptodev.h>
#include <rte_byteorder.h>
#include <rte_errno.h>
#include <rte_ip.h>
#include <rte_random.h>
#include <rte_ethdev.h>
#include <rte_malloc.h>

// #include "ipsec.h"
// #include "esp.h"
// #include "parser.h"

#define IPDEFTTL 64

#define IP4_FULL_MASK (sizeof(((struct ip_addr *)NULL)->ip.ip4) * CHAR_BIT)

#define IP6_FULL_MASK (sizeof(((struct ip_addr *)NULL)->ip.ip6.ip6) * CHAR_BIT)

#define MBUF_NO_SEC_OFFLOAD(m) ((m->ol_flags & PKT_RX_SEC_OFFLOAD) == 0)

struct supported_cipher_algo {
	const char *keyword;
	enum rte_crypto_cipher_algorithm algo;
	uint16_t iv_len;
	uint16_t block_size;
	uint16_t key_len;
};

struct supported_auth_algo {
	const char *keyword;
	enum rte_crypto_auth_algorithm algo;
	uint16_t digest_len;
	uint16_t key_len;
	uint8_t key_not_req;
};

struct supported_aead_algo {
	const char *keyword;
	enum rte_crypto_aead_algorithm algo;
	uint16_t iv_len;
	uint16_t block_size;
	uint16_t digest_len;
	uint16_t key_len;
	uint8_t aad_len;
};


const struct supported_cipher_algo cipher_algos[] = {
	{
		.keyword = "null",
		.algo = RTE_CRYPTO_CIPHER_NULL,
		.iv_len = 0,
		.block_size = 4,
		.key_len = 0
	},
	{
		.keyword = "aes-128-cbc",
		.algo = RTE_CRYPTO_CIPHER_AES_CBC,
		.iv_len = 16,
		.block_size = 16,
		.key_len = 16
	},
	{
		.keyword = "aes-256-cbc",
		.algo = RTE_CRYPTO_CIPHER_AES_CBC,
		.iv_len = 16,
		.block_size = 16,
		.key_len = 32
	},
	{
		.keyword = "aes-128-ctr",
		.algo = RTE_CRYPTO_CIPHER_AES_CTR,
		.iv_len = 8,
		.block_size = 4,
		.key_len = 20
	},
	{
		.keyword = "3des-cbc",
		.algo = RTE_CRYPTO_CIPHER_3DES_CBC,
		.iv_len = 8,
		.block_size = 8,
		.key_len = 24
	}
};

const struct supported_auth_algo auth_algos[] = {
	{
		.keyword = "null",
		.algo = RTE_CRYPTO_AUTH_NULL,
		.digest_len = 0,
		.key_len = 0,
		.key_not_req = 1
	},
	{
		.keyword = "sha1-hmac",
		.algo = RTE_CRYPTO_AUTH_SHA1_HMAC,
		.digest_len = 12,
		.key_len = 20
	},
	{
		.keyword = "sha256-hmac",
		.algo = RTE_CRYPTO_AUTH_SHA256_HMAC,
		.digest_len = 16,
		.key_len = 32
	}
};

const struct supported_aead_algo aead_algos[] = {
	{
		.keyword = "aes-128-gcm",
		.algo = RTE_CRYPTO_AEAD_AES_GCM,
		.iv_len = 8,
		.block_size = 4,
		.key_len = 20,
		.digest_len = 16,
		.aad_len = 8,
	}
};

static struct ipsec_sa sa_out[IPSEC_SA_MAX_ENTRIES];
static uint32_t nb_sa_out;

static struct ipsec_sa sa_in[IPSEC_SA_MAX_ENTRIES];
static uint32_t nb_sa_in;

static const struct supported_cipher_algo *
find_match_cipher_algo(const char *cipher_keyword)
{
	size_t i;

	for (i = 0; i < RTE_DIM(cipher_algos); i++) {
		const struct supported_cipher_algo *algo =
			&cipher_algos[i];

		if (strcmp(cipher_keyword, algo->keyword) == 0)
			return algo;
	}

	return NULL;
}

static const struct supported_auth_algo *
find_match_auth_algo(const char *auth_keyword)
{
	size_t i;

	for (i = 0; i < RTE_DIM(auth_algos); i++) {
		const struct supported_auth_algo *algo =
			&auth_algos[i];

		if (strcmp(auth_keyword, algo->keyword) == 0)
			return algo;
	}

	return NULL;
}

static const struct supported_aead_algo *
find_match_aead_algo(const char *aead_keyword)
{
	size_t i;

	for (i = 0; i < RTE_DIM(aead_algos); i++) {
		const struct supported_aead_algo *algo =
			&aead_algos[i];

		if (strcmp(aead_keyword, algo->keyword) == 0)
			return algo;
	}

	return NULL;
}

/** parse_key_string
 *  parse x:x:x:x.... hex number key string into uint8_t *key
 *  return:
 *  > 0: number of bytes parsed
 *  0:   failed
 */
static uint32_t
parse_key_string(const char *key_str, uint8_t *key)
{
	const char *pt_start = key_str, *pt_end = key_str;
	uint32_t nb_bytes = 0;

	while (pt_end != NULL) {
		char sub_str[3] = {0};

		pt_end = strchr(pt_start, ':');

		if (pt_end == NULL) {
			if (strlen(pt_start) > 2)
				return 0;
			strncpy(sub_str, pt_start, 2);
		} else {
			if (pt_end - pt_start > 2)
				return 0;

			strncpy(sub_str, pt_start, pt_end - pt_start);
			pt_start = pt_end + 1;
		}

		key[nb_bytes++] = strtol(sub_str, NULL, 16);
	}

	return nb_bytes;
}

void
parse_sa_tokens(char **tokens, uint32_t n_tokens,
	struct parse_status *status)
{
	struct ipsec_sa *rule = NULL;
	struct rte_ipsec_session *ips;
	uint32_t ti; /*token index*/
	uint32_t *ri /*rule index*/;
	uint32_t cipher_algo_p = 0;
	uint32_t auth_algo_p = 0;
	uint32_t aead_algo_p = 0;
	uint32_t src_p = 0;
	uint32_t dst_p = 0;
	uint32_t mode_p = 0;
	uint32_t type_p = 0;
	uint32_t portid_p = 0;
	uint32_t fallback_p = 0;

	if (strcmp(tokens[0], "in") == 0) {
		ri = &nb_sa_in;

		APP_CHECK(*ri <= IPSEC_SA_MAX_ENTRIES - 1, status,
			"too many sa rules, abort insertion\n");
		if (status->status < 0)
			return;

		rule = &sa_in[*ri];
		rule->direction = RTE_SECURITY_IPSEC_SA_DIR_INGRESS;
	} else {
		ri = &nb_sa_out;

		APP_CHECK(*ri <= IPSEC_SA_MAX_ENTRIES - 1, status,
			"too many sa rules, abort insertion\n");
		if (status->status < 0)
			return;

		rule = &sa_out[*ri];
		rule->direction = RTE_SECURITY_IPSEC_SA_DIR_EGRESS;
	}

	/* spi number */
	APP_CHECK_TOKEN_IS_NUM(tokens, 1, status);
	if (status->status < 0)
		return;
	if (atoi(tokens[1]) == INVALID_SPI)
		return;
	rule->spi = atoi(tokens[1]);
	ips = ipsec_get_primary_session(rule);

	for (ti = 2; ti < n_tokens; ti++) {
		if (strcmp(tokens[ti], "mode") == 0) {
			APP_CHECK_PRESENCE(mode_p, tokens[ti], status);
			if (status->status < 0)
				return;

			INCREMENT_TOKEN_INDEX(ti, n_tokens, status);
			if (status->status < 0)
				return;

			if (strcmp(tokens[ti], "ipv4-tunnel") == 0)
				rule->flags = IP4_TUNNEL;
			else if (strcmp(tokens[ti], "ipv6-tunnel") == 0)
				rule->flags = IP6_TUNNEL;
			else if (strcmp(tokens[ti], "transport") == 0)
				rule->flags = TRANSPORT;
			else {
				APP_CHECK(0, status, "unrecognized "
					"input \"%s\"", tokens[ti]);
				return;
			}

			mode_p = 1;
			continue;
		}

		if (strcmp(tokens[ti], "cipher_algo") == 0) {
			const struct supported_cipher_algo *algo;
			uint32_t key_len;

			APP_CHECK_PRESENCE(cipher_algo_p, tokens[ti],
				status);
			if (status->status < 0)
				return;

			INCREMENT_TOKEN_INDEX(ti, n_tokens, status);
			if (status->status < 0)
				return;

			algo = find_match_cipher_algo(tokens[ti]);

			APP_CHECK(algo != NULL, status, "unrecognized "
				"input \"%s\"", tokens[ti]);

			if (status->status < 0)
				return;

			rule->cipher_algo = algo->algo;
			rule->block_size = algo->block_size;
			rule->iv_len = algo->iv_len;
			rule->cipher_key_len = algo->key_len;

			/* for NULL algorithm, no cipher key required */
			if (rule->cipher_algo == RTE_CRYPTO_CIPHER_NULL) {
				cipher_algo_p = 1;
				continue;
			}

			INCREMENT_TOKEN_INDEX(ti, n_tokens, status);
			if (status->status < 0)
				return;

			APP_CHECK(strcmp(tokens[ti], "cipher_key") == 0,
				status, "unrecognized input \"%s\", "
				"expect \"cipher_key\"", tokens[ti]);
			if (status->status < 0)
				return;

			INCREMENT_TOKEN_INDEX(ti, n_tokens, status);
			if (status->status < 0)
				return;

			key_len = parse_key_string(tokens[ti],
				rule->cipher_key);
			APP_CHECK(key_len == rule->cipher_key_len, status,
				"unrecognized input \"%s\"", tokens[ti]);
			if (status->status < 0)
				return;

			if (algo->algo == RTE_CRYPTO_CIPHER_AES_CBC ||
				algo->algo == RTE_CRYPTO_CIPHER_3DES_CBC)
				rule->salt = (uint32_t)rte_rand();

			if (algo->algo == RTE_CRYPTO_CIPHER_AES_CTR) {
				key_len -= 4;
				rule->cipher_key_len = key_len;
				memcpy(&rule->salt,
					&rule->cipher_key[key_len], 4);
			}

			cipher_algo_p = 1;
			continue;
		}

		if (strcmp(tokens[ti], "auth_algo") == 0) {
			const struct supported_auth_algo *algo;
			uint32_t key_len;

			APP_CHECK_PRESENCE(auth_algo_p, tokens[ti],
				status);
			if (status->status < 0)
				return;

			INCREMENT_TOKEN_INDEX(ti, n_tokens, status);
			if (status->status < 0)
				return;

			algo = find_match_auth_algo(tokens[ti]);
			APP_CHECK(algo != NULL, status, "unrecognized "
				"input \"%s\"", tokens[ti]);

			if (status->status < 0)
				return;

			rule->auth_algo = algo->algo;
			rule->auth_key_len = algo->key_len;
			rule->digest_len = algo->digest_len;

			/* NULL algorithm and combined algos do not
			 * require auth key
			 */
			if (algo->key_not_req) {
				auth_algo_p = 1;
				continue;
			}

			INCREMENT_TOKEN_INDEX(ti, n_tokens, status);
			if (status->status < 0)
				return;

			APP_CHECK(strcmp(tokens[ti], "auth_key") == 0,
				status, "unrecognized input \"%s\", "
				"expect \"auth_key\"", tokens[ti]);
			if (status->status < 0)
				return;

			INCREMENT_TOKEN_INDEX(ti, n_tokens, status);
			if (status->status < 0)
				return;

			key_len = parse_key_string(tokens[ti],
				rule->auth_key);
			APP_CHECK(key_len == rule->auth_key_len, status,
				"unrecognized input \"%s\"", tokens[ti]);
			if (status->status < 0)
				return;

			auth_algo_p = 1;
			continue;
		}

		if (strcmp(tokens[ti], "aead_algo") == 0) {
			const struct supported_aead_algo *algo;
			uint32_t key_len;

			APP_CHECK_PRESENCE(aead_algo_p, tokens[ti],
				status);
			if (status->status < 0)
				return;

			INCREMENT_TOKEN_INDEX(ti, n_tokens, status);
			if (status->status < 0)
				return;

			algo = find_match_aead_algo(tokens[ti]);

			APP_CHECK(algo != NULL, status, "unrecognized "
				"input \"%s\"", tokens[ti]);

			if (status->status < 0)
				return;

			rule->aead_algo = algo->algo;
			rule->cipher_key_len = algo->key_len;
			rule->digest_len = algo->digest_len;
			rule->aad_len = algo->aad_len;
			rule->block_size = algo->block_size;
			rule->iv_len = algo->iv_len;

			INCREMENT_TOKEN_INDEX(ti, n_tokens, status);
			if (status->status < 0)
				return;

			APP_CHECK(strcmp(tokens[ti], "aead_key") == 0,
				status, "unrecognized input \"%s\", "
				"expect \"aead_key\"", tokens[ti]);
			if (status->status < 0)
				return;

			INCREMENT_TOKEN_INDEX(ti, n_tokens, status);
			if (status->status < 0)
				return;

			key_len = parse_key_string(tokens[ti],
				rule->cipher_key);
			APP_CHECK(key_len == rule->cipher_key_len, status,
				"unrecognized input \"%s\"", tokens[ti]);
			if (status->status < 0)
				return;

			key_len -= 4;
			rule->cipher_key_len = key_len;
			memcpy(&rule->salt,
				&rule->cipher_key[key_len], 4);

			aead_algo_p = 1;
			continue;
		}

		if (strcmp(tokens[ti], "src") == 0) {
			APP_CHECK_PRESENCE(src_p, tokens[ti], status);
			if (status->status < 0)
				return;

			INCREMENT_TOKEN_INDEX(ti, n_tokens, status);
			if (status->status < 0)
				return;

			if (IS_IP4_TUNNEL(rule->flags)) {
				struct in_addr ip;

				APP_CHECK(parse_ipv4_addr(tokens[ti],
					&ip, NULL) == 0, status,
					"unrecognized input \"%s\", "
					"expect valid ipv4 addr",
					tokens[ti]);
				if (status->status < 0)
					return;
				rule->src.ip.ip4 = rte_bswap32(
					(uint32_t)ip.s_addr);
			} else if (IS_IP6_TUNNEL(rule->flags)) {
				struct in6_addr ip;

				APP_CHECK(parse_ipv6_addr(tokens[ti], &ip,
					NULL) == 0, status,
					"unrecognized input \"%s\", "
					"expect valid ipv6 addr",
					tokens[ti]);
				if (status->status < 0)
					return;
				memcpy(rule->src.ip.ip6.ip6_b,
					ip.s6_addr, 16);
			} else if (IS_TRANSPORT(rule->flags)) {
				APP_CHECK(0, status, "unrecognized input "
					"\"%s\"", tokens[ti]);
				return;
			}

			src_p = 1;
			continue;
		}

		if (strcmp(tokens[ti], "dst") == 0) {
			APP_CHECK_PRESENCE(dst_p, tokens[ti], status);
			if (status->status < 0)
				return;

			INCREMENT_TOKEN_INDEX(ti, n_tokens, status);
			if (status->status < 0)
				return;

			if (IS_IP4_TUNNEL(rule->flags)) {
				struct in_addr ip;

				APP_CHECK(parse_ipv4_addr(tokens[ti],
					&ip, NULL) == 0, status,
					"unrecognized input \"%s\", "
					"expect valid ipv4 addr",
					tokens[ti]);
				if (status->status < 0)
					return;
				rule->dst.ip.ip4 = rte_bswap32(
					(uint32_t)ip.s_addr);
			} else if (IS_IP6_TUNNEL(rule->flags)) {
				struct in6_addr ip;

				APP_CHECK(parse_ipv6_addr(tokens[ti], &ip,
					NULL) == 0, status,
					"unrecognized input \"%s\", "
					"expect valid ipv6 addr",
					tokens[ti]);
				if (status->status < 0)
					return;
				memcpy(rule->dst.ip.ip6.ip6_b, ip.s6_addr, 16);
			} else if (IS_TRANSPORT(rule->flags)) {
				APP_CHECK(0, status, "unrecognized "
					"input \"%s\"",	tokens[ti]);
				return;
			}

			dst_p = 1;
			continue;
		}

		if (strcmp(tokens[ti], "type") == 0) {
			APP_CHECK_PRESENCE(type_p, tokens[ti], status);
			if (status->status < 0)
				return;

			INCREMENT_TOKEN_INDEX(ti, n_tokens, status);
			if (status->status < 0)
				return;

			if (strcmp(tokens[ti], "inline-crypto-offload") == 0)
				ips->type =
					RTE_SECURITY_ACTION_TYPE_INLINE_CRYPTO;
			else if (strcmp(tokens[ti],
					"inline-protocol-offload") == 0)
				ips->type =
				RTE_SECURITY_ACTION_TYPE_INLINE_PROTOCOL;
			else if (strcmp(tokens[ti],
					"lookaside-protocol-offload") == 0)
				ips->type =
				RTE_SECURITY_ACTION_TYPE_LOOKASIDE_PROTOCOL;
			else if (strcmp(tokens[ti], "no-offload") == 0)
				ips->type = RTE_SECURITY_ACTION_TYPE_NONE;
			else {
				APP_CHECK(0, status, "Invalid input \"%s\"",
						tokens[ti]);
				return;
			}

			type_p = 1;
			continue;
		}

		if (strcmp(tokens[ti], "port_id") == 0) {
			APP_CHECK_PRESENCE(portid_p, tokens[ti], status);
			if (status->status < 0)
				return;
			INCREMENT_TOKEN_INDEX(ti, n_tokens, status);
			if (status->status < 0)
				return;
			rule->portid = atoi(tokens[ti]);
			if (status->status < 0)
				return;
			portid_p = 1;
			continue;
		}

		if (strcmp(tokens[ti], "fallback") == 0) {
			struct rte_ipsec_session *fb;

			APP_CHECK(app_sa_prm.enable, status, "Fallback session "
				"not allowed for legacy mode.");
			if (status->status < 0)
				return;
			APP_CHECK(ips->type ==
				RTE_SECURITY_ACTION_TYPE_INLINE_CRYPTO, status,
				"Fallback session allowed if primary session "
				"is of type inline-crypto-offload only.");
			if (status->status < 0)
				return;
			APP_CHECK(rule->direction ==
				RTE_SECURITY_IPSEC_SA_DIR_INGRESS, status,
				"Fallback session not allowed for egress "
				"rule");
			if (status->status < 0)
				return;
			APP_CHECK_PRESENCE(fallback_p, tokens[ti], status);
			if (status->status < 0)
				return;
			INCREMENT_TOKEN_INDEX(ti, n_tokens, status);
			if (status->status < 0)
				return;
			fb = ipsec_get_fallback_session(rule);
			if (strcmp(tokens[ti], "lookaside-none") == 0) {
				fb->type = RTE_SECURITY_ACTION_TYPE_NONE;
			} else {
				APP_CHECK(0, status, "unrecognized fallback "
					"type %s.", tokens[ti]);
				return;
			}

			rule->fallback_sessions = 1;
			fallback_p = 1;
			continue;
		}

		/* unrecognizeable input */
		APP_CHECK(0, status, "unrecognized input \"%s\"",
			tokens[ti]);
		return;
	}

	if (aead_algo_p) {
		APP_CHECK(cipher_algo_p == 0, status,
				"AEAD used, no need for cipher options");
		if (status->status < 0)
			return;

		APP_CHECK(auth_algo_p == 0, status,
				"AEAD used, no need for auth options");
		if (status->status < 0)
			return;
	} else {
		APP_CHECK(cipher_algo_p == 1, status, "missing cipher or AEAD options");
		if (status->status < 0)
			return;

		APP_CHECK(auth_algo_p == 1, status, "missing auth or AEAD options");
		if (status->status < 0)
			return;
	}

	APP_CHECK(mode_p == 1, status, "missing mode option");
	if (status->status < 0)
		return;

	if ((ips->type != RTE_SECURITY_ACTION_TYPE_NONE) && (portid_p == 0))
		printf("Missing portid option, falling back to non-offload\n");

	if (!type_p || !portid_p) {
		ips->type = RTE_SECURITY_ACTION_TYPE_NONE;
		rule->portid = -1;
	}

	*ri = *ri + 1;
}

static void
print_one_sa_rule(const struct ipsec_sa *sa, int inbound)
{
	uint32_t i;
	uint8_t a, b, c, d;
	const struct rte_ipsec_session *ips;
	const struct rte_ipsec_session *fallback_ips;

	printf("\tspi_%s(%3u):", inbound?"in":"out", sa->spi);

	for (i = 0; i < RTE_DIM(cipher_algos); i++) {
		if (cipher_algos[i].algo == sa->cipher_algo &&
				cipher_algos[i].key_len == sa->cipher_key_len) {
			printf("%s ", cipher_algos[i].keyword);
			break;
		}
	}

	for (i = 0; i < RTE_DIM(auth_algos); i++) {
		if (auth_algos[i].algo == sa->auth_algo) {
			printf("%s ", auth_algos[i].keyword);
			break;
		}
	}

	for (i = 0; i < RTE_DIM(aead_algos); i++) {
		if (aead_algos[i].algo == sa->aead_algo) {
			printf("%s ", aead_algos[i].keyword);
			break;
		}
	}

	printf("mode:");

	switch (WITHOUT_TRANSPORT_VERSION(sa->flags)) {
	case IP4_TUNNEL:
		printf("IP4Tunnel ");
		uint32_t_to_char(sa->src.ip.ip4, &a, &b, &c, &d);
		printf("%hhu.%hhu.%hhu.%hhu ", d, c, b, a);
		uint32_t_to_char(sa->dst.ip.ip4, &a, &b, &c, &d);
		printf("%hhu.%hhu.%hhu.%hhu", d, c, b, a);
		break;
	case IP6_TUNNEL:
		printf("IP6Tunnel ");
		for (i = 0; i < 16; i++) {
			if (i % 2 && i != 15)
				printf("%.2x:", sa->src.ip.ip6.ip6_b[i]);
			else
				printf("%.2x", sa->src.ip.ip6.ip6_b[i]);
		}
		printf(" ");
		for (i = 0; i < 16; i++) {
			if (i % 2 && i != 15)
				printf("%.2x:", sa->dst.ip.ip6.ip6_b[i]);
			else
				printf("%.2x", sa->dst.ip.ip6.ip6_b[i]);
		}
		break;
	case TRANSPORT:
		printf("Transport ");
		break;
	}

	ips = &sa->sessions[IPSEC_SESSION_PRIMARY];
	printf(" type:");
	switch (ips->type) {
	case RTE_SECURITY_ACTION_TYPE_NONE:
		printf("no-offload ");
		break;
	case RTE_SECURITY_ACTION_TYPE_INLINE_CRYPTO:
		printf("inline-crypto-offload ");
		break;
	case RTE_SECURITY_ACTION_TYPE_INLINE_PROTOCOL:
		printf("inline-protocol-offload ");
		break;
	case RTE_SECURITY_ACTION_TYPE_LOOKASIDE_PROTOCOL:
		printf("lookaside-protocol-offload ");
		break;
	}

	fallback_ips = &sa->sessions[IPSEC_SESSION_FALLBACK];
	if (fallback_ips != NULL && sa->fallback_sessions > 0) {
		printf("inline fallback: ");
		if (fallback_ips->type == RTE_SECURITY_ACTION_TYPE_NONE)
			printf("lookaside-none");
		else
			printf("invalid");
	}
	printf("\n");
}


static struct sa_ctx *
sa_create(const char *name, int32_t socket_id)
{
	char s[PATH_MAX];
	struct sa_ctx *sa_ctx;
	uint32_t mz_size;
	const struct rte_memzone *mz;

	snprintf(s, sizeof(s), "%s_%u", name, socket_id);

	/* Create SA array table */
	printf("Creating SA context with %u maximum entries on socket %d\n",
			IPSEC_SA_MAX_ENTRIES, socket_id);

	mz_size = sizeof(struct sa_ctx);
	mz = rte_memzone_reserve(s, mz_size, socket_id,
			RTE_MEMZONE_1GB | RTE_MEMZONE_SIZE_HINT_ONLY);
	if (mz == NULL) {
		printf("Failed to allocate SA DB memory\n");
		rte_errno = ENOMEM;
		return NULL;
	}

	sa_ctx = (struct sa_ctx *)mz->addr;

	return sa_ctx;
}

static int
check_eth_dev_caps(uint16_t portid, uint32_t inbound)
{
	struct rte_eth_dev_info dev_info;
	int retval;

	retval = rte_eth_dev_info_get(portid, &dev_info);
	if (retval != 0) {
		RTE_LOG(ERR, IPSEC,
			"Error during getting device (port %u) info: %s\n",
			portid, strerror(-retval));

		return retval;
	}

	if (inbound) {
		if ((dev_info.rx_offload_capa &
				DEV_RX_OFFLOAD_SECURITY) == 0) {
			RTE_LOG(WARNING, PORT,
				"hardware RX IPSec offload is not supported\n");
			return -EINVAL;
		}

	} else { /* outbound */
		if ((dev_info.tx_offload_capa &
				DEV_TX_OFFLOAD_SECURITY) == 0) {
			RTE_LOG(WARNING, PORT,
				"hardware TX IPSec offload is not supported\n");
			return -EINVAL;
		}
	}
	return 0;
}

/*
 * Helper function, tries to determine next_proto for SPI
 * by searching though SP rules.
 */
static int
get_spi_proto(uint32_t spi, enum rte_security_ipsec_sa_direction dir,
		struct ip_addr ip_addr[2], uint32_t mask[2])
{
	int32_t rc4, rc6;

	rc4 = sp4_spi_present(spi, dir == RTE_SECURITY_IPSEC_SA_DIR_INGRESS,
				ip_addr, mask);
	rc6 = sp6_spi_present(spi, dir == RTE_SECURITY_IPSEC_SA_DIR_INGRESS,
				ip_addr, mask);

	if (rc4 >= 0) {
		if (rc6 >= 0) {
			RTE_LOG(ERR, IPSEC,
				"%s: SPI %u used simultaeously by "
				"IPv4(%d) and IPv6 (%d) SP rules\n",
				__func__, spi, rc4, rc6);
			return -EINVAL;
		} else
			return IPPROTO_IPIP;
	} else if (rc6 < 0) {
		RTE_LOG(ERR, IPSEC,
			"%s: SPI %u is not used by any SP rule\n",
			__func__, spi);
		return -EINVAL;
	} else
		return IPPROTO_IPV6;
}

/*
 * Helper function for getting source and destination IP addresses
 * from SP. Needed for inline crypto transport mode, as addresses are not
 * provided in config file for that mode. It checks if SP for current SA exists,
 * and based on what type of protocol is returned, it stores appropriate
 * addresses got from SP into SA.
 */
static int
sa_add_address_inline_crypto(struct ipsec_sa *sa)
{
	int protocol;
	struct ip_addr ip_addr[2];
	uint32_t mask[2];

	protocol = get_spi_proto(sa->spi, sa->direction, ip_addr, mask);
	if (protocol < 0)
		return protocol;
	else if (protocol == IPPROTO_IPIP) {
		sa->flags |= IP4_TRANSPORT;
		if (mask[0] == IP4_FULL_MASK &&
				mask[1] == IP4_FULL_MASK &&
				ip_addr[0].ip.ip4 != 0 &&
				ip_addr[1].ip.ip4 != 0) {

			sa->src.ip.ip4 = ip_addr[0].ip.ip4;
			sa->dst.ip.ip4 = ip_addr[1].ip.ip4;
		} else {
			RTE_LOG(ERR, IPSEC,
			"%s: No valid address or mask entry in"
			" IPv4 SP rule for SPI %u\n",
			__func__, sa->spi);
			return -EINVAL;
		}
	} else if (protocol == IPPROTO_IPV6) {
		sa->flags |= IP6_TRANSPORT;
		if (mask[0] == IP6_FULL_MASK &&
				mask[1] == IP6_FULL_MASK &&
				(ip_addr[0].ip.ip6.ip6[0] != 0 ||
				ip_addr[0].ip.ip6.ip6[1] != 0) &&
				(ip_addr[1].ip.ip6.ip6[0] != 0 ||
				ip_addr[1].ip.ip6.ip6[1] != 0)) {

			sa->src.ip.ip6 = ip_addr[0].ip.ip6;
			sa->dst.ip.ip6 = ip_addr[1].ip.ip6;
		} else {
			RTE_LOG(ERR, IPSEC,
			"%s: No valid address or mask entry in"
			" IPv6 SP rule for SPI %u\n",
			__func__, sa->spi);
			return -EINVAL;
		}
	}
	return 0;
}

static int
sa_add_rules(struct sa_ctx *sa_ctx, const struct ipsec_sa entries[],
		uint32_t nb_entries, uint32_t inbound,
		struct socket_ctx *skt_ctx)
{
	struct ipsec_sa *sa;
	uint32_t i, idx;
	uint16_t iv_length, aad_length;
	int inline_status;
	int32_t rc;
	struct rte_ipsec_session *ips;

	/* for ESN upper 32 bits of SQN also need to be part of AAD */
	aad_length = (app_sa_prm.enable_esn != 0) ? sizeof(uint32_t) : 0;

	for (i = 0; i < nb_entries; i++) {
		idx = SPI2IDX(entries[i].spi);

		sa = &sa_ctx->sa[idx];
		if (sa->spi != 0) {
			printf("Index %u already in use by SPI %u\n",
					idx, sa->spi);
			return -EINVAL;
		}
		*sa = entries[i];
		sa->seq = 0;
		ips = ipsec_get_primary_session(sa);

		if (ips->type == RTE_SECURITY_ACTION_TYPE_INLINE_PROTOCOL ||
			ips->type == RTE_SECURITY_ACTION_TYPE_INLINE_CRYPTO) {
			if (check_eth_dev_caps(sa->portid, inbound))
				return -EINVAL;
		}


		switch (WITHOUT_TRANSPORT_VERSION(sa->flags)) {
		case IP4_TUNNEL:
			sa->src.ip.ip4 = rte_cpu_to_be_32(sa->src.ip.ip4);
			sa->dst.ip.ip4 = rte_cpu_to_be_32(sa->dst.ip.ip4);
			break;
		case TRANSPORT:
			if (ips->type ==
				RTE_SECURITY_ACTION_TYPE_INLINE_CRYPTO) {
				inline_status =
					sa_add_address_inline_crypto(sa);
				if (inline_status < 0)
					return inline_status;
			}
			break;
		}

		if (sa->aead_algo == RTE_CRYPTO_AEAD_AES_GCM) {
			iv_length = 12;

			sa_ctx->xf[idx].a.type = RTE_CRYPTO_SYM_XFORM_AEAD;
			sa_ctx->xf[idx].a.aead.algo = sa->aead_algo;
			sa_ctx->xf[idx].a.aead.key.data = sa->cipher_key;
			sa_ctx->xf[idx].a.aead.key.length =
				sa->cipher_key_len;
			sa_ctx->xf[idx].a.aead.op = (inbound == 1) ?
				RTE_CRYPTO_AEAD_OP_DECRYPT :
				RTE_CRYPTO_AEAD_OP_ENCRYPT;
			sa_ctx->xf[idx].a.next = NULL;
			sa_ctx->xf[idx].a.aead.iv.offset = IV_OFFSET;
			sa_ctx->xf[idx].a.aead.iv.length = iv_length;
			sa_ctx->xf[idx].a.aead.aad_length =
				sa->aad_len + aad_length;
			sa_ctx->xf[idx].a.aead.digest_length =
				sa->digest_len;

			sa->xforms = &sa_ctx->xf[idx].a;
		} else {
			switch (sa->cipher_algo) {
			case RTE_CRYPTO_CIPHER_NULL:
			case RTE_CRYPTO_CIPHER_3DES_CBC:
			case RTE_CRYPTO_CIPHER_AES_CBC:
				iv_length = sa->iv_len;
				break;
			case RTE_CRYPTO_CIPHER_AES_CTR:
				iv_length = 16;
				break;
			default:
				RTE_LOG(ERR, IPSEC_ESP,
						"unsupported cipher algorithm %u\n",
						sa->cipher_algo);
				return -EINVAL;
			}

			if (inbound) {
				sa_ctx->xf[idx].b.type = RTE_CRYPTO_SYM_XFORM_CIPHER;
				sa_ctx->xf[idx].b.cipher.algo = sa->cipher_algo;
				sa_ctx->xf[idx].b.cipher.key.data = sa->cipher_key;
				sa_ctx->xf[idx].b.cipher.key.length =
					sa->cipher_key_len;
				sa_ctx->xf[idx].b.cipher.op =
					RTE_CRYPTO_CIPHER_OP_DECRYPT;
				sa_ctx->xf[idx].b.next = NULL;
				sa_ctx->xf[idx].b.cipher.iv.offset = IV_OFFSET;
				sa_ctx->xf[idx].b.cipher.iv.length = iv_length;

				sa_ctx->xf[idx].a.type = RTE_CRYPTO_SYM_XFORM_AUTH;
				sa_ctx->xf[idx].a.auth.algo = sa->auth_algo;
				sa_ctx->xf[idx].a.auth.key.data = sa->auth_key;
				sa_ctx->xf[idx].a.auth.key.length =
					sa->auth_key_len;
				sa_ctx->xf[idx].a.auth.digest_length =
					sa->digest_len;
				sa_ctx->xf[idx].a.auth.op =
					RTE_CRYPTO_AUTH_OP_VERIFY;
			} else { /* outbound */
				sa_ctx->xf[idx].a.type = RTE_CRYPTO_SYM_XFORM_CIPHER;
				sa_ctx->xf[idx].a.cipher.algo = sa->cipher_algo;
				sa_ctx->xf[idx].a.cipher.key.data = sa->cipher_key;
				sa_ctx->xf[idx].a.cipher.key.length =
					sa->cipher_key_len;
				sa_ctx->xf[idx].a.cipher.op =
					RTE_CRYPTO_CIPHER_OP_ENCRYPT;
				sa_ctx->xf[idx].a.next = NULL;
				sa_ctx->xf[idx].a.cipher.iv.offset = IV_OFFSET;
				sa_ctx->xf[idx].a.cipher.iv.length = iv_length;

				sa_ctx->xf[idx].b.type = RTE_CRYPTO_SYM_XFORM_AUTH;
				sa_ctx->xf[idx].b.auth.algo = sa->auth_algo;
				sa_ctx->xf[idx].b.auth.key.data = sa->auth_key;
				sa_ctx->xf[idx].b.auth.key.length =
					sa->auth_key_len;
				sa_ctx->xf[idx].b.auth.digest_length =
					sa->digest_len;
				sa_ctx->xf[idx].b.auth.op =
					RTE_CRYPTO_AUTH_OP_GENERATE;
			}

			sa_ctx->xf[idx].a.next = &sa_ctx->xf[idx].b;
			sa_ctx->xf[idx].b.next = NULL;
			sa->xforms = &sa_ctx->xf[idx].a;
		}

		if (ips->type ==
			RTE_SECURITY_ACTION_TYPE_INLINE_PROTOCOL ||
			ips->type ==
			RTE_SECURITY_ACTION_TYPE_INLINE_CRYPTO) {
			// rider added 2020-9-28 disable
			// rc = create_inline_session(skt_ctx, sa, ips);
			// if (rc != 0) {
			// 	RTE_LOG(ERR, IPSEC_ESP,
			// 		"create_inline_session() failed\n");
			// 	return -EINVAL;
			// }
			//
		}

		print_one_sa_rule(sa, inbound);
	}

	return 0;
}

static inline int
sa_out_add_rules(struct sa_ctx *sa_ctx, const struct ipsec_sa entries[],
		uint32_t nb_entries, struct socket_ctx *skt_ctx)
{
	return sa_add_rules(sa_ctx, entries, nb_entries, 0, skt_ctx);
}

static inline int
sa_in_add_rules(struct sa_ctx *sa_ctx, const struct ipsec_sa entries[],
		uint32_t nb_entries, struct socket_ctx *skt_ctx)
{
	return sa_add_rules(sa_ctx, entries, nb_entries, 1, skt_ctx);
}

/*
 * helper function, fills parameters that are identical for all SAs
 */
static void
fill_ipsec_app_sa_prm(struct rte_ipsec_sa_prm *prm,
	const struct app_sa_prm *app_prm)
{
	memset(prm, 0, sizeof(*prm));

	prm->flags = app_prm->flags;
	prm->ipsec_xform.options.esn = app_prm->enable_esn;
	prm->ipsec_xform.replay_win_sz = app_prm->window_size;
}

static int
fill_ipsec_sa_prm(struct rte_ipsec_sa_prm *prm, const struct ipsec_sa *ss,
	const struct rte_ipv4_hdr *v4, struct rte_ipv6_hdr *v6)
{
	int32_t rc;
	
	/*
	 * Try to get SPI next proto by searching that SPI in SPD.
	 * probably not the optimal way, but there seems nothing
	 * better right now.
	 */
	// rider added 2020-9-28 disable default rc and manually set
	// rc = get_spi_proto(ss->spi, ss->direction, NULL, NULL);
	// if (rc < 0)
	// 	return rc;
	//
	rc = IPPROTO_IPIP;

	fill_ipsec_app_sa_prm(prm, &app_sa_prm);
	prm->userdata = (uintptr_t)ss;

	/* setup ipsec xform */
	prm->ipsec_xform.spi = ss->spi;
	prm->ipsec_xform.salt = ss->salt;
	prm->ipsec_xform.direction = ss->direction;
	prm->ipsec_xform.proto = RTE_SECURITY_IPSEC_SA_PROTO_ESP;
	prm->ipsec_xform.mode = (IS_TRANSPORT(ss->flags)) ?
		RTE_SECURITY_IPSEC_SA_MODE_TRANSPORT :
		RTE_SECURITY_IPSEC_SA_MODE_TUNNEL;
	prm->ipsec_xform.options.ecn = 1;
	prm->ipsec_xform.options.copy_dscp = 1;

	if (IS_IP4_TUNNEL(ss->flags)) {
		prm->ipsec_xform.tunnel.type = RTE_SECURITY_IPSEC_TUNNEL_IPV4;
		prm->tun.hdr_len = sizeof(*v4);
		prm->tun.next_proto = rc;
		prm->tun.hdr = v4;
	} else if (IS_IP6_TUNNEL(ss->flags)) {
		prm->ipsec_xform.tunnel.type = RTE_SECURITY_IPSEC_TUNNEL_IPV6;
		prm->tun.hdr_len = sizeof(*v6);
		prm->tun.next_proto = rc;
		prm->tun.hdr = v6;
	} else {
		/* transport mode */
		prm->trs.proto = rc;
	}

	/* setup crypto section */
	prm->crypto_xform = ss->xforms;
	return 0;
}

static int
fill_ipsec_session(struct rte_ipsec_session *ss, struct rte_ipsec_sa *sa)
{
	int32_t rc = 0;

	ss->sa = sa;

	if (ss->type == RTE_SECURITY_ACTION_TYPE_INLINE_CRYPTO ||
		ss->type == RTE_SECURITY_ACTION_TYPE_INLINE_PROTOCOL) {
		if (ss->security.ses != NULL) {
			rc = rte_ipsec_session_prepare(ss);
			if (rc != 0)
				memset(ss, 0, sizeof(*ss));
		}
	}

	return rc;
}

/*
 * Initialise related rte_ipsec_sa object.
 */
static int
ipsec_sa_init(struct ipsec_sa *lsa, struct rte_ipsec_sa *sa, uint32_t sa_size)
{
	int rc;
	struct rte_ipsec_sa_prm prm;
	struct rte_ipsec_session *ips;
	struct rte_ipv4_hdr v4  = {
		.version_ihl = IPVERSION << 4 |
			sizeof(v4) / RTE_IPV4_IHL_MULTIPLIER,
		.time_to_live = IPDEFTTL,
		.next_proto_id = IPPROTO_ESP,
		.src_addr = lsa->src.ip.ip4,
		.dst_addr = lsa->dst.ip.ip4,
	};
	struct rte_ipv6_hdr v6 = {
		.vtc_flow = htonl(IP6_VERSION << 28),
		.proto = IPPROTO_ESP,
	};

	if (IS_IP6_TUNNEL(lsa->flags)) {
		memcpy(v6.src_addr, lsa->src.ip.ip6.ip6_b, sizeof(v6.src_addr));
		memcpy(v6.dst_addr, lsa->dst.ip.ip6.ip6_b, sizeof(v6.dst_addr));
	}

	rc = fill_ipsec_sa_prm(&prm, lsa, &v4, &v6);
	if (rc == 0)
		rc = rte_ipsec_sa_init(sa, &prm, sa_size);
	if (rc < 0)
		return rc;

	/* init primary processing session */
	ips = ipsec_get_primary_session(lsa);
	rc = fill_ipsec_session(ips, sa);
	if (rc != 0)
		return rc;

	/* init inline fallback processing session */
	if (lsa->fallback_sessions == 1)
		rc = fill_ipsec_session(ipsec_get_fallback_session(lsa), sa);

	return rc;
}

/*
 * Allocate space and init rte_ipsec_sa strcutures,
 * one per session.
 */
static int
ipsec_satbl_init(struct sa_ctx *ctx, const struct ipsec_sa *ent,
	uint32_t nb_ent, int32_t socket)
{
	int32_t rc, sz;
	uint32_t i, idx;
	size_t tsz;
	struct rte_ipsec_sa *sa;
	struct ipsec_sa *lsa;
	struct rte_ipsec_sa_prm prm;

	/* determine SA size */
	idx = SPI2IDX(ent[0].spi);
	fill_ipsec_sa_prm(&prm, ctx->sa + idx, NULL, NULL);
	sz = rte_ipsec_sa_size(&prm);
	if (sz < 0) {
		RTE_LOG(ERR, IPSEC, "%s(%p, %u, %d): "
			"failed to determine SA size, error code: %d\n",
			__func__, ctx, nb_ent, socket, sz);
		return sz;
	}

	tsz = sz * nb_ent;

	// rider added 2020-9-28 change to malloc and memset
	// ctx->satbl = rte_zmalloc_socket(NULL, tsz, RTE_CACHE_LINE_SIZE, socket);
	ctx->satbl = malloc(tsz);
	memset(ctx->satbl, 0x00, tsz);
	//
	if (ctx->satbl == NULL) {
		RTE_LOG(ERR, IPSEC,
			"%s(%p, %u, %d): failed to allocate %zu bytes\n",
			__func__,  ctx, nb_ent, socket, tsz);
		return -ENOMEM;
	}

	rc = 0;
	for (i = 0; i != nb_ent && rc == 0; i++) {

		
		idx = SPI2IDX(ent[i].spi);

		// rider added 2020-9-28 to store sa size
		ctx->sasize = sz;
		// 
		sa = (struct rte_ipsec_sa *)((uintptr_t)ctx->satbl + sz * i);
		// rider added 2020-12-9 to store pointer list of struct rte_ipsec_sa* ipsec_sa
		ctx->ipsec_sa_ptr_list[ent[i].spi] = sa;
		//
		lsa = ctx->sa + idx;

		rc = ipsec_sa_init(lsa, sa, sz);
	}

	return rc;
}

/*
 * Walk through all SA rules to find an SA with given SPI
 */
int
sa_spi_present(uint32_t spi, int inbound)
{
	uint32_t i, num;
	const struct ipsec_sa *sar;

	if (inbound != 0) {
		sar = sa_in;
		num = nb_sa_in;
	} else {
		sar = sa_out;
		num = nb_sa_out;
	}

	for (i = 0; i != num; i++) {
		if (sar[i].spi == spi)
			return i;
	}

	return -ENOENT;
}

void
sa_init(struct socket_ctx *ctx, int32_t socket_id)
{
	int32_t rc;
	const char *name;

	if (ctx == NULL)
		rte_exit(EXIT_FAILURE, "NULL context.\n");

	if (ctx->sa_in != NULL)
		rte_exit(EXIT_FAILURE, "Inbound SA DB for socket %u already "
				"initialized\n", socket_id);

	if (ctx->sa_out != NULL)
		rte_exit(EXIT_FAILURE, "Outbound SA DB for socket %u already "
				"initialized\n", socket_id);

	if (nb_sa_in > 0) {
		name = "sa_in";
		// rider added 2020-9-28 disable sa_create and use malloc
		// ctx->sa_in = sa_create(name, socket_id);
		ctx->sa_in = (struct sa_ctx*) malloc(sizeof(struct sa_ctx));
		//
		if (ctx->sa_in == NULL)
			rte_exit(EXIT_FAILURE, "Error [%d] creating SA "
				"context %s in socket %d\n", rte_errno,
				name, socket_id);

		sa_in_add_rules(ctx->sa_in, sa_in, nb_sa_in, ctx);

		if (app_sa_prm.enable != 0) {
			rc = ipsec_satbl_init(ctx->sa_in, sa_in, nb_sa_in,
				socket_id);
			if (rc != 0)
				rte_exit(EXIT_FAILURE,
					"failed to init inbound SAs\n");
		}
	} else
		RTE_LOG(WARNING, IPSEC, "No SA Inbound rule specified\n");

	if (nb_sa_out > 0) {
		name = "sa_out";
		// rider added 2020-9-28 disable sa_create and use malloc
		// ctx->sa_out = sa_create(name, socket_id);
		ctx->sa_out = (struct sa_ctx*) malloc(sizeof(struct sa_ctx));
		//
		if (ctx->sa_out == NULL)
			rte_exit(EXIT_FAILURE, "Error [%d] creating SA "
				"context %s in socket %d\n", rte_errno,
				name, socket_id);

		sa_out_add_rules(ctx->sa_out, sa_out, nb_sa_out, ctx);

		if (app_sa_prm.enable != 0) {
			rc = ipsec_satbl_init(ctx->sa_out, sa_out, nb_sa_out,
				socket_id);
			if (rc != 0)
				rte_exit(EXIT_FAILURE,
					"failed to init outbound SAs\n");
		}
	} else
		RTE_LOG(WARNING, IPSEC, "No SA Outbound rule "
			"specified\n");
}

int
inbound_sa_check(struct sa_ctx *sa_ctx, struct rte_mbuf *m, uint32_t sa_idx)
{
	struct ipsec_mbuf_metadata *priv;
	struct ipsec_sa *sa;

	priv = get_priv(m);
	sa = priv->sa;
	if (sa != NULL)
		return (sa_ctx->sa[sa_idx].spi == sa->spi);
	RTE_LOG(ERR, IPSEC, "SA not saved in private data\n");
	return 0;
}

static inline void
single_inbound_lookup(struct ipsec_sa *sadb, struct rte_mbuf *pkt,
		void **sa_ret)
{
	struct rte_esp_hdr *esp;
	struct ip *ip;
	uint32_t *src4_addr;
	uint8_t *src6_addr;
	struct ipsec_sa *sa;
	void *result_sa;

	*sa_ret = NULL;

	ip = rte_pktmbuf_mtod(pkt, struct ip *);
	esp = rte_pktmbuf_mtod_offset(pkt, struct rte_esp_hdr *, pkt->l3_len);

	if (esp->spi == INVALID_SPI)
		return;

	result_sa = sa = &sadb[SPI2IDX(rte_be_to_cpu_32(esp->spi))];
	if (rte_be_to_cpu_32(esp->spi) != sa->spi)
		return;

	/*
	 * Mark need for inline offload fallback on the LSB of SA pointer.
	 * Thanks to packet grouping mechanism which ipsec_process is using
	 * packets marked for fallback processing will form separate group.
	 *
	 * Because it is not safe to use SA pointer it is casted to generic
	 * pointer to prevent from unintentional use. Use ipsec_mask_saptr
	 * to get valid struct pointer.
	 */
	if (MBUF_NO_SEC_OFFLOAD(pkt) && sa->fallback_sessions > 0) {
		uintptr_t intsa = (uintptr_t)sa;
		intsa |= IPSEC_SA_OFFLOAD_FALLBACK_FLAG;
		result_sa = (void *)intsa;
	}

	switch (WITHOUT_TRANSPORT_VERSION(sa->flags)) {
	case IP4_TUNNEL:
		src4_addr = RTE_PTR_ADD(ip, offsetof(struct ip, ip_src));
		if ((ip->ip_v == IPVERSION) &&
				(sa->src.ip.ip4 == *src4_addr) &&
				(sa->dst.ip.ip4 == *(src4_addr + 1)))
			*sa_ret = result_sa;
		break;
	case IP6_TUNNEL:
		src6_addr = RTE_PTR_ADD(ip, offsetof(struct ip6_hdr, ip6_src));
		if ((ip->ip_v == IP6_VERSION) &&
				!memcmp(&sa->src.ip.ip6.ip6, src6_addr, 16) &&
				!memcmp(&sa->dst.ip.ip6.ip6, src6_addr + 16, 16))
			*sa_ret = result_sa;
		break;
	case TRANSPORT:
		*sa_ret = result_sa;
	}
}

void
inbound_sa_lookup(struct sa_ctx *sa_ctx, struct rte_mbuf *pkts[],
		void *sa[], uint16_t nb_pkts)
{
	uint32_t i;

	for (i = 0; i < nb_pkts; i++)
		single_inbound_lookup(sa_ctx->sa, pkts[i], &sa[i]);
}

void
outbound_sa_lookup(struct sa_ctx *sa_ctx, uint32_t sa_idx[],
		void *sa[], uint16_t nb_pkts)
{
	uint32_t i;

	for (i = 0; i < nb_pkts; i++)
		sa[i] = &sa_ctx->sa[sa_idx[i]];
}

/*
 * Select HW offloads to be used.
 */
int
sa_check_offloads(uint16_t port_id, uint64_t *rx_offloads,
		uint64_t *tx_offloads)
{
	struct ipsec_sa *rule;
	uint32_t idx_sa;
	enum rte_security_session_action_type rule_type;

	*rx_offloads = 0;
	*tx_offloads = 0;

	/* Check for inbound rules that use offloads and use this port */
	for (idx_sa = 0; idx_sa < nb_sa_in; idx_sa++) {
		rule = &sa_in[idx_sa];
		rule_type = ipsec_get_action_type(rule);
		if ((rule_type == RTE_SECURITY_ACTION_TYPE_INLINE_CRYPTO ||
				rule_type ==
				RTE_SECURITY_ACTION_TYPE_INLINE_PROTOCOL)
				&& rule->portid == port_id)
			*rx_offloads |= DEV_RX_OFFLOAD_SECURITY;
	}

	/* Check for outbound rules that use offloads and use this port */
	for (idx_sa = 0; idx_sa < nb_sa_out; idx_sa++) {
		rule = &sa_out[idx_sa];
		rule_type = ipsec_get_action_type(rule);
		if ((rule_type == RTE_SECURITY_ACTION_TYPE_INLINE_CRYPTO ||
				rule_type ==
				RTE_SECURITY_ACTION_TYPE_INLINE_PROTOCOL)
				&& rule->portid == port_id)
			*tx_offloads |= DEV_TX_OFFLOAD_SECURITY;
	}
	return 0;
}




//////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////
// sp4.c

/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2016 Intel Corporation
 */

/*
 * Security Policies
 */
#include <sys/types.h>
#include <netinet/in.h>
#include <netinet/ip.h>

#include <rte_acl.h>
#include <rte_ip.h>

// #include "ipsec.h"
// #include "parser.h"


#define MAX_ACL_RULE_NUM	1024

#define IPV4_DST_FROM_SP(acr) \
		(rte_cpu_to_be_32((acr).field[DST_FIELD_IPV4].value.u32))

#define IPV4_SRC_FROM_SP(acr) \
		(rte_cpu_to_be_32((acr).field[SRC_FIELD_IPV4].value.u32))

#define IPV4_DST_MASK_FROM_SP(acr) \
		((acr).field[DST_FIELD_IPV4].mask_range.u32)

#define IPV4_SRC_MASK_FROM_SP(acr) \
		((acr).field[SRC_FIELD_IPV4].mask_range.u32)

/*
 * Rule and trace formats definitions.
 */
enum {
	PROTO_FIELD_IPV4,
	SRC_FIELD_IPV4,
	DST_FIELD_IPV4,
	SRCP_FIELD_IPV4,
	DSTP_FIELD_IPV4,
	NUM_FIELDS_IPV4
};

/*
 * That effectively defines order of IPV4 classifications:
 *  - PROTO
 *  - SRC IP ADDRESS
 *  - DST IP ADDRESS
 *  - PORTS (SRC and DST)
 */
enum {
	RTE_ACL_IPV4_PROTO,
	RTE_ACL_IPV4_SRC,
	RTE_ACL_IPV4_DST,
	RTE_ACL_IPV4_PORTS,
	RTE_ACL_IPV4_NUM
};

static struct rte_acl_field_def ip4_defs[NUM_FIELDS_IPV4] = {
	{
	.type = RTE_ACL_FIELD_TYPE_BITMASK,
	.size = sizeof(uint8_t),
	.field_index = PROTO_FIELD_IPV4,
	.input_index = RTE_ACL_IPV4_PROTO,
	.offset = 0,
	},
	{
	.type = RTE_ACL_FIELD_TYPE_MASK,
	.size = sizeof(uint32_t),
	.field_index = SRC_FIELD_IPV4,
	.input_index = RTE_ACL_IPV4_SRC,
	.offset = offsetof(struct ip, ip_src) -	offsetof(struct ip, ip_p)
	},
	{
	.type = RTE_ACL_FIELD_TYPE_MASK,
	.size = sizeof(uint32_t),
	.field_index = DST_FIELD_IPV4,
	.input_index = RTE_ACL_IPV4_DST,
	.offset = offsetof(struct ip, ip_dst) - offsetof(struct ip, ip_p)
	},
	{
	.type = RTE_ACL_FIELD_TYPE_RANGE,
	.size = sizeof(uint16_t),
	.field_index = SRCP_FIELD_IPV4,
	.input_index = RTE_ACL_IPV4_PORTS,
	.offset = sizeof(struct ip) - offsetof(struct ip, ip_p)
	},
	{
	.type = RTE_ACL_FIELD_TYPE_RANGE,
	.size = sizeof(uint16_t),
	.field_index = DSTP_FIELD_IPV4,
	.input_index = RTE_ACL_IPV4_PORTS,
	.offset = sizeof(struct ip) - offsetof(struct ip, ip_p) +
		sizeof(uint16_t)
	},
};

RTE_ACL_RULE_DEF(acl4_rules, RTE_DIM(ip4_defs));

static struct acl4_rules acl4_rules_out[MAX_ACL_RULE_NUM];
static uint32_t nb_acl4_rules_out;

static struct acl4_rules acl4_rules_in[MAX_ACL_RULE_NUM];
static uint32_t nb_acl4_rules_in;

void
parse_sp4_tokens(char **tokens, uint32_t n_tokens,
	struct parse_status *status)
{
	struct acl4_rules *rule_ipv4 = NULL;

	uint32_t *ri = NULL; /* rule index */
	uint32_t ti = 0; /* token index */
	uint32_t tv;

	uint32_t esp_p = 0;
	uint32_t protect_p = 0;
	uint32_t bypass_p = 0;
	uint32_t discard_p = 0;
	uint32_t pri_p = 0;
	uint32_t src_p = 0;
	uint32_t dst_p = 0;
	uint32_t proto_p = 0;
	uint32_t sport_p = 0;
	uint32_t dport_p = 0;

	if (strcmp(tokens[1], "in") == 0) {
		ri = &nb_acl4_rules_in;

		APP_CHECK(*ri <= MAX_ACL_RULE_NUM - 1, status,
			"too many sp rules, abort insertion\n");
		if (status->status < 0)
			return;

		rule_ipv4 = &acl4_rules_in[*ri];

	} else if (strcmp(tokens[1], "out") == 0) {
		ri = &nb_acl4_rules_out;

		APP_CHECK(*ri <= MAX_ACL_RULE_NUM - 1, status,
			"too many sp rules, abort insertion\n");
		if (status->status < 0)
			return;

		rule_ipv4 = &acl4_rules_out[*ri];
	} else {
		APP_CHECK(0, status, "unrecognized input \"%s\", expect"
			" \"in\" or \"out\"\n", tokens[ti]);
		return;
	}

	rule_ipv4->data.category_mask = 1;

	for (ti = 2; ti < n_tokens; ti++) {
		if (strcmp(tokens[ti], "esp") == 0) {
			/* currently do nothing */
			APP_CHECK_PRESENCE(esp_p, tokens[ti], status);
			if (status->status < 0)
				return;
			esp_p = 1;
			continue;
		}

		if (strcmp(tokens[ti], "protect") == 0) {
			APP_CHECK_PRESENCE(protect_p, tokens[ti], status);
			if (status->status < 0)
				return;
			APP_CHECK(bypass_p == 0, status, "conflict item "
				"between \"%s\" and \"%s\"", tokens[ti],
				"bypass");
			if (status->status < 0)
				return;
			APP_CHECK(discard_p == 0, status, "conflict item "
				"between \"%s\" and \"%s\"", tokens[ti],
				"discard");
			if (status->status < 0)
				return;
			INCREMENT_TOKEN_INDEX(ti, n_tokens, status);
			if (status->status < 0)
				return;
			APP_CHECK_TOKEN_IS_NUM(tokens, ti, status);
			if (status->status < 0)
				return;

			tv = atoi(tokens[ti]);
			APP_CHECK(tv != DISCARD && tv != BYPASS, status,
				"invalid SPI: %s", tokens[ti]);
			if (status->status < 0)
				return;
			rule_ipv4->data.userdata = tv;

			protect_p = 1;
			continue;
		}

		if (strcmp(tokens[ti], "bypass") == 0) {
			APP_CHECK_PRESENCE(bypass_p, tokens[ti], status);
			if (status->status < 0)
				return;
			APP_CHECK(protect_p == 0, status, "conflict item "
				"between \"%s\" and \"%s\"", tokens[ti],
				"protect");
			if (status->status < 0)
				return;
			APP_CHECK(discard_p == 0, status, "conflict item "
				"between \"%s\" and \"%s\"", tokens[ti],
				"discard");
			if (status->status < 0)
				return;

			rule_ipv4->data.userdata = BYPASS;

			bypass_p = 1;
			continue;
		}

		if (strcmp(tokens[ti], "discard") == 0) {
			APP_CHECK_PRESENCE(discard_p, tokens[ti], status);
			if (status->status < 0)
				return;
			APP_CHECK(protect_p == 0, status, "conflict item "
				"between \"%s\" and \"%s\"", tokens[ti],
				"protect");
			if (status->status < 0)
				return;
			APP_CHECK(bypass_p == 0, status, "conflict item "
				"between \"%s\" and \"%s\"", tokens[ti],
				"discard");
			if (status->status < 0)
				return;

			rule_ipv4->data.userdata = DISCARD;

			discard_p = 1;
			continue;
		}

		if (strcmp(tokens[ti], "pri") == 0) {
			APP_CHECK_PRESENCE(pri_p, tokens[ti], status);
			if (status->status < 0)
				return;
			INCREMENT_TOKEN_INDEX(ti, n_tokens, status);
			if (status->status < 0)
				return;
			APP_CHECK_TOKEN_IS_NUM(tokens, ti, status);
			if (status->status < 0)
				return;

			rule_ipv4->data.priority = atoi(tokens[ti]);

			pri_p = 1;
			continue;
		}

		if (strcmp(tokens[ti], "src") == 0) {
			struct in_addr ip;
			uint32_t depth;

			APP_CHECK_PRESENCE(src_p, tokens[ti], status);
			if (status->status < 0)
				return;
			INCREMENT_TOKEN_INDEX(ti, n_tokens, status);
			if (status->status < 0)
				return;

			APP_CHECK(parse_ipv4_addr(tokens[ti], &ip,
				&depth) == 0, status, "unrecognized "
				"input \"%s\", expect valid ipv4 addr",
				tokens[ti]);
			if (status->status < 0)
				return;

			rule_ipv4->field[1].value.u32 =
				rte_bswap32(ip.s_addr);
			rule_ipv4->field[1].mask_range.u32 =
				depth;

			src_p = 1;
			continue;
		}

		if (strcmp(tokens[ti], "dst") == 0) {
			struct in_addr ip;
			uint32_t depth;

			APP_CHECK_PRESENCE(dst_p, tokens[ti], status);
			if (status->status < 0)
				return;
			INCREMENT_TOKEN_INDEX(ti, n_tokens, status);
			if (status->status < 0)
				return;
			APP_CHECK(parse_ipv4_addr(tokens[ti], &ip,
				&depth) == 0, status, "unrecognized "
				"input \"%s\", expect valid ipv4 addr",
				tokens[ti]);
			if (status->status < 0)
				return;

			rule_ipv4->field[2].value.u32 =
				rte_bswap32(ip.s_addr);
			rule_ipv4->field[2].mask_range.u32 =
				depth;

			dst_p = 1;
			continue;
		}

		if (strcmp(tokens[ti], "proto") == 0) {
			uint16_t low, high;

			APP_CHECK_PRESENCE(proto_p, tokens[ti], status);
			if (status->status < 0)
				return;
			INCREMENT_TOKEN_INDEX(ti, n_tokens, status);
			if (status->status < 0)
				return;

			APP_CHECK(parse_range(tokens[ti], &low, &high)
				== 0, status, "unrecognized input \"%s\""
				", expect \"from:to\"", tokens[ti]);
			if (status->status < 0)
				return;
			APP_CHECK(low <= 0xff, status, "proto low "
				"over-limit");
			if (status->status < 0)
				return;
			APP_CHECK(high <= 0xff, status, "proto high "
				"over-limit");
			if (status->status < 0)
				return;

			rule_ipv4->field[0].value.u8 = (uint8_t)low;
			rule_ipv4->field[0].mask_range.u8 = (uint8_t)high;

			proto_p = 1;
			continue;
		}

		if (strcmp(tokens[ti], "sport") == 0) {
			uint16_t port_low, port_high;

			APP_CHECK_PRESENCE(sport_p, tokens[ti], status);
			if (status->status < 0)
				return;
			INCREMENT_TOKEN_INDEX(ti, n_tokens, status);
			if (status->status < 0)
				return;

			APP_CHECK(parse_range(tokens[ti], &port_low,
				&port_high) == 0, status, "unrecognized "
				"input \"%s\", expect \"port_from:"
				"port_to\"", tokens[ti]);
			if (status->status < 0)
				return;

			rule_ipv4->field[3].value.u16 = port_low;
			rule_ipv4->field[3].mask_range.u16 = port_high;

			sport_p = 1;
			continue;
		}

		if (strcmp(tokens[ti], "dport") == 0) {
			uint16_t port_low, port_high;

			APP_CHECK_PRESENCE(dport_p, tokens[ti], status);
			if (status->status < 0)
				return;
			INCREMENT_TOKEN_INDEX(ti, n_tokens, status);
			if (status->status < 0)
				return;

			APP_CHECK(parse_range(tokens[ti], &port_low,
				&port_high) == 0, status, "unrecognized "
				"input \"%s\", expect \"port_from:"
				"port_to\"", tokens[ti]);
			if (status->status < 0)
				return;

			rule_ipv4->field[4].value.u16 = port_low;
			rule_ipv4->field[4].mask_range.u16 = port_high;

			dport_p = 1;
			continue;
		}

		/* unrecognizeable input */
		APP_CHECK(0, status, "unrecognized input \"%s\"",
			tokens[ti]);
		return;
	}

	/* check if argument(s) are missing */
	APP_CHECK(esp_p == 1, status, "missing argument \"esp\"");
	if (status->status < 0)
		return;

	APP_CHECK(protect_p | bypass_p | discard_p, status, "missing "
		"argument \"protect\", \"bypass\", or \"discard\"");
	if (status->status < 0)
		return;

	*ri = *ri + 1;
}

static void
print_one_ip4_rule(const struct acl4_rules *rule, int32_t extra)
{
	uint8_t a, b, c, d;

	uint32_t_to_char(rule->field[SRC_FIELD_IPV4].value.u32,
			&a, &b, &c, &d);
	printf("%hhu.%hhu.%hhu.%hhu/%u ", a, b, c, d,
			rule->field[SRC_FIELD_IPV4].mask_range.u32);
	uint32_t_to_char(rule->field[DST_FIELD_IPV4].value.u32,
			&a, &b, &c, &d);
	printf("%hhu.%hhu.%hhu.%hhu/%u ", a, b, c, d,
			rule->field[DST_FIELD_IPV4].mask_range.u32);
	printf("%hu : %hu %hu : %hu 0x%hhx/0x%hhx ",
		rule->field[SRCP_FIELD_IPV4].value.u16,
		rule->field[SRCP_FIELD_IPV4].mask_range.u16,
		rule->field[DSTP_FIELD_IPV4].value.u16,
		rule->field[DSTP_FIELD_IPV4].mask_range.u16,
		rule->field[PROTO_FIELD_IPV4].value.u8,
		rule->field[PROTO_FIELD_IPV4].mask_range.u8);
	if (extra)
		printf("0x%x-0x%x-0x%x ",
			rule->data.category_mask,
			rule->data.priority,
			rule->data.userdata);
}

static inline void
dump_ip4_rules(const struct acl4_rules *rule, int32_t num, int32_t extra)
{
	int32_t i;

	for (i = 0; i < num; i++, rule++) {
		printf("\t%d:", i + 1);
		print_one_ip4_rule(rule, extra);
		printf("\n");
	}
}

static struct rte_acl_ctx *
acl4_init(const char *name, int32_t socketid, const struct acl4_rules *rules,
		uint32_t rules_nb)
{
	printf("RIDER REPORT: acl4_init function\n"); fflush(stdout);
	char s[PATH_MAX];
	struct rte_acl_param acl_param;
	struct rte_acl_config acl_build_param;
	struct rte_acl_ctx *ctx;

	printf("Creating SP context with %u max rules\n", MAX_ACL_RULE_NUM);
	
	memset(&acl_param, 0, sizeof(acl_param));
	
	
	/* Create ACL contexts */
	snprintf(s, sizeof(s), "%s_%d", name, socketid);

	printf("IPv4 %s entries [%u]:\n", s, rules_nb);
	dump_ip4_rules(rules, rules_nb, 1);

	acl_param.name = s;
	acl_param.socket_id = socketid;
	acl_param.rule_size = RTE_ACL_RULE_SZ(RTE_DIM(ip4_defs));
	acl_param.max_rule_num = MAX_ACL_RULE_NUM;
	ctx = rte_acl_create(&acl_param);
	if (ctx == NULL)
		rte_exit(EXIT_FAILURE, "Failed to create ACL context\n");
	
	if (rte_acl_add_rules(ctx, (const struct rte_acl_rule *)rules,
				rules_nb) < 0)
		rte_exit(EXIT_FAILURE, "add rules failed\n");
	
	/* Perform builds */
	memset(&acl_build_param, 0, sizeof(acl_build_param));

	acl_build_param.num_categories = DEFAULT_MAX_CATEGORIES;
	acl_build_param.num_fields = RTE_DIM(ip4_defs);
	memcpy(&acl_build_param.defs, ip4_defs, sizeof(ip4_defs));
	
	if (rte_acl_build(ctx, &acl_build_param) != 0)
		rte_exit(EXIT_FAILURE, "Failed to build ACL trie\n");

	rte_acl_dump(ctx);

	return ctx;
}


/*
 * check that for each rule it's SPI has a correspondent entry in SAD
 */
static int
check_spi_value(int inbound)
{
	uint32_t i, num, spi;
	const struct acl4_rules *acr;

	if (inbound != 0) {
		acr = acl4_rules_in;
		num = nb_acl4_rules_in;
	} else {
		acr = acl4_rules_out;
		num = nb_acl4_rules_out;
	}

	for (i = 0; i != num; i++) {
		spi = acr[i].data.userdata;
		if (spi != DISCARD && spi != BYPASS &&
				sa_spi_present(spi, inbound) < 0) {
			RTE_LOG(ERR, IPSEC, "SPI %u is not present in SAD\n",
				spi);
			return -ENOENT;
		}
	}

	return 0;
}

void
sp4_init(struct socket_ctx *ctx, int32_t socket_id)
{
	printf("RIDER REPORT: sp4_init function\n"); fflush(stdout);
	const char *name;

	if (ctx == NULL)
		rte_exit(EXIT_FAILURE, "NULL context.\n");

	if (ctx->sp_ip4_in != NULL)
		rte_exit(EXIT_FAILURE, "Inbound SP DB for socket %u already "
				"initialized\n", socket_id);

	if (ctx->sp_ip4_out != NULL)
		rte_exit(EXIT_FAILURE, "Outbound SP DB for socket %u already "
				"initialized\n", socket_id);

	if (check_spi_value(1) < 0)
		rte_exit(EXIT_FAILURE,
			"Inbound IPv4 SP DB has unmatched in SAD SPIs\n");

	if (check_spi_value(0) < 0)
		rte_exit(EXIT_FAILURE,
			"Outbound IPv4 SP DB has unmatched in SAD SPIs\n");

	if (nb_acl4_rules_in > 0) {
		name = "sp_ip4_in";
		ctx->sp_ip4_in = (struct sp_ctx *)acl4_init(name,
			socket_id, acl4_rules_in, nb_acl4_rules_in);
	} else
		RTE_LOG(WARNING, IPSEC, "No IPv4 SP Inbound rule "
			"specified\n");

	if (nb_acl4_rules_out > 0) {
		name = "sp_ip4_out";
		ctx->sp_ip4_out = (struct sp_ctx *)acl4_init(name,
			socket_id, acl4_rules_out, nb_acl4_rules_out);
	} else
		RTE_LOG(WARNING, IPSEC, "No IPv4 SP Outbound rule "
			"specified\n");
}

/*
 * Search though SP rules for given SPI.
 */
int
sp4_spi_present(uint32_t spi, int inbound, struct ip_addr ip_addr[2],
			uint32_t mask[2])
{
	uint32_t i, num;
	const struct acl4_rules *acr;

	if (inbound != 0) {
		acr = acl4_rules_in;
		num = nb_acl4_rules_in;
	} else {
		acr = acl4_rules_out;
		num = nb_acl4_rules_out;
	}

	for (i = 0; i != num; i++) {
		if (acr[i].data.userdata == spi) {
			if (NULL != ip_addr && NULL != mask) {
				ip_addr[0].ip.ip4 = IPV4_SRC_FROM_SP(acr[i]);
				ip_addr[1].ip.ip4 = IPV4_DST_FROM_SP(acr[i]);
				mask[0] = IPV4_SRC_MASK_FROM_SP(acr[i]);
				mask[1] = IPV4_DST_MASK_FROM_SP(acr[i]);
			}
			return i;
		}
	}

	return -ENOENT;
}



//////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////
// sp6.c

/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2016 Intel Corporation
 */

/*
 * Security Policies
 */
#include <sys/types.h>
#include <netinet/in.h>
#include <netinet/ip6.h>

#include <rte_acl.h>
#include <rte_ip.h>

// #include "ipsec.h"
// #include "parser.h"

#define MAX_ACL_RULE_NUM	1024

#define IPV6_FROM_SP(acr, fidx_low, fidx_high) \
		(((uint64_t)(acr).field[(fidx_high)].value.u32 << 32) | \
		(acr).field[(fidx_low)].value.u32)

#define IPV6_DST_FROM_SP(addr, acr) do {\
		(addr).ip.ip6.ip6[0] = rte_cpu_to_be_64(IPV6_FROM_SP((acr), \
						IP6_DST1, IP6_DST0));\
		(addr).ip.ip6.ip6[1] = rte_cpu_to_be_64(IPV6_FROM_SP((acr), \
						IP6_DST3, IP6_DST2));\
		} while (0)

#define IPV6_SRC_FROM_SP(addr, acr) do {\
		(addr).ip.ip6.ip6[0] = rte_cpu_to_be_64(IPV6_FROM_SP((acr), \
							IP6_SRC1, IP6_SRC0));\
		(addr).ip.ip6.ip6[1] = rte_cpu_to_be_64(IPV6_FROM_SP((acr), \
							IP6_SRC3, IP6_SRC2));\
		} while (0)

#define IPV6_DST_MASK_FROM_SP(mask, acr) \
		((mask) = (acr).field[IP6_DST0].mask_range.u32 + \
			(acr).field[IP6_DST1].mask_range.u32 + \
			(acr).field[IP6_DST2].mask_range.u32 + \
			(acr).field[IP6_DST3].mask_range.u32)

#define IPV6_SRC_MASK_FROM_SP(mask, acr) \
		((mask) = (acr).field[IP6_SRC0].mask_range.u32 + \
			(acr).field[IP6_SRC1].mask_range.u32 + \
			(acr).field[IP6_SRC2].mask_range.u32 + \
			(acr).field[IP6_SRC3].mask_range.u32)

enum {
	IP6_PROTO,
	IP6_SRC0,
	IP6_SRC1,
	IP6_SRC2,
	IP6_SRC3,
	IP6_DST0,
	IP6_DST1,
	IP6_DST2,
	IP6_DST3,
	IP6_SRCP,
	IP6_DSTP,
	IP6_NUM
};

#define IP6_ADDR_SIZE 16

static struct rte_acl_field_def ip6_defs[IP6_NUM] = {
	{
	.type = RTE_ACL_FIELD_TYPE_BITMASK,
	.size = sizeof(uint8_t),
	.field_index = IP6_PROTO,
	.input_index = IP6_PROTO,
	.offset = 0,
	},
	{
	.type = RTE_ACL_FIELD_TYPE_MASK,
	.size = 4,
	.field_index = IP6_SRC0,
	.input_index = IP6_SRC0,
	.offset = 2
	},
	{
	.type = RTE_ACL_FIELD_TYPE_MASK,
	.size = 4,
	.field_index = IP6_SRC1,
	.input_index = IP6_SRC1,
	.offset = 6
	},
	{
	.type = RTE_ACL_FIELD_TYPE_MASK,
	.size = 4,
	.field_index = IP6_SRC2,
	.input_index = IP6_SRC2,
	.offset = 10
	},
	{
	.type = RTE_ACL_FIELD_TYPE_MASK,
	.size = 4,
	.field_index = IP6_SRC3,
	.input_index = IP6_SRC3,
	.offset = 14
	},
	{
	.type = RTE_ACL_FIELD_TYPE_MASK,
	.size = 4,
	.field_index = IP6_DST0,
	.input_index = IP6_DST0,
	.offset = 18
	},
	{
	.type = RTE_ACL_FIELD_TYPE_MASK,
	.size = 4,
	.field_index = IP6_DST1,
	.input_index = IP6_DST1,
	.offset = 22
	},
	{
	.type = RTE_ACL_FIELD_TYPE_MASK,
	.size = 4,
	.field_index = IP6_DST2,
	.input_index = IP6_DST2,
	.offset = 26
	},
	{
	.type = RTE_ACL_FIELD_TYPE_MASK,
	.size = 4,
	.field_index = IP6_DST3,
	.input_index = IP6_DST3,
	.offset = 30
	},
	{
	.type = RTE_ACL_FIELD_TYPE_RANGE,
	.size = sizeof(uint16_t),
	.field_index = IP6_SRCP,
	.input_index = IP6_SRCP,
	.offset = 34
	},
	{
	.type = RTE_ACL_FIELD_TYPE_RANGE,
	.size = sizeof(uint16_t),
	.field_index = IP6_DSTP,
	.input_index = IP6_SRCP,
	.offset = 36
	}
};

RTE_ACL_RULE_DEF(acl6_rules, RTE_DIM(ip6_defs));

static struct acl6_rules acl6_rules_out[MAX_ACL_RULE_NUM];
static uint32_t nb_acl6_rules_out;

static struct acl6_rules acl6_rules_in[MAX_ACL_RULE_NUM];
static uint32_t nb_acl6_rules_in;



/*
 * Search though SP rules for given SPI.
 */
int
sp6_spi_present(uint32_t spi, int inbound, struct ip_addr ip_addr[2],
			uint32_t mask[2])
{
	uint32_t i, num;
	const struct acl6_rules *acr;

	if (inbound != 0) {
		acr = acl6_rules_in;
		num = nb_acl6_rules_in;
	} else {
		acr = acl6_rules_out;
		num = nb_acl6_rules_out;
	}

	for (i = 0; i != num; i++) {
		if (acr[i].data.userdata == spi) {
			if (NULL != ip_addr && NULL != mask) {
				IPV6_SRC_FROM_SP(ip_addr[0], acr[i]);
				IPV6_DST_FROM_SP(ip_addr[1], acr[i]);
				IPV6_SRC_MASK_FROM_SP(mask[0], acr[i]);
				IPV6_DST_MASK_FROM_SP(mask[1], acr[i]);
			}
			return i;
		}
	}

	return -ENOENT;
}