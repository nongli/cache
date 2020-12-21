#!/bin/bash

set -e

GTEST_VERSION=1.10.0
TP_DIR=$(cd "$(dirname "$BASH_SOURCE")"; pwd)
pushd $TP_DIR/../thirdparty

echo "Fetching googletest-release-${GTEST_VERSION}"
pwd
rm -rf release-${GTEST_VERSION}.tar.gz
wget https://github.com/google/googletest/archive/release-${GTEST_VERSION}.tar.gz
tar xzf release-${GTEST_VERSION}.tar.gz
rm release-${GTEST_VERSION}.tar.gz
rm -rf googltest-release
mv googletest-release-${GTEST_VERSION} googletest-release

popd
