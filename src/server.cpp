#include <iostream>
#include <thread>
#include <string>
#include <cstring>
#include <cstdlib>
#include <netdb.h>
#include <unistd.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include<fstream>
#include<map>

void handleClient(int client_fd, std::string files_directory) {
    char buffer[2048];
    memset(buffer, 0, sizeof(buffer));
    recv(client_fd, buffer, sizeof(buffer) - 1, 0);

    std::string request(buffer);
    std::string request_line = request.substr(0, request.find("\r\n"));
    std::cout << "Request: " << request_line << std::endl;

    size_t method_end = request_line.find(' ');
    size_t path_end = request_line.find(' ', method_end + 1);
    std::string method = request_line.substr(0, method_end);
    std::string path = request_line.substr(method_end + 1, path_end - method_end - 1);

    std::map<std::string, std::string> headers;
    size_t header_start = request.find("\r\n");
    if (header_start != std::string::npos) {
        std::string headers_str = request.substr(header_start + 2);
        size_t header_end = 0;
        while ((header_end = headers_str.find("\r\n")) != std::string::npos) {
            std::string header = headers_str.substr(0, header_end);
            size_t colon_pos = header.find(": ");
            if (colon_pos != std::string::npos) {
                std::string key = header.substr(0, colon_pos);
                std::string value = header.substr(colon_pos + 2);
                headers[key] = value;
            }
            headers_str = headers_str.substr(header_end + 2);
        }
    }
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
        if(headers.find("Accept-Encoding") != headers.end() && headers["Accept-Encoding"] == "gzip") {
            http_response =
                "HTTP/1.1 200 OK\r\n"
                "Content-Type: text/plain\r\n"
                "Content-Encoding: gzip\r\n"
                "Content-Length: " + std::to_string(message.size()) + "\r\n"
                "Connection: close\r\n"
                "\r\n"
                + message;
        }
        else{
            http_response =
                "HTTP/1.1 200 OK\r\n"
                "Content-Type: text/plain\r\n"
                "Content-Length: " + std::to_string(message.size()) + "\r\n"
                "Connection: close\r\n"
                "\r\n"
                + message;
        }
    } else if (path.rfind("/user-agent", 0) == 0) {
        http_response =
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: text/plain\r\n"
            "Content-Length: " + std::to_string(headers["User-Agent"].size()) + "\r\n"
            "Connection: close\r\n"
            "\r\n"
            + headers["User-Agent"];
    } else if (path.rfind("/files", 0) == 0) {

        std::string filePath = files_directory + path.substr(6);
        if(method == "GET") {
            std::ifstream file(filePath, std::ios::binary);

            if (file.is_open()) {
                std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
                file.close();

                http_response =
                    "HTTP/1.1 200 OK\r\n"
                    "Content-Type: application/octet-stream\r\n"
                    "Content-Length: " + std::to_string(content.size()) + "\r\n"
                    "Connection: close\r\n"
                    "\r\n" +
                    content;
            } else {
                http_response =
                    "HTTP/1.1 404 Not Found\r\n"
                    "Content-Type: text/html\r\n"
                    "Content-Length: 23\r\n"
                    "Connection: close\r\n"
                    "\r\n"
                    "<html><body><h1>404 Not Found</h1></body></html>";
            }
        } else if (method == "POST") {
            std::string content = request.substr(request.find("\r\n\r\n") + 4);

            std::ofstream file(filePath, std::ios::binary);
            if (file.is_open()) {
                file << content;
                file.close();

                http_response =
                    "HTTP/1.1 201 Created\r\n\r\n";
            } else {
                http_response =
                    "HTTP/1.1 404 Not Found\r\n"
                    "Content-Type: text/html\r\n"
                    "Content-Length: 23\r\n"
                    "Connection: close\r\n"
                    "\r\n"
                    "<html><body><h1>404 Not Found</h1></body></html>";
            }
        }
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
    send(client_fd, http_response.c_str(), http_response.length(), 0);

    close(client_fd);


}
int main(int argc, char **argv) {
    std::cout << std::unitbuf;
    std::cerr << std::unitbuf;
    std::string files_directory = "/tmp/";
    if(argc > 1)
        for(int i = 1; i < argc; i++){
            if(strcmp(argv[i],  "--directory") == 0){
                std::cout<<"adding new directory "<<std::endl;
                if(i+1 < argc){
                    files_directory = argv[i+1];
                }else{
                    std::cerr<<"no directory specified"<<std::endl;
                    return 1;
                }
            }
        }

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

    while(true){
        int client = accept(server_fd, (struct sockaddr *) &client_addr, (socklen_t *) &client_addr_len);
        if (client < 0) {
            std::cerr << "Failed to accept client connection\n";
            return 1;
        }

        std::cout << "Client connected\n";
        std::thread(handleClient, client, files_directory).detach();
    }

    close(server_fd);

    return 0;
}
