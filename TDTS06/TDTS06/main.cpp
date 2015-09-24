#include <stdio.h>
#include <iostream>
#include <sys/types.h>
#include <winsock2.h>
#include <WS2tcpip.h>
#include <string>
#include <algorithm>

#define BUFLEN 60000

// --------------------------Custom functions--------------------------
std::string extract_url(std::string msg);
char *extract_host_name(std::string full_url);
std::string extract_header(std::string msg);
int set_connection_type(std::string *msg, std::string type);
bool replace_str(std::string& str, const std::string& from, const std::string& to);
bool contains(std::string str_buf, std::string search_str);
// --------------------------------------------------------------------

int main()
{
	// -------------------------------Setup-------------------------------
	WSADATA data;
	WSAStartup(MAKEWORD(2, 0), &data);

	char *hostname = "localhost";
	char *port = "3334"; // Port in browser to listen to.

	// Bad words to look out for.
	std::string badword1 = "spongebob";
	std::string badword2 = "britney spears";
	std::string badword3 = "paris hilton";
	std::string badword4 = "norrköping";

	struct in_addr addr;
	struct addrinfo hints, *res;
	int err;

	memset(&hints, 0, sizeof(hints));
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_family = AF_INET;

	// ---Get info about the browser---
	if ((err = getaddrinfo(hostname, port, &hints, &res)) != 0)
	{
		printf("Error during getaddrinfo() %d\n", err);
		return 1;
	}

	addr.S_un = ((struct sockaddr_in *)(res->ai_addr))->sin_addr.S_un;
	printf("######################### Local IP address: %s ##########################\n", inet_ntoa(addr));

	// Make a socket using the information received from getaddrinfo
	int sockfd;
	sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);

	// Bind socket to port
	if ((err = bind(sockfd, res->ai_addr, res->ai_addrlen)) != 0)
	{
		printf("Error during bind() %d\n", err);
		return 1;
	}
	// -------------------------------------------------------------------

	while (true)
	{
		printf("\n################################# Proxy ready! #################################\n");

		// Listening
		int backlog = 20;
		if ((err = listen(sockfd, backlog)) != 0)
		{
			printf("Error during listen() %d\n", err);
			return 1;
		}

		// Accepting
		struct sockaddr_storage their_addr;
		socklen_t addr_size;
		int newfd;
		addr_size = sizeof(their_addr);
		newfd = accept(sockfd, (struct sockaddr *)&their_addr, &addr_size);
		
		// Receiving
		char buf[BUFLEN];
		std::string msg = "";
		int byte_count;
		int buf_pos = 0;
		while ((byte_count = recv(newfd, buf, BUFLEN, 0)) != 0)
		{
			std::string curr_buf(buf);
			msg += curr_buf.substr(0, byte_count);
			buf_pos += byte_count;
			if (byte_count < BUFLEN)
			{
				break;
			}
		}

		// Sometimes the proxy's browser listen triggers and accepts,
		// but 5~ seconds into recv 0 bytes are received. This causes a
		// soft lock, the code below circumvent that lock. Why this
		// happens is unknown, which is why it's referred to as strange.
		bool strange_error = false;
		if (msg == "")
		{
			strange_error = true;
			std::cout << "\n########## Something strange happened, back to listening to browser! ###########\n";
		}

		if (!strange_error)
		{
			if (contains(msg, "connection: keep-alive"))
			{
				//printf("\n################### HTTP request has connection: keep-alive. ###################\n");
			}
			else if (contains(msg, "connection: close"))
			{
				//printf("\n################### HTTP request has connection: close. ###################\n");
			}
			else
			{
				printf("\n############## Warning! HTTP request does not have a connection type. ##############\n");
			}

			set_connection_type(&msg, "close");
			// ---Filtering---

			// ---------------
			const char *msg_tmp = msg.c_str();
			char *ch_msg = _strdup(msg_tmp);
			printf("\n############ Intercepted HTTP request with connection set to close: ############\n%s", ch_msg);
			std::cout << "\n" << "Header length:" << msg.length() << "\n";
			// ---------------
			// ------------CLIENT SIDE------------
			// Received HTTP GET request ignored (for now)
			///std::string testtest = extract_url(msg);
			///char magic_url[] = "http://www.ida.liu.se/~TDTS04/labs/2011/ass2/error1.html";
			//int url_len = sizeof(magic_url) / sizeof(char);
			//std::string new_site(magic_url);
			// ---IP Look-up---
			char *server_hostname = extract_host_name(extract_url(msg));

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

			printf("\n###################### Server IP address: %s ######################\n", inet_ntoa(server_addr));
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
			std::cout << "\n" << "Bytes sent: " << bytes_sent << "\n";
			// -------------
			// --------------------------
			// ---Receiving---
			char server_buf[BUFLEN];

			std::string text_msg = "";
			int amount_of_msgs = 0;
			int server_byte_count;
			int server_buf_pos = 0;
			bool plaintext = false;
			bool checked_connection_response = false;
			//printf("\n############## HTTP response, with connection set to keep-alive: ###############\n");
			bool found_header = false;
			while ((server_byte_count = recv(server_sockfd, server_buf, BUFLEN, 0)) != 0)
			{
				std::string curr_buf(server_buf);
				curr_buf = curr_buf.substr(0, server_byte_count);
				text_msg += curr_buf;
				amount_of_msgs++;
				//--------------------------------------------------------------------------------!!!!!!!
				if (contains(curr_buf, "content-type: text/")) // !!!!!!!!
				{
					plaintext = true;
				}
				server_buf_pos += server_byte_count;
				if (!checked_connection_response)
				{
					if (contains(msg, "connection: keep-alive"))
					{
						//printf("\n################### HTTP response has connection: keep-alive. ##################\n");
						checked_connection_response = true;
					}
					else if (contains(msg, "connection: close"))
					{
						//printf("\n#################### HTTP response has connection: close. ####################\n");
						checked_connection_response = true;
					}
				}
				//set_connection_type(&server_msg, "keep-alive");
				std::string header = extract_header(curr_buf);
				if (header != "error" && !found_header)
				{
					std::cout << header << "\n" << "Header size:" << header.length();
					found_header = true;
				}
				std::cout << "\n" << "Received: " << server_byte_count;
				if (!plaintext)
				{
					int server_len = server_byte_count; // Change this after filtering!
					char *curr_buf_ch = server_buf; // Change this after filtering!
					int server_bytes_sent = send(newfd, curr_buf_ch, server_len, 0);
					std::cout << "\nBytes sent to browser (not text): " << server_bytes_sent << "\n";
				}
			}
			// -----------------------------------

			if (plaintext)
			{
				// Filtering
				// ---------
				const char *server_msg_tmp = text_msg.c_str();
				char *server_ch_msg = _strdup(server_msg_tmp);
				int server_len = strlen(server_ch_msg);
				int server_bytes_sent = send(newfd, server_ch_msg, server_len, 0);
				std::cout << "\nBytes sent to browser (text): " << server_bytes_sent << "\n";
				std::cout << "\nMessage was divided into " << amount_of_msgs << " chunks.\n";
			}

			// ---Clean up---
			freeaddrinfo(server_res);
			closesocket(server_sockfd);
		}
		
		// ---Clean up---
		closesocket(newfd);
	}

	// Final clean up
	freeaddrinfo(res);
	closesocket(sockfd);
	WSACleanup();

	return 0;
}

// --------------------------Custom functions--------------------------
std::string extract_url(std::string msg)
{
	// Extracts a URL from an HTTP header.
	std::string start_str = "http";
	char end_ch = ' ';
	int msg_len = msg.length();

	size_t start_pos = msg.find(start_str);
	if (start_pos == std::string::npos)
	{
		return "error";
	}
	int end_pos;
	bool found = false;
	for (int n = start_pos; n < msg_len; n++)
	{
		if (msg.at(n) == end_ch)
		{
			end_pos = n;
			found = true;
			break;
		}
	}
	std::string out_str;

	if (found)
	{
		out_str = msg.substr(start_pos, end_pos - start_pos);
		return out_str;
	}
	else
	{
		return "error";
	}
}

char *extract_host_name(std::string full_url)
{
	// Extracts and returns a hostname given a URL.
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

std::string extract_header(std::string msg)
{
	// Returns HTTP header from a message (packet).
	size_t header_end = msg.find("\r\n\r\n");
	if (header_end == std::string::npos)
	{
		return "error";
	}
	else
	{
		return msg.substr(0, header_end + 4);
	}
}

int set_connection_type(std::string *msg, std::string type)
{
	// Sets connection type between keep-alive and close. Returns 1 on error.
	std::string tmp_msg = *msg;
	if (type == "close")
	{
		if (replace_str(tmp_msg, "Connection: keep-alive", "Connection: close"))
		{
			*msg = tmp_msg;
			return 0;
		}
		else
		{
			return 1;
		}
	}
	else if (type == "keep-alive")
	{
		if (replace_str(tmp_msg, "Connection: close", "Connection: keep-alive"))
		{
			*msg = tmp_msg;
			return 0;
		}
		else
		{
			return 1;
		}
	}
	else
	{
		return 1;
	}
}

bool replace_str(std::string& str, const std::string& from, const std::string& to)
{
	// Replaces substring from in str with to. Returns false when substring is not found.
	size_t start_pos = str.find(from);
	if (start_pos == std::string::npos)
	{
		return false;
	}
	str.replace(start_pos, from.length(), to);

	return true;
}

bool contains(std::string str_buf, std::string search_str)
{
	// Returns true if substring search_str (case insensitive) is found in str_buf.
	std::transform(str_buf.begin(), str_buf.end(), str_buf.begin(), ::tolower);
	size_t start_pos = str_buf.find(search_str);
	int len = str_buf.length();
	if (start_pos == std::string::npos)
	{
		return false;
	}
	else
	{
		return true;
	}
}
// --------------------------------------------------------------------