#include <stdio.h>
#include <iostream>
#include <sys/types.h>
#include <winsock2.h>
#include <WS2tcpip.h>
#include <string>

#define BUFLEN 512

char *extract_host_name(std::string full_url);

int main()
{
	// ---Setup---
	WSADATA data;
	WSAStartup(MAKEWORD(2, 0), &data);

	char *hostname = "localhost";
	char *port = "3334"; // Port in broswer to listen to.
	struct in_addr addr;
	struct addrinfo hints, *res;
	int err;

	memset(&hints, 0, sizeof(hints));
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_family = AF_INET;

	if ((err = getaddrinfo(hostname, port, &hints, &res)) != 0)
	{
		printf("Error during getaddrinfo() %d\n", err);
		return 1;
	}

	addr.S_un = ((struct sockaddr_in *)(res->ai_addr))->sin_addr.S_un;

	printf("Local IP address : %s\n", inet_ntoa(addr));

	// Make a socket using the information received from getaddrinfo
	int sockfd;
	sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);

	// Bind socket to port
	if ((err = bind(sockfd, res->ai_addr, res->ai_addrlen)) != 0)
	{
		printf("Error during bind() %d\n", err);
		return 1;
	}
	// -----------

	// ---Listening, accepting---
	int backlog = 20;
	if ((err = listen(sockfd, backlog)) != 0)
	{
		printf("Error during listen() %d\n", err);
		return 1;
	}

	struct sockaddr_storage their_addr;
	socklen_t addr_size;
	int newfd;
	addr_size = sizeof(their_addr);
	newfd = accept(sockfd, (struct sockaddr *)&their_addr, &addr_size);
	// --------------------------

	// ---Receiving---
	char buf[BUFLEN];
	int byte_count;
	byte_count = recv(newfd, buf, BUFLEN, 0);
	// ---------------

	// ------------CLIENT SIDE------------
	// Received HTTP GET request ignored (for now)
	char magic_url[] = "http://www.ida.liu.se/~TDTS04/labs/2011/ass2/error1.html";
	//int url_len = sizeof(magic_url) / sizeof(char);
	std::string new_site(magic_url);
	// ---IP Look-up---
	char *server_hostname = extract_host_name(new_site);

	struct in_addr server_addr;

	if ((err = getaddrinfo(hostname, port, &hints, &res)) != 0)
	{
		printf("error %d\n", err);
		return 1;
	}

	addr.S_un = ((struct sockaddr_in *)(res->ai_addr))->sin_addr.S_un;

	printf("ip address : %s\n", inet_ntoa(addr));

	// If we want to clear:
	//freeaddrinfo(res);
	// ----------------

	// ---Sending---
	// -------------

	// -----------------------------------


	WSACleanup();
	return 0;
}

char *extract_host_name(std::string full_url)
{
	int start_pos = 0;
	int end_pos = 0;
	char curr_char;
	char next_char;
	int url_len = full_url.length();
	for (int n = 0; n < url_len; n++)
	{
		curr_char = full_url[n];
		next_char = full_url[n + 1];
		if (curr_char == '/' && next_char == '/')
		{
			start_pos = n + 2;
		}
		else if (next_char == '/' && start_pos != 0)
		{
			end_pos = n;
			break;
		}
	}
	std::string host = full_url.substr(start_pos, end_pos - start_pos + 1);
	const char *hostname = host.c_str();

	return _strdup(hostname);
}