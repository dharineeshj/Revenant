
### 📖 **Project Revenant** 

**Project Revenant** is a open-source Command and Control (C2) server written in **C++**, designed to automate the entire reverse shell lifecycle—from **tunnel creation** to **connection handling**—with minimal user intervention.

What sets Revenant apart is its ability to **automatically create a Cloudflared tunnel** and **embed the live tunnel URL into the generated payload**, eliminating the need for manual configuration or static IP setups. This makes it effective even in environments where victims are behind NAT or firewalls.

In addition to payload generation, Revenant provides an **interactive terminal interface** for managing victims, issuing commands, and monitoring live connections. It offers a clear, session-based mechanism to interact with multiple victims through unique identifiers—making remote command execution intuitive and efficient.

---

### 🔍 Key Differentiator

> **Project Revenant is currently the only known open-source C2 framework written in C++ that automatically sets up a Cloudflared tunnel, embeds the live URL into payloads, and provides a built-in interface for real-time connection management.**

---

## Features

* Fully written in modern C++ using socket programming and multithreading.
* Generates reverse shell payloads for **Windows** and **Linux**.
* Automatically creates a Cloudflared tunnel to expose the C2 server to the internet.
* Victim identification system using dynamic Victim IDs.
* Persistent interactive shell sessions for real-time command execution.
* URL-encoded HTTP-based command-and-response mechanism for communication.
* Captures command output from the victim and logs it back to the attacker.

---

## Requirements

To run Project Revenant, ensure your system has the following:

### Dependencies

* `g++` — C++ compiler
* `cloudflared` — Binary from Cloudflare to create secure tunnels

You can install the required dependencies using the included setup script:

```bash
sudo sh requirments.sh
```

---

## Building the C2 Server

Compile the server using the GNU C++ compiler with pthread support:

```bash
g++ c2_server.cpp -o c2
```

If you run into issues during compilation, ensure all dependencies are installed and try again.

---

## Running the Server

Once compiled, start the server with root privileges:

```bash
sudo ./c2
```

The server performs the following on startup:

1. Picks an available local port.
2. Launches a Cloudflared tunnel exposing that port.
3. Starts listening for victim connections.
4. Opens an interactive terminal for managing connections.

---

## Server Interface and Available Commands

After launching the server, you will enter an interactive command-line interface (CLI) called the **MainShell**. The following commands are available:

| Command               | Description                                                   |
| --------------------- | ------------------------------------------------------------- |
| `help`                | Displays a list of available commands.                        |
| `banner`              | Shows the project’s banner/logo.                              |
| `generate os=windows` | Generates a reverse shell payload for Windows systems.        |
| `generate os=linux`   | Generates a reverse shell payload for Linux systems.          |
| `show shell`          | Lists all currently connected victim sessions.                |
| `shell <Victim_id>`   | Starts an interactive shell session with the selected victim. |
| `quit`                | Gracefully stops the server and shuts down the tunnel.        |

Each victim is assigned a unique `Victim_id` upon connection, which is used to manage individual sessions.

---

## Payload Functionality Overview

The generated payload—whether for Linux or Windows—is a lightweight script designed to run in the background on the victim machine. It establishes a reverse connection to the attacker's server via a Cloudflared tunnel, continuously polling for commands. When a command is received, it is executed locally, and the output is sent back to the server. The Linux version uses Bash, while the Windows version utilizes PowerShell, but both follow the same communication logic for command execution and result delivery.

---

## Ethical Usage and Disclaimer

This project is intended strictly for **educational** and **ethical research** purposes only.

Using this tool against systems that you do not own or have explicit written permission to test is illegal and unethical. It may violate computer misuse laws in your country or region.

**The author(s) of this project do not take responsibility for any misuse or unauthorized use of this code.** The sole intent is to help security researchers, students, and ethical hackers understand how reverse shells and C2 infrastructures function from the inside.

Use responsibly and always act within the boundaries of the law and with informed consent.

---
