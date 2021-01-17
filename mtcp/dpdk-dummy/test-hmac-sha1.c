
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



void
hexdump(FILE *fp,
        const char *msg,
        const void *p,
        size_t len)
{
        unsigned int i, out, ofs;
        const unsigned char *data = p;

        fprintf(fp, "%s\n", msg);

        ofs = 0;
        while (ofs < len) {
                char line[120];

                out = snprintf(line, sizeof(line), "%08x:", ofs);
                for (i = 0; ((ofs + i) < len) && (i < 16); i++)
                        out += snprintf(line + out, sizeof(line) - out,
                                        " %02x", (data[ofs + i] & 0xff));
                for (; i <= 16; i++)
                        out += snprintf(line + out, sizeof(line) - out, " | ");
                for (i = 0; (ofs < len) && (i < 16); i++, ofs++) {
                        unsigned char c = data[ofs];

                        if ((c < ' ') || (c > '~'))
                                c = '.';
                        out += snprintf(line + out,
                                        sizeof(line) - out, "%c", c);
                }
                fprintf(fp, "%s\n", line);
        }
}


// algorithm parameters for hmac-sha1-98
#define HMAC_SHA1_96_BLOCK_SIZE     64
#define HMAC_SHA1_96_KEY_SIZE       20
#define HMAC_SHA1_96_DIGEST_SIZE    12
//


static int
test_hmac_sha1(struct MB_MGR *mb_mgr, char *packet, int *packet_len, uint8_t *authkey, int authkey_len)
{

    hexdump(stdout, "Raw Packet", packet, *packet_len + 10);
    
    struct JOB_AES_HMAC *job;

    int i, ret;

    uint8_t key[HMAC_SHA1_96_BLOCK_SIZE];
    uint8_t buf[HMAC_SHA1_96_BLOCK_SIZE];
    DECLARE_ALIGNED(uint8_t ipad_hash[HMAC_SHA1_96_KEY_SIZE], 16);
    DECLARE_ALIGNED(uint8_t opad_hash[HMAC_SHA1_96_KEY_SIZE], 16);
    int key_len = 0;

    /* prepare the key */
    memset(key, 0, sizeof(key));
    if (authkey_len <= HMAC_SHA1_96_BLOCK_SIZE) {
            memcpy(key, authkey, authkey_len);
            key_len = (int) authkey_len;
    } else {
            IMB_SHA1(mb_mgr, authkey, authkey_len, key);
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
    while ((job = IMB_FLUSH_JOB(mb_mgr)) != NULL);

    job = IMB_GET_NEXT_JOB(mb_mgr);
    job->aes_enc_key_expanded = NULL;
    job->aes_dec_key_expanded = NULL;
    job->cipher_direction = ENCRYPT;
    job->chain_order = HASH_CIPHER;
    job->dst = NULL;
    job->aes_key_len_in_bytes = 0;
    job->auth_tag_output = packet + *packet_len;
    job->auth_tag_output_len_in_bytes = HMAC_SHA1_96_DIGEST_SIZE;
    job->iv = NULL;
    job->iv_len_in_bytes = 0;
    job->src = packet;
    job->cipher_start_src_offset_in_bytes = 0;
    job->msg_len_to_cipher_in_bytes = 0;
    job->hash_start_src_offset_in_bytes = 0;
    job->msg_len_to_hash_in_bytes = *packet_len;
    job->u.HMAC._hashed_auth_key_xor_ipad = ipad_hash;
    job->u.HMAC._hashed_auth_key_xor_opad = opad_hash;
    job->cipher_mode = NULL_CIPHER;
    job->hash_alg = SHA1;

    job = IMB_SUBMIT_JOB(mb_mgr);
    
    job = IMB_FLUSH_JOB(mb_mgr);

    if (job->status != STS_COMPLETED) {
            printf("line:%d job error status:%d ", __LINE__, job->status);
            return 0;
    }

    *packet_len = *packet_len + HMAC_SHA1_96_DIGEST_SIZE;
    hexdump(stdout, "ESP Packet", packet, *packet_len + 10);
        
    ret = 0;

    return ret;
}

int main(int argc, char ** argv)
{

    MB_MGR *p_mgr = NULL;

    p_mgr = alloc_mb_mgr(0);

    init_mb_mgr_avx(p_mgr);

    uint8_t authkey[20] = {
       0xb5,0xb5,0xb5,0xb5,0xb5,0xb5,0xb5,0xb5,0xb5,0xb5,0xb5,0xb5,0xb5,0xb5,0xb5,0xb5,0xb5,0xb5,0xb5,0xb5
    };  
    int authkey_len = 20;

    char data[] = "\x00\x00\x00\x08\x00\x00\x00\x01\xa2\x07\xd0\x71\x5d\x94\x88\x88\xbf\x97\xd7\x45\x42\x8a\xf0\xfc\x59\x8c\xe3\x67\x67\x7f\xfb\xea\xdf\xd4\xad\x46\xb6\x80\xcb\x56\x8a\x10\x3c\x93\xda\xbb\xbb\xb1\xfe\xbd\xab\x01\xfb\x88\xb3\x1e\x39\x83\xb9\xe4\x9d\x34\x16\x16\xc0\x41\x1c\x00\xd7\x52\xa6\xc8\x84\x03\xa3\x08\x53\x04\xf9\x03\x01\xe4\x43\x50\xf0\x38\xe3\xbc";
    int data_len = 88;

    char packet[4096] = {0,};
    memcpy(packet, data, data_len);
    int packet_len = data_len;

    test_hmac_sha1(p_mgr, packet, &packet_len, authkey, authkey_len);

    free_mb_mgr(p_mgr);

   
	return 0;
}
