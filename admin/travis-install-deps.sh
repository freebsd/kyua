#! /bin/sh

set -e -x

sudo apt-get update -qq
sudo apt-get install -y doxygen liblua5.2-0 liblua5.2-dev \
    libsqlite3-0 libsqlite3-dev pkg-config sqlite3

install_from_github() {
    local project="${1}"; shift
    local name="${1}"; shift
    local release="${1}"; shift

    local distname="${name}-${release}"

    local baseurl="https://github.com/jmmv/${project}"
    wget --no-check-certificate \
        "${baseurl}/releases/download/${distname}/${distname}.tar.gz"
    tar -xzvf "${distname}.tar.gz"

    cd "${distname}"
    ./configure \
        --disable-developer \
        --prefix=/usr \
        --without-atf \
        --without-doxygen
    make
    sudo make install
    cd -
}

install_from_github atf atf 0.20
install_from_github lutok lutok 0.4
install_from_github kyua kyua-testers 0.2
install_from_github kyua kyua-cli 0.8
