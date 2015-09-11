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

	std::string msg = "";
	int byte_count;
	int buf_pos = 0;
	while ((byte_count = recv(newfd, buf, BUFLEN, 0)) != 0)
	{
		std::string curr_buf(buf);
		msg += curr_buf.substr(buf_pos, byte_count - 1);
		buf_pos += byte_count;
		if (byte_count < BUFLEN)
		{
			break;
		}
	}
	const char *msg_tmp = msg.c_str();
	char *ch_msg = _strdup(msg_tmp);

	// ---------------
	printf(ch_msg);
	// ------------CLIENT SIDE------------
	// Received HTTP GET request ignored (for now)
	char magic_url[] = "http://www.ida.liu.se/~TDTS04/labs/2011/ass2/error1.html";
	//int url_len = sizeof(magic_url) / sizeof(char);
	std::string new_site(magic_url);
	// ---IP Look-up---
	char *server_hostname = extract_host_name(new_site);

	struct in_addr server_addr;
	struct addrinfo server_hints, *server_res, *p;

	memset(&server_hints, 0, sizeof(server_hints));
	server_hints.ai_socktype = SOCK_STREAM;
	server_hints.ai_family = AF_INET;

	if ((err = getaddrinfo(server_hostname, "80", &server_hints, &server_res)) != 0)
	{
		printf("Error during server getaddrinfo() %d\n", err);
		return 1;
	}

	server_addr.S_un = ((struct sockaddr_in *)(server_res->ai_addr))->sin_addr.S_un;

	printf("Server IP address : %s\n", inet_ntoa(server_addr));
	// ----------------

	// Make a socket using the information received from getaddrinfo
	int server_sockfd;
	server_sockfd = socket(server_res->ai_family, server_res->ai_socktype, server_res->ai_protocol);

	// Loop through all the results and connect to the first available
	for (p = server_res; p != NULL; p = p->ai_next) {
		if ((server_sockfd = socket(p->ai_family, p->ai_socktype,
			p->ai_protocol)) == -1) {
			perror("Error during client socket assignment");
			continue;
		}

		if (connect(server_sockfd, p->ai_addr, p->ai_addrlen) == -1) {
			closesocket(server_sockfd);
			perror("Error during client connection to server");
			continue;
		}

		break;
	}

	if (p == NULL) {
		fprintf(stderr, "Client: failed to connect to web server\n");
		return 2;
	}

	// ---Sending---
	// INNAN DETTA: url-filtrering (+ connection close)
	int len = strlen(ch_msg);
	int bytes_sent = send(server_sockfd, ch_msg, len, 0);
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