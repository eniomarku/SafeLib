#ifndef _MTCP_IPSEC_H_
#define _MTCP_IPSEC_H_

// rider added 2020-9-28 for ipsec
#include "rte_ipsec_sa.h"
#include <rte_acl.h>
#include "parser.h"
#include "sa.h"

static char *cfgfile = "ipsec.cfg";

struct socket_ctx* global_socket_ctx;

struct tcp_stream_info {
    uint32_t addr1;
    uint32_t addr2;
    uint16_t port1;
    uint16_t port2;
} g_stream_infos[8192];
void * g_stream_ctxt[8192];
int g_stream_cnt;


#ifdef IN_ENCLAVE
struct rte_mbuf * get_encrypted_packet_buf(struct mtcp_thread_context *ctxt);
struct rte_mbuf * get_plain_packet_buf(struct mtcp_thread_context *ctxt);
struct rte_mbuf * init_plain_packet_buf(struct mtcp_thread_context *ctxt);
#endif



#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <intel-ipsec-mb.h>


// void hexdump(FILE *fp,
//              const char *msg,
//              const void *p,
//              size_t len)
// {
//     unsigned int i, out, ofs;
//     const unsigned char *data = p;

//     fprintf(fp, "%s\n", msg);

//     ofs = 0;
//     while (ofs < len)
//     {
//         char line[120];

//         out = snprintf(line, sizeof(line), "%08x:", ofs);
//         for (i = 0; ((ofs + i) < len) && (i < 16); i++)
//             out += snprintf(line + out, sizeof(line) - out,
//                             " %02x", (data[ofs + i] & 0xff));
//         for (; i <= 16; i++)
//             out += snprintf(line + out, sizeof(line) - out, " | ");
//         for (i = 0; (ofs < len) && (i < 16); i++, ofs++)
//         {
//             unsigned char c = data[ofs];

//             if ((c < ' ') || (c > '~'))
//                 c = '.';
//             out += snprintf(line + out,
//                             sizeof(line) - out, "%c", c);
//         }
//         fprintf(fp, "%s\n", line);
//     }
// }



#define ETHERNET_HEADER_SIZE            (14)
#define IP_HEADER_SIZE                  (20)
#define UDP_HEADER_SIZE                 (8)
#define ESP_HEADER_SIZE                 (24)


// algorithm parameters for hmac-sha1-98 aes-cbc-128
#define BLOCK_SIZE                      (64)
#define HMAC_SHA1_96_KEY_SIZE           (20)
#define HMAC_SHA1_96_DIGEST_SIZE        (12)
#define HMAC_SHA1_96_ENC_START_OFFSET   (IP_HEADER_SIZE) // ignore IP header
#define HMAC_SHA1_96_ENC_END_OFFSET		(HMAC_SHA1_96_DIGEST_SIZE) // ignore authentication data
// rider added 2020-12-9 UDP_HEADER_SIZE for esp over udp
#define HMAC_SHA1_96_DEC_START_OFFSET   (UDP_HEADER_SIZE + IP_HEADER_SIZE + ETHERNET_HEADER_SIZE) // ignore UDP, IP and Ethernet header
#define HMAC_SHA1_96_DEC_END_OFFSET     (HMAC_SHA1_96_DIGEST_SIZE) // ignore authentication data
#define AES_CBC_128_KEY_SIZE            (16)
#define AES_CBC_128_IV_SIZE             (16)
#define AES_CBC_128_IV_ENC_START_OFFSET		(8 + IP_HEADER_SIZE) // position of initial vector in ESP header
// rider added 2020-12-9 UDP_HEADER_SIZE for esp over udp
#define AES_CBC_128_IV_DEC_START_OFFSET		(8 + UDP_HEADER_SIZE + IP_HEADER_SIZE + ETHERNET_HEADER_SIZE) // position of initial vector in ESP header
#define AES_CBC_128_ENC_START_OFFSET    (ESP_HEADER_SIZE + IP_HEADER_SIZE) // ignore ESP and IP header
#define AES_CBC_128_ENC_END_OFFSET		(HMAC_SHA1_96_DIGEST_SIZE) // ignore authentication data
// rider added 2020-12-9 UDP_HEADER_SIZE for esp over udp
#define AES_CBC_128_DEC_START_OFFSET    (ESP_HEADER_SIZE + UDP_HEADER_SIZE + IP_HEADER_SIZE + ETHERNET_HEADER_SIZE) // ignore ESP, UDP, IP and Ethernet header
#define AES_CBC_128_DEC_END_OFFSET		(HMAC_SHA1_96_DIGEST_SIZE) // ignore authentication data
//

static int
esp_encrypt_aes_cbc_128_hmac_sha1_96(struct MB_MGR *mb_mgr, char *packet, int packet_len, uint8_t *cipherkey, uint8_t *authkey)
{
    struct JOB_AES_HMAC *job;

    int i, ret;

    uint8_t key[BLOCK_SIZE];
    uint8_t buf[BLOCK_SIZE];
    DECLARE_ALIGNED(uint32_t enc_keys[15 * 4], 16);
    DECLARE_ALIGNED(uint32_t dec_keys[15 * 4], 16);
    DECLARE_ALIGNED(uint8_t ipad_hash[128], 16);
    DECLARE_ALIGNED(uint8_t opad_hash[128], 16);
    int key_len = 0;

    /* prepare the cipher key */
    switch (AES_CBC_128_KEY_SIZE)
    {
    case 16:
        IMB_AES_KEYEXP_128(mb_mgr, cipherkey,
                           enc_keys, dec_keys);
        break;
    case 24:
        IMB_AES_KEYEXP_192(mb_mgr, cipherkey,
                           enc_keys, dec_keys);
        break;
    case 32:
    default:
        IMB_AES_KEYEXP_256(mb_mgr, cipherkey,
                           enc_keys, dec_keys);
        break;
    }

    /* prepare the key */
    memset(key, 0, sizeof(key));
    if (HMAC_SHA1_96_KEY_SIZE <= BLOCK_SIZE)
    {
        memcpy(key, authkey, HMAC_SHA1_96_KEY_SIZE);
        key_len = HMAC_SHA1_96_KEY_SIZE;
    }
    else
    {
        IMB_SHA1(mb_mgr, authkey, HMAC_SHA1_96_KEY_SIZE, key);
        key_len = HMAC_SHA1_96_KEY_SIZE;
    }

    /* compute ipad hash */
    memset(buf, 0x36, sizeof(buf));
    for (i = 0; i < key_len; i++)
        buf[i] ^= key[i];
    IMB_SHA1_ONE_BLOCK(mb_mgr, buf, ipad_hash);

    /* compute opad hash */
    memset(buf, 0x5c, sizeof(buf));
    for (i = 0; i < key_len; i++)
        buf[i] ^= key[i];
    IMB_SHA1_ONE_BLOCK(mb_mgr, buf, opad_hash);

    /* empty the manager */
    while ((job = IMB_FLUSH_JOB(mb_mgr)) != NULL)
        ;

    job = IMB_GET_NEXT_JOB(mb_mgr);
    job->aes_enc_key_expanded = enc_keys;
    job->aes_dec_key_expanded = dec_keys;
    job->cipher_direction = ENCRYPT;
    job->chain_order = CIPHER_HASH;
    job->dst = (uint8_t *)(packet + AES_CBC_128_ENC_START_OFFSET);
    job->aes_key_len_in_bytes = AES_CBC_128_KEY_SIZE;
    job->auth_tag_output = (uint8_t *)(packet + packet_len - HMAC_SHA1_96_DIGEST_SIZE);
    job->auth_tag_output_len_in_bytes = HMAC_SHA1_96_DIGEST_SIZE;
    job->iv = (uint8_t *)(packet + AES_CBC_128_IV_ENC_START_OFFSET);
    job->iv_len_in_bytes = AES_CBC_128_IV_SIZE;
    job->src = (uint8_t *)packet;
    job->cipher_start_src_offset_in_bytes = AES_CBC_128_ENC_START_OFFSET;
    job->msg_len_to_cipher_in_bytes = packet_len - AES_CBC_128_ENC_START_OFFSET - AES_CBC_128_ENC_END_OFFSET;
    job->hash_start_src_offset_in_bytes = HMAC_SHA1_96_ENC_START_OFFSET;
    job->msg_len_to_hash_in_bytes = packet_len - HMAC_SHA1_96_ENC_START_OFFSET - HMAC_SHA1_96_ENC_END_OFFSET;
    job->u.HMAC._hashed_auth_key_xor_ipad = ipad_hash;
    job->u.HMAC._hashed_auth_key_xor_opad = opad_hash;
    job->cipher_mode = CBC;
    job->hash_alg = SHA1;

    job = IMB_SUBMIT_JOB(mb_mgr);

    job = IMB_FLUSH_JOB(mb_mgr);

    if (job == NULL)
    {
        // printf("Error: job cannot be returned. %d\n", __LINE__); fflush(stdout);
        return -1;
    }
    else if (job->status != STS_COMPLETED)
    {
        // printf("Error: error status of job line:%d job error status:%d\n", __LINE__, job->status); fflush(stdout);
        return -1;
    }

    return 0;
}


static int
esp_decrypt_aes_cbc_128_hmac_sha1_96(struct MB_MGR *mb_mgr, char *packet, int packet_len, uint8_t *cipherkey, uint8_t *authkey)
{
    struct JOB_AES_HMAC *job;

    int i, ret;

    uint8_t key[BLOCK_SIZE];
    uint8_t buf[BLOCK_SIZE];
    DECLARE_ALIGNED(uint32_t enc_keys[15 * 4], 16);
    DECLARE_ALIGNED(uint32_t dec_keys[15 * 4], 16);
    DECLARE_ALIGNED(uint8_t ipad_hash[128], 16);
    DECLARE_ALIGNED(uint8_t opad_hash[128], 16);
    int key_len = 0;
    char recv_auth[HMAC_SHA1_96_DIGEST_SIZE];

    /* prepare the cipher key */
    switch (AES_CBC_128_KEY_SIZE)
    {
    case 16:
        IMB_AES_KEYEXP_128(mb_mgr, cipherkey,
                           enc_keys, dec_keys);
        break;
    case 24:
        IMB_AES_KEYEXP_192(mb_mgr, cipherkey,
                           enc_keys, dec_keys);
        break;
    case 32:
    default:
        IMB_AES_KEYEXP_256(mb_mgr, cipherkey,
                           enc_keys, dec_keys);
        break;
    }

    /* prepare the key */
    memset(key, 0, sizeof(key));
    if (HMAC_SHA1_96_KEY_SIZE <= BLOCK_SIZE)
    {
        memcpy(key, authkey, HMAC_SHA1_96_KEY_SIZE);
        key_len = HMAC_SHA1_96_KEY_SIZE;
    }
    else
    {
        IMB_SHA1(mb_mgr, authkey, HMAC_SHA1_96_KEY_SIZE, key);
        key_len = HMAC_SHA1_96_KEY_SIZE;
    }

    /* compute ipad hash */
    memset(buf, 0x36, sizeof(buf));
    for (i = 0; i < key_len; i++)
        buf[i] ^= key[i];
    IMB_SHA1_ONE_BLOCK(mb_mgr, buf, ipad_hash);

    /* compute opad hash */
    memset(buf, 0x5c, sizeof(buf));
    for (i = 0; i < key_len; i++)
        buf[i] ^= key[i];
    IMB_SHA1_ONE_BLOCK(mb_mgr, buf, opad_hash);

    /* empty the manager */
    while ((job = IMB_FLUSH_JOB(mb_mgr)) != NULL)
        ;

    job = IMB_GET_NEXT_JOB(mb_mgr);
    job->aes_enc_key_expanded = enc_keys;
    job->aes_dec_key_expanded = dec_keys;
    job->cipher_direction = DECRYPT;
    job->chain_order = HASH_CIPHER;
    job->dst = (uint8_t *)(packet + AES_CBC_128_DEC_START_OFFSET);
    job->aes_key_len_in_bytes = AES_CBC_128_KEY_SIZE;
    job->auth_tag_output = (uint8_t *)recv_auth;
    job->auth_tag_output_len_in_bytes = HMAC_SHA1_96_DIGEST_SIZE;
    job->iv = (uint8_t *)(packet + AES_CBC_128_IV_DEC_START_OFFSET);
    job->iv_len_in_bytes = AES_CBC_128_IV_SIZE;
    job->src = (uint8_t *)packet;
    job->cipher_start_src_offset_in_bytes = AES_CBC_128_DEC_START_OFFSET;
    job->msg_len_to_cipher_in_bytes = packet_len - AES_CBC_128_DEC_START_OFFSET - AES_CBC_128_DEC_END_OFFSET;
    job->hash_start_src_offset_in_bytes = HMAC_SHA1_96_DEC_START_OFFSET;
    job->msg_len_to_hash_in_bytes = packet_len - HMAC_SHA1_96_DEC_START_OFFSET - HMAC_SHA1_96_DEC_END_OFFSET;
    job->u.HMAC._hashed_auth_key_xor_ipad = ipad_hash;
    job->u.HMAC._hashed_auth_key_xor_opad = opad_hash;
    job->cipher_mode = CBC;
    job->hash_alg = SHA1;

    job = IMB_SUBMIT_JOB(mb_mgr);

    job = IMB_FLUSH_JOB(mb_mgr);

    if (job == NULL)
    {
        printf("Error: job cannot be returned. %d\n", __LINE__); fflush(stdout);
        return -1;
    }
    else if (job->status != STS_COMPLETED)
    {
        printf("Error: error status of job line:%d job error status:%d\n", __LINE__, job->status); fflush(stdout);
        return -1;
    }

    // authentication code check
    if (memcmp(recv_auth, (packet + packet_len - HMAC_SHA1_96_DIGEST_SIZE), HMAC_SHA1_96_DIGEST_SIZE)) 
    {
        // printf("Error: hmac mismatch!\n"); fflush(stdout);
        return -2;
    }

    return 0;
}


//

#endif
