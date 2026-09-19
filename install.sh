#!/bin/bash
# Universal Installer for TTun

if [ "$EUID" -ne 0 ]; then
  echo -e "\033[31mError: Please run this script as root (sudo bash)\033[0m"
  exit 1
fi

REPO_URL="https://github.com/TheToex/ttun.git"
TMP_DIR="/tmp/ttun_build"

echo -e "\033[34m[*] Installing dependencies...\033[0m"
if command -v apt-get >/dev/null; then
    apt-get update && apt-get install -y gcc make git iproute2
elif command -v pacman >/dev/null; then
    pacman -Sy --noconfirm gcc make git iproute2
elif command -v dnf >/dev/null; then
    dnf install -y gcc make git iproute
else
    echo -e "\033[31m[-] Unsupported OS. Please install gcc, make, and git manually.\033[0m"
    exit 1
fi

echo -e "\033[34m[*] Downloading TTun source code...\033[0m"
rm -rf $TMP_DIR
git clone $REPO_URL $TMP_DIR
if [ $? -ne 0 ]; then
    echo -e "\033[31m[-] Failed to download the repository.\033[0m"
    exit 1
fi

echo -e "\033[34m[*] Compiling and Installing TTun...\033[0m"
cd $TMP_DIR
make
make install

echo -e "\033[34m[*] Cleaning up...\033[0m"
cd /
rm -rf $TMP_DIR

echo -e "\n\033[32m[✔] TTun Installed Successfully!\033[0m"
echo -e "Type \033[1;33msudo ttun\033[0m anywhere in your terminal to start."