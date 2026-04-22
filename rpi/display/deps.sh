#!/bin/sh
set -e

KERNEL_DIR="${KERNEL_DIR:-$(dirname "$0")/linux}"
KERNEL_BRANCH="${KERNEL_BRANCH:-v6.6}"

detect_os() {
    if [ -f /etc/os-release ]; then
        . /etc/os-release
        echo "$ID"
    elif [ -f /etc/alpine-release ]; then
        echo "alpine"
    elif [ -f /etc/fedora-release ]; then
        echo "fedora"
    elif [ -f /etc/debian_version ]; then
        echo "debian"
    elif [ -f /etc/redhat-release ]; then
        echo "rhel"
    elif command -v brew >/dev/null 2>&1; then
        echo "darwin"
    else
        echo "unknown"
    fi
}

install_alpine() {
    apk add --no-cache dtc gcc make git
}

install_debian() {
    apt-get update -qq
    apt-get install -y -qq device-tree-compiler gcc make git
}

install_fedora() {
    dnf install -y dtc gcc make git
}

install_rhel() {
    yum install -y dtc gcc make git
}

install_darwin() {
    brew install dtc gcc make git
}

clone_kernel() {
    if [ -d "$KERNEL_DIR/.git" ]; then
        echo "Kernel source already cloned at $KERNEL_DIR"
        return
    fi
    echo "Cloning kernel source for dt-bindings headers..."
    git clone --depth 1 --branch "$KERNEL_BRANCH" https://github.com/torvalds/linux.git "$KERNEL_DIR"
}

OS=$(detect_os)
echo "Detected OS: $OS"

case "$OS" in
    alpine)   install_alpine ;;
    debian|ubuntu) install_debian ;;
    fedora)   install_fedora ;;
    rhel|centos|rocky|amzn) install_rhel ;;
    darwin)   install_darwin ;;
    *)
        echo "Unsupported OS: $OS"
        echo "Install dtc, gcc, make, and git manually, then run make."
        exit 1
        ;;
esac

clone_kernel

echo "Dependencies installed."
