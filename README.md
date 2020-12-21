# cache

## Download thirdparty dependencies

Typically just need to run this once:
$ dev/download-thirdparty.sh

## Build
mkdir -p build && cd build && cmake -H.. -B.
make
./exe/build-check

