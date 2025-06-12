#!/bin/bash

# requirements.sh - Script to install dependencies for Project Revenant

echo "[*] Updating system packages..."
sudo apt update -y

echo "[*] Installing g++ (GNU C++ compiler)..."
sudo apt install -y g++

echo "[*] Installing cloudflared..."
if ! command -v cloudflared &> /dev/null; then
    echo "[*] Downloading cloudflared..."
    wget -q https://github.com/cloudflare/cloudflared/releases/latest/download/cloudflared-linux-amd64.deb -O cloudflared.deb

    echo "[*] Installing cloudflared package..."
    sudo dpkg -i cloudflared.deb

    echo "[*] Cleaning up..."
    rm cloudflared.deb
else
    echo "[+] Cloudflared is already installed."
fi

echo "[+] All dependencies installed successfully."