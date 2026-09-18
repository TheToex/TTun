#!/bin/bash
# install.sh

if [ "$EUID" -ne 0 ]; then
  echo -e "\033[31mError: Please run this script as root (sudo ./install.sh)\033[0m"
  exit 1
fi

echo -e "\033[34m[*] Detecting OS and installing dependencies...\033[0m"
if command -v apt-get >/dev/null; then
    apt-get update && apt-get install -y gcc make iproute2
elif command -v pacman >/dev/null; then
    pacman -Sy --noconfirm gcc make iproute2
elif command -v dnf >/dev/null; then
    dnf install -y gcc make iproute
else
    echo -e "\033[31m[-] Unsupported Package Manager. Please install gcc and iproute2 manually.\033[0m"
    exit 1
fi

echo -e "\033[34m[*] Compiling TTun Core Engine...\033[0m"
if [ ! -f "src/ttun-core.c" ]; then
    echo -e "\033[31m[-] Error: src/ttun-core.c not found!\033[0m"
    exit 1
fi

gcc src/ttun-core.c -o /usr/bin/ttun-core -O2
if [ $? -ne 0 ]; then
    echo -e "\033[31m[-] Compilation failed!\033[0m"
    exit 1
fi

echo -e "\033[34m[*] Installing Services and CLI manager...\033[0m"
mkdir -p /etc/ttun
cp systemd/ttun@.service /etc/systemd/system/
systemctl daemon-reload

cp bin/ttun /usr/bin/ttun
chmod +x /usr/bin/ttun

echo -e "\n\033[32m[✔] TTun Installed Successfully!\033[0m"
echo -e "Type \033[1;33msudo ttun\033[0m anywhere in your terminal to start."