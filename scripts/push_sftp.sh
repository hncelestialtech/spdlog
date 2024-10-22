#!/bin/bash
SFTP_USER=$1
SFTP_HOST=$2
SFTP_PORT=$3

echo -e "\033[32m[+] Creating tmp directory\033[0m"
mkdir -p tmp

echo -e "\033[32m[+] Downloading zip file\033[0m"
wget https://github.com/hncelestialtech/spdlog/archive/refs/heads/stable.zip -O tmp/cel_spdlog.zip

# Check if the zip file is empty
if [ -s tmp/cel_spdlog.zip ]; then
    echo -e "\033[32m[+] Zip file is not empty\033[0m"
else
    echo -e "\033[31m[-] Zip file is empty\033[0m"
    exit 1
fi

echo -e "\033[32m[+] Uploading zip file\033[0m"
sftp -P $SFTP_PORT $SFTP_USER@$SFTP_HOST <<EOF
put tmp/cel_spdlog.zip /deps/cel_spdlog.zip
EOF

echo -e "\033[32m[+] Removing tmp directory\033[0m"
rm -rf tmp