#ifndef IP_OUT_H
#define IP_OUT_H

#include <stdint.h>
#include "tcp_stream.h"

extern inline int 
GetOutputInterface(uint32_t daddr, uint8_t *is_external);

void
ForwardIPv4Packet(mtcp_manager_t mtcp, int nif_in, char *buf, int len);

uint8_t *
IPOutputStandalone(struct mtcp_manager *mtcp, uint8_t protocol, 
		uint16_t ip_id, uint32_t saddr, uint32_t daddr, uint16_t tcplen);

uint8_t *
IPOutput(struct mtcp_manager *mtcp, tcp_stream *stream, uint16_t tcplen);

// rider added 2020-9-29 for plain packets
uint8_t *
IPOutputPlain(struct mtcp_manager *mtcp, tcp_stream *stream, uint16_t tcplen);
//

#endif /* IP_OUT_H */
