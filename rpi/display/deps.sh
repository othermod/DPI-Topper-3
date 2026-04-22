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

install_missing() {
    missing=""
    command -v dtc >/dev/null 2>&1 || missing="$missing dtc"
    command -v gcc >/dev/null 2>&1 || missing="$missing gcc"
    command -v make >/dev/null 2>&1 || missing="$missing make"
    command -v git >/dev/null 2>&1 || missing="$missing git"

    if [ -z "$missing" ]; then
        return
    fi

    OS=$(detect_os)
    echo "Missing tools:$missing"
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
}

clone_kernel() {
    if [ -d "$KERNEL_DIR/.git" ] && [ -f "$KERNEL_DIR/include/dt-bindings/gpio/gpio.h" ]; then
        echo "Kernel headers already present at $KERNEL_DIR"
        return
    fi
    if [ -d "$KERNEL_DIR" ]; then
        rm -rf "$KERNEL_DIR"
    fi
    echo "Cloning kernel source for dt-bindings headers (sparse checkout)..."
    git clone --depth 1 --filter=blob:none --sparse \
        https://github.com/torvalds/linux.git "$KERNEL_DIR"
    cd "$KERNEL_DIR"
    git sparse-checkout init --cone
    git sparse-checkout set include/dt-bindings
    echo "Kernel headers ready at $KERNEL_DIR"
}

install_missing
clone_kernel

echo "Dependencies installed."
