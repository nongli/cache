#!/bin/bash

set -e

GFLAGS_VERSION=2.2.2
GTEST_VERSION=1.10.0

TP_DIR=$(cd "$(dirname "$BASH_SOURCE")"; pwd)
pushd $TP_DIR/../thirdparty

echo "Checking google-release-${GTEST_VERSION}..."
if [ ! -d googletest-release-${GTEST_VERSION} ]; then
  echo "   fetching ..."
  rm -f release-${GTEST_VERSION}.tar.gz
  wget https://github.com/google/googletest/archive/release-${GTEST_VERSION}.tar.gz
  tar xzf release-${GTEST_VERSION}.tar.gz
  rm release-${GTEST_VERSION}.tar.gz
  rm -f gtest
  ln -s googletest-release-${GTEST_VERSION} gtest
else
  echo "   gtest already downloaded"
fi

echo "Checking gflags-${GFLAGS_VERSION}..."
if [ ! -d gflags-${GFLAGS_VERSION} ]; then
  echo "   fetching ..."
  rm -f v${GFLAGS_VERSION}.tar.gz
  wget https://github.com/gflags/gflags/archive/v${GFLAGS_VERSION}.tar.gz
  tar xzf v${GFLAGS_VERSION}.tar.gz
  rm v${GFLAGS_VERSION}.tar.gz
  rm -f gflags
  ln -s gflags-${GFLAGS_VERSION} gflags
else
  echo "   gflags already downloaded"
fi

popd
