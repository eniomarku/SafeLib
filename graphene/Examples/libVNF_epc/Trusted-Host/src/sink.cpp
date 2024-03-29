#include "../../NFV_LTE_EPC-1.0/src/sink.h"

string g_pgw_sgi_ip_addr = "169.254.9.9"; // type 1
string g_sink_ip_addr = "169.254.9.9"; // type 1
int g_pgw_sgi_port = 8100;
int g_sink_port = 8500;

void TrafficMonitor::handle_uplink_udata() {
	struct sockaddr_in src_sock_addr;
	Packet pkt;

	// TRACE(cout << "sink_handleuplinkudata:" << " Data pending from pgw" << endl;)
	server.rcv(src_sock_addr, pkt);
	tun.snd(pkt);
}

void TrafficMonitor::handle_downlink_udata() {
	UdpClient pgw_sgi_client;
	Packet pkt;

	// TRACE(cout << "sink_handledownlinkudata:" << " Data pending at tun" << endl;)
	tun.rcv(pkt);
	pgw_sgi_client.conn(g_sink_ip_addr, g_pgw_sgi_ip_addr, g_pgw_sgi_port);
	pgw_sgi_client.snd(pkt);
}
