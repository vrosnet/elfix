#!/bin/sh

aclocal && \
autoheader && \
autoconf && \
libtoolize --copy && \
automake --add-missing --copy

cd doc
./make.sh
