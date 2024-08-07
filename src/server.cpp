#include <iostream>
#include <cstdlib>
#include <string>
#include <cstring>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <vector>

std::vector<std::string> split(const std::string& s, const std::string& delimiter) {
    size_t pos = 0;
    size_t prev_pos = 0;
    std::vector<std::string> result;

    while ((pos = s.find(delimiter, prev_pos)) != std::string::npos) {
        std::string token = s.substr(prev_pos, pos - prev_pos);
        result.push_back(token);
        prev_pos = pos + delimiter.length();
    }
    result.push_back(s.substr(prev_pos));

    return result;
}

int main(int argc, char **argv) {
    std::cout << std::unitbuf;
    std::cerr << std::unitbuf;

    std::cout << "Logs from your program will appear here!\n";

    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        std::cerr << "Failed to create server socket\n";
        return 1;
    }

    int reuse = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0) {
        std::cerr << "setsockopt failed\n";
        return 1;
    }

    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(4221);

    if (bind(server_fd, (struct sockaddr *) &server_addr, sizeof(server_addr)) != 0) {
        std::cerr << "Failed to bind to port 4221\n";
        return 1;
    }

    int connection_backlog = 5;
    if (listen(server_fd, connection_backlog) != 0) {
        std::cerr << "listen failed\n";
        return 1;
    }

    struct sockaddr_in client_addr;
    int client_addr_len = sizeof(client_addr);

    std::cout << "Waiting for a client to connect...\n";

    int client = accept(server_fd, (struct sockaddr *) &client_addr, (socklen_t *) &client_addr_len);
    if (client < 0) {
        std::cerr << "Failed to accept client connection\n";
        return 1;
    }

    std::cout << "Client connected\n";

    char buffer[2048];
    memset(buffer, 0, sizeof(buffer));
    recv(client, buffer, sizeof(buffer) - 1, 0);

    std::string request(buffer);
    std::cout << request << std::endl;
    std::string request_line = request.substr(0, request.find("\r\n"));
    std::cout << "Request: " << request_line << std::endl;

    size_t method_end = request_line.find(' ');
    size_t path_end = request_line.find(' ', method_end + 1);
    std::string path = request_line.substr(method_end + 1, path_end - method_end - 1);

    std::cout << "Path: " << path << std::endl;

    std::string http_response;
    if (path == "/") {
        http_response =
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: text/html\r\n"
            "Content-Length: 47\r\n"
            "Connection: close\r\n"
            "\r\n"
            "<html><body><h1>Hello from your server!</h1></body></html>";
    } else if (path.rfind("/echo", 0) == 0) {
        std::string message = path.substr(6);
        http_response =
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: text/plain\r\n"
            "Content-Length: " + std::to_string(message.size()) + "\r\n"
            "Connection: close\r\n"
            "\r\n"
            + message;
    } else if (path.rfind("/user-agent", 0) == 0) {
        std::string user_agent = "Unknown";
        size_t header_start = request.find("\r\nUser-Agent: ");
        if (header_start != std::string::npos) {
            header_start += std::string("\r\nUser-Agent: ").length();
            size_t header_end = request.find("\r\n", header_start);
            user_agent = request.substr(header_start, header_end - header_start);
        }

        http_response =
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: text/plain\r\n"
            "Content-Length: " + std::to_string(user_agent.size()) + "\r\n"
            "Connection: close\r\n"
            "\r\n"
            + user_agent;
    } else {
        http_response =
            "HTTP/1.1 404 Not Found\r\n"
            "Content-Type: text/html\r\n"
            "Content-Length: 23\r\n"
            "Connection: close\r\n"
            "\r\n"
            "<html><body><h1>404 Not Found</h1></body></html>";
    }

    // Send the response
    send(client, http_response.c_str(), http_response.length(), 0);

    close(client);
    close(server_fd);

    return 0;
}
