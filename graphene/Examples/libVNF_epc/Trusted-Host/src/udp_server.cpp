#include "../../NFV_LTE_EPC-1.0/src/udp_server.h"


int sockaddr_cmp(struct sockaddr *x, struct sockaddr *y)
{
#define CMP(a, b) if (a != b) return a < b ? -1 : 1
	
    CMP(x->sa_family, y->sa_family);

    if (x->sa_family == AF_INET) {
        struct sockaddr_in *xin = (struct sockaddr_in*)x;
		struct sockaddr_in *yin = (struct sockaddr_in*)y;
        CMP(ntohl(xin->sin_addr.s_addr), ntohl(yin->sin_addr.s_addr));
        CMP(ntohs(xin->sin_port), ntohs(yin->sin_port));
    } else if (x->sa_family == AF_INET6) {
        struct sockaddr_in6 *xin6 = (struct sockaddr_in6*)x;
		struct sockaddr_in6 *yin6 = (struct sockaddr_in6*)y;
        int r = memcmp(xin6->sin6_addr.s6_addr, yin6->sin6_addr.s6_addr, sizeof(xin6->sin6_addr.s6_addr));
        if (r != 0)
            return r;
        CMP(ntohs(xin6->sin6_port), ntohs(yin6->sin6_port));
        CMP(xin6->sin6_flowinfo, yin6->sin6_flowinfo);
        CMP(xin6->sin6_scope_id, yin6->sin6_scope_id);
    } else {
        printf("Error: unknown sa_family\n");
		exit(-1);
    }

#undef CMP
    return 0;
}


UdpServer::UdpServer() {
	conn_fd = socket(AF_INET, SOCK_STREAM, 0);
	g_utils.handle_type1_error(conn_fd, "Socket error: udpserver_udpserver");		
}

void UdpServer::run(string arg_ip_addr, int arg_port) {
	init(arg_ip_addr, arg_port);
	g_nw.set_sock_reuse(conn_fd);
	g_nw.bind_sock(conn_fd, sock_addr);

	if ((listen(conn_fd, 5)) != 0) { 
		TRACE(cout << "Listen failed..." << endl;)
		exit(0); 
	} 
	else
		TRACE(cout << "Server listening.." << endl;)
}

void UdpServer::init(string arg_ip_addr, int arg_port) {
	int status;

	port = arg_port;
	ip_addr = arg_ip_addr;
	g_nw.set_inet_sock_addr(ip_addr, port, sock_addr);
}

void UdpServer::snd(struct sockaddr_in dst_sock_addr, Packet pkt){

	printf("RIDER REPORT: UdpServer::snd function, pkt.len=%d, %x,%x,%x,%x\n", pkt.len, pkt.data[0], pkt.data[1], pkt.data[2], pkt.data[3]); fflush(stdout);
	int cli_fd = 0;
	int i;
	for (i = 0; i < cli_conn_count; i++) {
		if (sockaddr_cmp((struct sockaddr *)&dst_sock_addr, (struct sockaddr *)&cli_sock_addr[i]) == 0) {
			cli_fd = cli_conn_fd[i];
			break;
		}
	}

	if (cli_fd == 0) {
		printf("Error1: Connection doesn't exist. \n");
	}

	int status;

	pkt.prepend_len();
	printf("RIDER REPORT: UdpServer::snd function cli_fd=%d, pkt.len=%d, %x,%x,%x,%x\n", cli_fd, pkt.len, pkt.data[0], pkt.data[1], pkt.data[2], pkt.data[3]); fflush(stdout);
	while (1) {
		status = write(cli_fd, pkt.data, pkt.len);
		if (errno == EPERM) {
			TRACE(cout << "Too many writes to a socket - error: udpserver_snd" << endl;)
			errno = 0;
			usleep(1000);
			continue;
		}
		else {
			break;
		}
	}
	g_utils.handle_type1_error(status, "Sendto error: udpserver_snd");
}

void UdpServer::rcv(struct sockaddr_in &src_sock_addr, Packet &pkt) {

	printf("RIDER REPORT: UdpServer::rcv function started\n"); fflush(stdout);

	int cli_fd = 0;
	int i; 

	fd_set readfds;
	int max_sd;
	FD_ZERO(&readfds); 
	FD_SET(conn_fd, &readfds);
	max_sd = conn_fd;
	for (i = 0; i < cli_conn_count; i++) {
		FD_SET(cli_conn_fd[i], &readfds);
		if (max_sd < cli_conn_fd[i]) max_sd = cli_conn_fd[i];
	}
	// wait for something happens
	int activity = select(max_sd+1, &readfds, NULL, NULL, NULL);
	if ((activity < 0) && (errno!=EINTR)) {  
		printf("Error: select error\n");   
	}   
	// if new connection
	if (FD_ISSET(conn_fd, &readfds))  {
		struct sockaddr_in cli; 
		socklen_t len = (socklen_t)sizeof(cli); 	
		// Accept the data packet from client and verification 
		cli_fd = accept(conn_fd, (struct sockaddr*)&cli, &len);
		printf("RIDER REPORT: UdpServer::rcv function new connection ! cli_fd=%d, cli_conn_count=%d, errno=%d\n", cli_fd, cli_conn_count, errno); fflush(stdout); 
		src_sock_addr = cli;
		
		// store new connection
		for (i = 0; i < cli_conn_count; i++) {
			if (sockaddr_cmp((struct sockaddr *)&cli, (struct sockaddr *)&cli_sock_addr[i]) == 0) {
				cli_conn_fd[i] = cli_fd;
				break;
			}
		}
		if (i == cli_conn_count) {
			cli_sock_addr[i] = cli;
			cli_conn_fd[i] = cli_fd;
			cli_conn_count++;
		}
	}
	else {
		for (i = 0; i < cli_conn_count; i++) {
			if (FD_ISSET(cli_conn_fd[i], &readfds)) break;
		}
		if (i == cli_conn_count) {
			printf("Error: cannot read data.\n");
		}
		else {
			cli_fd = cli_conn_fd[i];
			src_sock_addr = cli_sock_addr[i];
		}
	}

	int status;
	
	pkt.clear_pkt();

	char packet[4096];
	int packet_length;
	
	printf("RIDER REPORT: UdpServer::rcv function reading... cli_fd=%d\n", cli_fd); fflush(stdout); 
	status = read(cli_fd, packet, BUF_SIZE); 

	memmove(&packet_length, packet, sizeof(int)); // Move packet length into packet_len
	memcpy(pkt.data, (packet+sizeof(int)), packet_length);

	g_utils.handle_type1_error(status, "Recvfrom error: udpserver_rcv");
	pkt.data_ptr = 0;
	pkt.len = packet_length;
	printf("RIDER REPORT: UdpServer::rcv function finished, cli_fd=%d, cli_conn_count=%d, pkt.len=%d, %x,%x,%x,%x\n", cli_fd, cli_conn_count, pkt.len, pkt.data[0], pkt.data[1], pkt.data[2], pkt.data[3]); fflush(stdout);
}

UdpServer::~UdpServer() {
	close(conn_fd);
}
