
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <intel-ipsec-mb.h>

// static const uint8_t key8[key_len8] = {
//        0xb5,0xb5,0xb5,0xb5,0xb5,0xb5,0xb5,0xb5,0xb5,0xb5,0xb5,0xb5,0xb5,0xb5,0xb5,0xb5,0xb5,0xb5,0xb5,0xb5
// };
// static const char data8[] = "\x00\x00\x00\x08\x00\x00\x00\x01\xa2\x07\xd0\x71\x5d\x94\x88\x88\xbf\x97\xd7\x45\x42\x8a\xf0\xfc\x59\x8c\xe3\x67\x67\x7f\xfb\xea\xdf\xd4\xad\x46\xb6\x80\xcb\x56\x8a\x10\x3c\x93\xda\xbb\xbb\xb1\xfe\xbd\xab\x01\xfb\x88\xb3\x1e\x39\x83\xb9\xe4\x9d\x34\x16\x16\xc0\x41\x1c\x00\xd7\x52\xa6\xc8\x84\x03\xa3\x08\x53\x04\xf9\x03\x01\xe4\x43\x50\xf0\x38\xe3\xbc";
// static const uint8_t digest8[digest_len8] = {
//         0x93,0x34,0x09,0x4d,0xf1,0x57,0xd2,0xcc,0xd2,0xe4,0xd8,0x0a
// };

void hexdump(FILE *fp,
             const char *msg,
             const void *p,
             size_t len)
{
    unsigned int i, out, ofs;
    const unsigned char *data = p;

    fprintf(fp, "%s\n", msg);

    ofs = 0;
    while (ofs < len)
    {
        char line[120];

        out = snprintf(line, sizeof(line), "%08x:", ofs);
        for (i = 0; ((ofs + i) < len) && (i < 16); i++)
            out += snprintf(line + out, sizeof(line) - out,
                            " %02x", (data[ofs + i] & 0xff));
        for (; i <= 16; i++)
            out += snprintf(line + out, sizeof(line) - out, " | ");
        for (i = 0; (ofs < len) && (i < 16); i++, ofs++)
        {
            unsigned char c = data[ofs];

            if ((c < ' ') || (c > '~'))
                c = '.';
            out += snprintf(line + out,
                            sizeof(line) - out, "%c", c);
        }
        fprintf(fp, "%s\n", line);
    }
}

#define ETHERNET_HEADER_SIZE            (14)
#define IP_HEADER_SIZE                  (20)
#define ESP_HEADER_SIZE                 (24)


// algorithm parameters for hmac-sha1-98 aes-cbc-128
#define BLOCK_SIZE                      (64)
#define HMAC_SHA1_96_KEY_SIZE           (20)
#define HMAC_SHA1_96_DIGEST_SIZE        (12)
#define HMAC_SHA1_96_ENC_START_OFFSET   (IP_HEADER_SIZE) // ignore IP header
#define HMAC_SHA1_96_ENC_END_OFFSET		(HMAC_SHA1_96_DIGEST_SIZE) // ignore authentication data
#define HMAC_SHA1_96_DEC_START_OFFSET   (IP_HEADER_SIZE + ETHERNET_HEADER_SIZE) // ignore IP and Ethernet header
#define HMAC_SHA1_96_DEC_END_OFFSET     (HMAC_SHA1_96_DIGEST_SIZE) // ignore authentication data
#define AES_CBC_128_KEY_SIZE            (16)
#define AES_CBC_128_IV_SIZE             (16)
#define AES_CBC_128_IV_ENC_START_OFFSET		(8 + IP_HEADER_SIZE) // position of initial vector in ESP header
#define AES_CBC_128_IV_DEC_START_OFFSET		(8 + IP_HEADER_SIZE + ETHERNET_HEADER_SIZE) // position of initial vector in ESP header
#define AES_CBC_128_ENC_START_OFFSET    (ESP_HEADER_SIZE + IP_HEADER_SIZE) // ignore ESP and IP header
#define AES_CBC_128_ENC_END_OFFSET		(HMAC_SHA1_96_DIGEST_SIZE) // ignore authentication data
#define AES_CBC_128_DEC_START_OFFSET    (ESP_HEADER_SIZE + IP_HEADER_SIZE + ETHERNET_HEADER_SIZE) // ignore ESP, IP and Ethernet header
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

    if (job->status == NULL)
    {
        printf("Error: job cannot be returned. %d\n", __LINE__);
        fflush(stdout);
        return -1;
    }
    else if (job->status != STS_COMPLETED)
    {
        printf("Error: error status of job line:%d job error status:%d\n", __LINE__, job->status);
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

    if (job->status == NULL)
    {
        printf("Error: job cannot be returned. %d\n", __LINE__);
        fflush(stdout);
        return -1;
    }
    else if (job->status != STS_COMPLETED)
    {
        printf("Error: error status of job line:%d job error status:%d\n", __LINE__, job->status);
        return -1;
    }

    // authentication code check
    if (memcmp(recv_auth, (packet + packet_len - HMAC_SHA1_96_DIGEST_SIZE), HMAC_SHA1_96_DIGEST_SIZE)) 
    {
        printf("Error: hmac mismatch!\n");
        return -2;
    }

    return 0;
}

/*
int main(int argc, char **argv)
{

    MB_MGR *p_mgr = NULL;

    p_mgr = alloc_mb_mgr(0);

    init_mb_mgr_avx(p_mgr);

    uint8_t authkey[20] = {0xb5, 0xb5, 0xb5, 0xb5, 0xb5, 0xb5, 0xb5, 0xb5, 0xb5, 0xb5, 0xb5, 0xb5, 0xb5, 0xb5, 0xb5, 0xb5, 0xb5, 0xb5, 0xb5, 0xb5};
    uint8_t cipherkey[] = {0x12, 0x12, 0x12, 0x12, 0x12, 0x12, 0x12, 0x12, 0x12, 0x12, 0x12, 0x12, 0x12, 0x12, 0x12, 0x12};
    uint8_t initialvector[] = {0xa2, 0x07, 0xd0, 0x71, 0x5d, 0x94, 0x88, 0x88, 0xbf, 0x97, 0xd7, 0x45, 0x42, 0x8a, 0xf0, 0xfc};

    // char data[] = "\x00\x00\x00\x08\x00\x00\x00\x01\xa2\x07\xd0\x71\x5d\x94\x88\x88\xbf\x97\xd7\x45\x42\x8a\xf0\xfc\x59\x8c\xe3\x67\x67\x7f\xfb\xea\xdf\xd4\xad\x46\xb6\x80\xcb\x56\x8a\x10\x3c\x93\xda\xbb\xbb\xb1\xfe\xbd\xab\x01\xfb\x88\xb3\x1e\x39\x83\xb9\xe4\x9d\x34\x16\x16\xc0\x41\x1c\x00\xd7\x52\xa6\xc8\x84\x03\xa3\x08\x53\x04\xf9\x03\x01\xe4\x43\x50\xf0\x38\xe3\xbc";
    char data[] = "\x45\x00\x00\x78\x00\x00\x40\x00\x40\x32\xd4\x44\xa9\xfe\x09\x09\xa9\xfe\x09\x0a\x00\x00\x00\x08\x00\x00\x00\x01\xa2\x07\xd0\x71\x5d\x94\x88\x88\xbf\x97\xd7\x45\x42\x8a\xf0\xfc\x45\x00\x00\x3c\x6d\x60\x40\x00\x40\x06\x67\x4c\xa9\xfe\x09\x09\xa9\xfe\x09\x0a\x96\xc0\x13\x88\x08\x54\x34\xf0\x00\x00\x00\x00\xa0\x02\xfa\xf0\x1c\xd7\x00\x00\x02\x04\x05\xb4\x04\x02\x08\x0a\x44\xd6\x9d\xc5\x00\x00\x00\x00\x01\x03\x03\x07\x01\x02\x02\x04\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00";
    int data_len = 120;

    char packet[4096] = {
        0,
    };
    memcpy(packet, data, data_len);
    int packet_len = data_len;

    hexdump(stdout, "Raw Packet", packet, packet_len);
    esp_encrypt_aes_cbc_128_hmac_sha1_96(p_mgr, packet, packet_len, cipherkey, authkey);
    hexdump(stdout, "ESP Packet", packet, packet_len);

    free_mb_mgr(p_mgr);

    return 0;
}
*/


int main(int argc, char **argv)
{

    MB_MGR *p_mgr = NULL;

    p_mgr = alloc_mb_mgr(0);

    init_mb_mgr_avx(p_mgr);

    uint8_t authkey[20] = {0xb5, 0xb5, 0xb5, 0xb5, 0xb5, 0xb5, 0xb5, 0xb5, 0xb5, 0xb5, 0xb5, 0xb5, 0xb5, 0xb5, 0xb5, 0xb5, 0xb5, 0xb5, 0xb5, 0xb5};
    uint8_t cipherkey[] = {0x12, 0x12, 0x12, 0x12, 0x12, 0x12, 0x12, 0x12, 0x12, 0x12, 0x12, 0x12, 0x12, 0x12, 0x12, 0x12};
    uint8_t initialvector[] = {0xa2, 0x07, 0xd0, 0x71, 0x5d, 0x94, 0x88, 0x88, 0xbf, 0x97, 0xd7, 0x45, 0x42, 0x8a, 0xf0, 0xfc};

    char data[] = "\x54\xb2\x03\x8c\xee\xb0\x54\xb2\x03\x8c\xf3\x64\x08\x00\x45\x00" \
                "\x00\x78\x00\x00\x40\x00\x40\x32\xd4\x44\xa9\xfe\x09\x09\xa9\xfe" \
                "\x09\x0a\x00\x00\x00\x08\x00\x00\x00\x01\x66\xde\x52\x18\x24\x42" \
                "\x57\x93\x62\x7c\xa1\xe9\x5e\x53\x72\x94\xbf\xf7\xda\xba\x4d\x75" \
                "\x3c\xa3\xb4\x36\x4a\x3b\xe6\xde\x5c\x0c\x9f\x47\x7b\xf3\x1e\x4a" \
                "\x1b\x30\x4c\xb5\x82\x11\x98\x07\xf6\x2a\x43\x97\x3b\xdf\xa2\xa5" \
                "\x9d\xbc\xfd\x13\x17\xdf\xbf\x24\x55\x45\xdb\xe3\x42\xa2\x0e\xa7" \
                "\x8c\xac\x11\x5a\xf6\xb3\xd0\x73\x2c\xd5\x8c\x9a\x7c\xb6\x26\xfe" \
                "\x39\xf2\x3c\xc4\x7e\xc3";
    int data_len = 134;

    char packet[4096] = {
        0,
    };
    memcpy(packet, data, data_len);
    int packet_len = data_len;

    hexdump(stdout, "Raw Packet", packet, packet_len);
    esp_decrypt_aes_cbc_128_hmac_sha1_96(p_mgr, packet, packet_len, cipherkey, authkey);
    hexdump(stdout, "ESP Packet", packet, packet_len);

    free_mb_mgr(p_mgr);

    return 0;
}
