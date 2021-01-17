#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <iostream>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <pthread.h>
#include <time.h>
#include <stdlib.h>
#include <csignal>
#include "common.h"
#include <mutex>
#include <vector>

#include <sys/ioctl.h>
#include <net/if.h>

using namespace std;

int main(int argc, char *argv[])
{
	char * destIp = argv[1];
    int destPort = atoi(argv[2]);

	int socketfd, n;
	struct sockaddr_in rcvr_addr;
	char buf[100];
	strcpy(buf, "hello ipsec");

	rcvr_addr.sin_family = AF_INET;
	rcvr_addr.sin_addr.s_addr = inet_addr(destIp);
	rcvr_addr.sin_port = htons(destPort);

	socketfd = socket(AF_INET, SOCK_STREAM, 0);
	if (socketfd < 0)
	{
		cout << "Error: Opening Socket" << '\n';
		exit(-1);
	}

	// struct ifreq ifr;
	// memset(&ifr, 0, sizeof(ifr));
	// snprintf(ifr.ifr_name, sizeof(ifr.ifr_name), "enp3s0");
	// setsockopt(socketfd, SOL_SOCKET, SO_BINDTODEVICE, (void *)&ifr, sizeof(ifr));
	struct timeval tv1;
	tv1.tv_sec = 2; //timeout in seconds
	tv1.tv_usec = 0;
	setsockopt(socketfd, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv1, sizeof tv1);

	cout << "connecting" << "\n";

	if (connect(socketfd, (struct sockaddr *)&rcvr_addr, sizeof(rcvr_addr)) < 0)
	{
		cout << "Error : connecting" << errno << '\n';
		close(socketfd);
		exit(-1);
	}

	cout << "writing data" << endl;

	cout << "buf: " << buf << endl;

	n = write(socketfd, buf, strlen(buf));
	if (n <= 0)
	{
		cout << "Error : Wirite Error" << errno << '\n';
		close(socketfd);
	}

	cout << "reading data" << "\n";

	n = read(socketfd, buf, 100);

	cout << "buf: " << buf << endl;

	if (n <= 0)
	{
		cout << "Error: Read Error" << errno << '\n';
	}

	cout << "closing data" << "\n";

	close(socketfd);

	cout << "T: Exiting" << '\n';
}
