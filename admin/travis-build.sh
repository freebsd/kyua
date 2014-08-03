#! /bin/sh

set -e -x

for module in kyua-testers kyua-cli; do
    cd "${module}"

    autoreconf -i -s
    ./configure

    if [ "${AS_ROOT:-no}" = yes ]; then
        sudo make distcheck
    else
        make distcheck
    fi

    cd -
done
