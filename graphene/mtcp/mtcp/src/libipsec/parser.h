/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2016 Intel Corporation
 */

#include <sys/types.h>
#include <netinet/in.h>
#include <netinet/ip.h>

#include <rte_flow.h>

#include "rte_ipsec.h"

#ifndef __PARSER_H
#define __PARSER_H

#define RTE_LOGTYPE_IPSEC       RTE_LOGTYPE_USER1
#define RTE_LOGTYPE_IPSEC_ESP   RTE_LOGTYPE_USER2
#define RTE_LOGTYPE_IPSEC_IPIP  RTE_LOGTYPE_USER3

#define IP6_VERSION (6)

#define IPSEC_SA_OFFLOAD_FALLBACK_FLAG (1)


#if RTE_BYTE_ORDER != RTE_LITTLE_ENDIAN
#define __BYTES_TO_UINT64(a, b, c, d, e, f, g, h) \
	(((uint64_t)((a) & 0xff) << 56) | \
	((uint64_t)((b) & 0xff) << 48) | \
	((uint64_t)((c) & 0xff) << 40) | \
	((uint64_t)((d) & 0xff) << 32) | \
	((uint64_t)((e) & 0xff) << 24) | \
	((uint64_t)((f) & 0xff) << 16) | \
	((uint64_t)((g) & 0xff) << 8)  | \
	((uint64_t)(h) & 0xff))
#else
#define __BYTES_TO_UINT64(a, b, c, d, e, f, g, h) \
	(((uint64_t)((h) & 0xff) << 56) | \
	((uint64_t)((g) & 0xff) << 48) | \
	((uint64_t)((f) & 0xff) << 40) | \
	((uint64_t)((e) & 0xff) << 32) | \
	((uint64_t)((d) & 0xff) << 24) | \
	((uint64_t)((c) & 0xff) << 16) | \
	((uint64_t)((b) & 0xff) << 8) | \
	((uint64_t)(a) & 0xff))
#endif
#define ETHADDR(a, b, c, d, e, f) (__BYTES_TO_UINT64(a, b, c, d, e, f, 0, 0))

#define ETHADDR_TO_UINT64(addr) __BYTES_TO_UINT64( \
		(addr)->addr_bytes[0], (addr)->addr_bytes[1], \
		(addr)->addr_bytes[2], (addr)->addr_bytes[3], \
		(addr)->addr_bytes[4], (addr)->addr_bytes[5], \
		0, 0)


#define IV_OFFSET		(sizeof(struct rte_crypto_op) + \
				sizeof(struct rte_crypto_sym_op))


#define uint32_t_to_char(ip, a, b, c, d) do {\
		*a = (uint8_t)(ip >> 24 & 0xff);\
		*b = (uint8_t)(ip >> 16 & 0xff);\
		*c = (uint8_t)(ip >> 8 & 0xff);\
		*d = (uint8_t)(ip & 0xff);\
	} while (0)

struct parse_status {
	int status;
	char parse_msg[256];
};

enum {
	IPSEC_SESSION_PRIMARY = 0,
	IPSEC_SESSION_FALLBACK = 1,
	IPSEC_SESSION_MAX
};

#define MAX_KEY_SIZE		32

/*
 * application wide SA parameters
 */
struct app_sa_prm {
	uint32_t enable; /* use librte_ipsec API for ipsec pkt processing */
	uint32_t window_size; /* replay window size */
	uint32_t enable_esn;  /* enable/disable ESN support */
	uint64_t flags;       /* rte_ipsec_sa_prm.flags */
};

static struct app_sa_prm app_sa_prm = {.enable = 1};


#define	APP_CHECK(exp, st, fmt, ...)					\
do {									\
	if (!(exp)) {							\
		sprintf((st)->parse_msg, fmt "\n",			\
			## __VA_ARGS__);				\
		(st)->status = -1;					\
	} else								\
		(st)->status = 0;					\
} while (0)

#define APP_CHECK_PRESENCE(val, str, status)				\
	APP_CHECK(val == 0, status,					\
		"item \"%s\" already present", str)

#define APP_CHECK_TOKEN_EQUAL(tokens, index, ref, status)		\
	APP_CHECK(strcmp(tokens[index], ref) == 0, status,		\
		"unrecognized input \"%s\": expect \"%s\"\n",		\
		tokens[index], ref)

/* port/source ethernet addr and destination ethernet addr */
struct ethaddr_info {
	uint64_t src, dst;
};

static struct ethaddr_info ethaddr_tbl[RTE_MAX_ETHPORTS] = {
	{ 0, ETHADDR(0x00, 0x16, 0x3e, 0x7e, 0x94, 0x9a) },
	{ 0, ETHADDR(0x00, 0x16, 0x3e, 0x22, 0xa1, 0xd9) },
	{ 0, ETHADDR(0x00, 0x16, 0x3e, 0x08, 0x69, 0x26) },
	{ 0, ETHADDR(0x00, 0x16, 0x3e, 0x49, 0x9e, 0xdd) }
};

#define DEFAULT_MAX_CATEGORIES	1

#define IPSEC_SA_MAX_ENTRIES (128) /* must be power of 2, max 2 power 30 */
#define SPI2IDX(spi) (spi & (IPSEC_SA_MAX_ENTRIES - 1))
#define INVALID_SPI (0)

#define DISCARD	INVALID_SPI
#define BYPASS	UINT32_MAX


struct ip_addr {
	union {
		uint32_t ip4;
		union {
			uint64_t ip6[2];
			uint8_t ip6_b[16];
		} ip6;
	} ip;
};

struct ipsec_sa {
	struct rte_ipsec_session sessions[IPSEC_SESSION_MAX];
	uint32_t spi;
	uint32_t cdev_id_qp;
	uint64_t seq;
	uint32_t salt;
	uint32_t fallback_sessions;
	enum rte_crypto_cipher_algorithm cipher_algo;
	enum rte_crypto_auth_algorithm auth_algo;
	enum rte_crypto_aead_algorithm aead_algo;
	uint16_t digest_len;
	uint16_t iv_len;
	uint16_t block_size;
	uint16_t flags;
#define IP4_TUNNEL (1 << 0)
#define IP6_TUNNEL (1 << 1)
#define TRANSPORT  (1 << 2)
#define IP4_TRANSPORT (1 << 3)
#define IP6_TRANSPORT (1 << 4)
	struct ip_addr src;
	struct ip_addr dst;
	uint8_t cipher_key[MAX_KEY_SIZE];
	uint16_t cipher_key_len;
	uint8_t auth_key[MAX_KEY_SIZE];
	uint16_t auth_key_len;
	uint16_t aad_len;
	union {
		struct rte_crypto_sym_xform *xforms;
		struct rte_security_ipsec_xform *sec_xform;
	};
	enum rte_security_ipsec_sa_direction direction;
	uint16_t portid;

#define MAX_RTE_FLOW_PATTERN (4)
#define MAX_RTE_FLOW_ACTIONS (3)
	struct rte_flow_item pattern[MAX_RTE_FLOW_PATTERN];
	struct rte_flow_action action[MAX_RTE_FLOW_ACTIONS];
	struct rte_flow_attr attr;
	union {
		struct rte_flow_item_ipv4 ipv4_spec;
		struct rte_flow_item_ipv6 ipv6_spec;
	};
	struct rte_flow_item_esp esp_spec;
	struct rte_flow *flow;
	struct rte_security_session_conf sess_conf;
} __rte_cache_aligned;


struct sa_ctx {
	void *satbl; /* pointer to array of rte_ipsec_sa objects*/
	// rider added 2020-9-28 to store sa size
	uint16_t sasize;
	//
	// rider added 2020-12-9 to store pointer list of struct rte_ipsec_sa* ipsec_sa
	struct rte_ipsec_sa* ipsec_sa_ptr_list[IPSEC_SA_MAX_ENTRIES];
	//
	struct ipsec_sa sa[IPSEC_SA_MAX_ENTRIES];
	union {
		struct {
			struct rte_crypto_sym_xform a;
			struct rte_crypto_sym_xform b;
		};
	} xf[IPSEC_SA_MAX_ENTRIES];
};

struct ipsec_mbuf_metadata {
	struct ipsec_sa *sa;
	struct rte_crypto_op cop;
	struct rte_crypto_sym_op sym_cop;
	uint8_t buf[32];
} __rte_cache_aligned;


struct socket_ctx {
	struct sa_ctx *sa_in;
	struct sa_ctx *sa_out;
	struct sp_ctx *sp_ip4_in;
	struct sp_ctx *sp_ip4_out;
	struct sp_ctx *sp_ip6_in;
	struct sp_ctx *sp_ip6_out;
	struct rt_ctx *rt_ip4;
	struct rt_ctx *rt_ip6;
	struct rte_mempool *mbuf_pool;
	struct rte_mempool *mbuf_pool_indir;
	struct rte_mempool *session_pool;
	struct rte_mempool *session_priv_pool;
};


#define IS_TRANSPORT(flags) ((flags) & TRANSPORT)

#define IS_TUNNEL(flags) ((flags) & (IP4_TUNNEL | IP6_TUNNEL))

#define IS_IP4(flags) ((flags) & (IP4_TUNNEL | IP4_TRANSPORT))

#define IS_IP6(flags) ((flags) & (IP6_TUNNEL | IP6_TRANSPORT))

#define IS_IP4_TUNNEL(flags) ((flags) & IP4_TUNNEL)

#define IS_IP6_TUNNEL(flags) ((flags) & IP6_TUNNEL)

/*
 * Macro for getting ipsec_sa flags statuses without version of protocol
 * used for transport (IP4_TRANSPORT and IP6_TRANSPORT flags).
 */
#define WITHOUT_TRANSPORT_VERSION(flags) \
		((flags) & (IP4_TUNNEL | \
			IP6_TUNNEL | \
			TRANSPORT))

static inline int
is_str_num(const char *str)
{
	uint32_t i;

	for (i = 0; i < strlen(str); i++)
		if (!isdigit(str[i]))
			return -1;

	return 0;
}

#define APP_CHECK_TOKEN_IS_NUM(tokens, index, status)			\
	APP_CHECK(is_str_num(tokens[index]) == 0, status,		\
	"input \"%s\" is not valid number string", tokens[index])


#define INCREMENT_TOKEN_INDEX(index, max_num, status)			\
do {									\
	APP_CHECK(index + 1 < max_num, status, "reaching the end of "	\
		"the token array");					\
	index++;							\
} while (0)

static inline struct rte_ipsec_session *
ipsec_get_primary_session(struct ipsec_sa *sa)
{
	return &sa->sessions[IPSEC_SESSION_PRIMARY];
}

static inline struct rte_ipsec_session *
ipsec_get_fallback_session(struct ipsec_sa *sa)
{
	return &sa->sessions[IPSEC_SESSION_FALLBACK];
}

static inline struct ipsec_mbuf_metadata *
get_priv(struct rte_mbuf *m)
{
	return rte_mbuf_to_priv(m);
}

static inline enum rte_security_session_action_type
ipsec_get_action_type(struct ipsec_sa *sa)
{
	struct rte_ipsec_session *ips;
	ips = ipsec_get_primary_session(sa);
	return ips->type;
}


int
parse_ipv4_addr(const char *token, struct in_addr *ipv4, uint32_t *mask);

int
parse_ipv6_addr(const char *token, struct in6_addr *ipv6, uint32_t *mask);

int
parse_range(const char *token, uint16_t *low, uint16_t *high);

void
parse_sp4_tokens(char **tokens, uint32_t n_tokens,
	struct parse_status *status);

void
parse_sp6_tokens(char **tokens, uint32_t n_tokens,
	struct parse_status *status);

void
parse_sa_tokens(char **tokens, uint32_t n_tokens,
	struct parse_status *status);

void
parse_rt_tokens(char **tokens, uint32_t n_tokens,
	struct parse_status *status);

int
parse_cfg_file(const char *cfg_filename);


/*
 * Search through SP rules for given SPI.
 * Returns first rule index if found(greater or equal then zero),
 * or -ENOENT otherwise.
 */
int
sp4_spi_present(uint32_t spi, int inbound, struct ip_addr ip_addr[2],
			uint32_t mask[2]);
int
sp6_spi_present(uint32_t spi, int inbound, struct ip_addr ip_addr[2],
			uint32_t mask[2]);

void
sa_init(struct socket_ctx *ctx, int32_t socket_id);

void
sp4_init(struct socket_ctx *ctx, int32_t socket_id);

#endif
