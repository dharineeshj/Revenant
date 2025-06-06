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
using namespace std;

const int PORT = 8080;
const int BUFFER_SIZE = 1024;

unordered_map<string, int> mp;
string user_input;
int client_id = 0;

void serve_client(string endpoint);
void help();
string start_cloudflared();

string get_endpoint(const char* buffer) {
    string request_line(buffer);

    size_t first_space = request_line.find(' ');
    if (first_space ==  string::npos) return "";

    size_t second_space = request_line.find(' ', first_space + 1);
    if (second_space ==  string::npos) return "";

     string path = request_line.substr(first_space + 1, second_space - first_space - 1);


    if (!path.empty() && path[0] == '/') {
        path = path.substr(1);
    }

    return path;
}


void shell(unordered_map<string,int> &mp){
 
    while (true) {
        string id;
        cout << "> ";
        getline(cin >> ws, id);  
        vector<string> tokens;
        stringstream ss(id);
        string token;

        while (std::getline(ss, token,' ')) {
            tokens.push_back(token);
        }
        
        unordered_set<string> se(tokens.begin(),tokens.end());
        if(id=="help"){
            help();
        }
        else if (id=="banner"){
            banner();
        }
        else if (se.find("show")!=se.end()){

        }
        else if (se.find("shell")!=se.end()){
            cout<<tokens[tokens.size()-1];
            if (mp.find(tokens[tokens.size()-1]) != mp.end()) {
                serve_client(tokens[tokens.size()-1]);
            } else {
                cout << "Client ID not found.\n";
            }
        }
    }

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
        if(mp.find(endpoint)==mp.end()){
            cout<<"\n New connection "<<endpoint<<"\n"<<"> "<<flush;
        }
        string body="";
        mp[endpoint] = client_conn;
        string response = "HTTP/1.1 200 OK\r\n"
                            "Content-Type: text/plain\r\n"
                            "Content-Length: " + to_string(body.length()) + "\r\n"
                            "Connection: keep-alive\r\n"
                            "\r\n" + body;

        send(mp[endpoint], response.c_str(), response.length(), 0);
   
    }
}

void serve_client(string endpoint) {
    bool flag = true;
    auto input_func = [&]() {
        while (flag) {
            cout << "Enter the input: ";
            cin >> user_input;
            if(user_input == "exit"){
                flag=false;
     
            }
        }
    };

    auto fut =  async( launch::async, input_func);
  
    while (true) {
        
        char buffer[BUFFER_SIZE] = {0};
        ssize_t valread = read(mp[endpoint], buffer, BUFFER_SIZE - 1);
        if (valread <= 0) {

            close(mp[endpoint]);
            continue;
        }
        string body = user_input;
        string response = "HTTP/1.1 200 OK\r\n"
                            "Content-Type: text/plain\r\n"
                            "Content-Length: " + to_string(body.length()) + "\r\n"
                            "Connection: keep-alive\r\n"
                            "\r\n" + body;

        send(mp[endpoint], response.c_str(), response.length(), 0);
        user_input = "";
        if(flag==false){
            break;
        }
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

    if (listen(server_fd, 10) < 0) {
        perror("Listening failed");
        exit(EXIT_FAILURE);
    }

    cout << "HTTP server running on port " << PORT << endl;

    // Start client_connect in a thread:
    thread client_thread(client_connect, server_fd);
    client_thread.detach();

    // Main thread: select client to serve
    shell(mp);

    close(server_fd);
}

int main() {
    string cloudflared_url=start_cloudflared();
    if (cloudflared_url.size()<=0){
        cout<<"Failed to create tunnel";
        return 0;
    }
    cout<<"Tunnel created successfully"<<endl;
    server();
    return 0;
}