#!/bin/sh

autoreconf --install --verbose || exit 
[  -f ./configure ] && ./configure $@

