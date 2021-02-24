#include "../../NFV_LTE_EPC-1.0/src/udp_client.h"

UdpClient::UdpClient() {
	conn_fd = socket(AF_INET, SOCK_STREAM, 0);
	g_utils.handle_type1_error(conn_fd, "Socket error: udpclient_udpclient");
	port = g_freeport;
}

void UdpClient::conn(string arg_ip_addr, string arg_server_ip_addr, int arg_server_port) {
	init(arg_ip_addr, arg_server_ip_addr, arg_server_port);
	g_nw.bind_sock(conn_fd, sock_addr);
	set_port();
	g_nw.set_rcv_timeout(conn_fd, 1);
}

void UdpClient::set_client(string arg_ip_addr) {
	ip_addr = arg_ip_addr;
	g_nw.set_inet_sock_addr(ip_addr, port, sock_addr);
	g_nw.bind_sock(conn_fd, sock_addr);
	set_port();	
	g_nw.set_rcv_timeout(conn_fd, 1);
}

void UdpClient::set_server(string arg_server_ip_addr, int arg_server_port) {
	server_port = arg_server_port;
	server_ip_addr = arg_server_ip_addr;
	g_nw.set_inet_sock_addr(server_ip_addr, server_port, server_sock_addr);
}

void UdpClient::init(string arg_ip_addr, string arg_server_ip_addr, int arg_server_port) {
	int status;

	ip_addr = arg_ip_addr;
	g_nw.set_inet_sock_addr(ip_addr, port, sock_addr);
	server_port = arg_server_port;
	server_ip_addr = arg_server_ip_addr;
	g_nw.set_inet_sock_addr(server_ip_addr, server_port, server_sock_addr);
}

void UdpClient::set_port() {
	g_nw.get_sock_addr(conn_fd, sock_addr);
	port = ntohs(sock_addr.sin_port);
}

void UdpClient::snd(Packet pkt) {
	int status;
	
	pkt.prepend_len();
	while (1) {
		if (connected == -1) {
			if (connect(conn_fd, (struct sockaddr *) &server_sock_addr, sizeof(server_sock_addr)) < 0) {
				TRACE(cout << "Could not connect. Error code: " << errno << endl;)
				break;
			}
			connected = 1;
		}
		
		status = write(conn_fd, pkt.data, pkt.len);
		if (errno == EPERM) {
			errno = 0;
			usleep(1000);
			continue;
		}
		else {
			break;
		}
	}
	g_utils.handle_type2_error(status, "Sendto error: udpclient_snd");
}

void UdpClient::rcv(Packet &pkt) {

	printf("RIDER REPORT: UdpClient::rcv function started\n"); fflush(stdout);
	int status;

	pkt.clear_pkt();

	char packet[4096];
	int packet_length;

	status = read(conn_fd, packet, BUF_SIZE); 

	memmove(&packet_length, packet, sizeof(int)); // Move packet length into packet_len
	memcpy(pkt.data, (packet+sizeof(int)), packet_length);

	g_utils.handle_type2_error(status, "Recvfrom error: udpclient_rcv");
	pkt.data_ptr = 0;
	pkt.len = packet_length;
	printf("RIDER REPORT: UdpClient::rcv function finished\n"); fflush(stdout);
}

UdpClient::~UdpClient() {
	close(conn_fd);
}