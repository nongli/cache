#!/bin/bash

set -e

TP_DIR=$(cd "$(dirname "$BASH_SOURCE")"; pwd)
pwd $TP_DIR

docker run --rm --privileged \
  --name cache-build-shell \
  -v $TP_DIR/..:/cache \
  -it cerebro/cache-ci:0.1 \
  bash
