#!/bin/bash

set -e

if [ "$#" -ne 1 ]; then
  echo "usage: trim-trace.sh <raw path>"
  exit 1
fi

cat $1 | awk '{print $NF}' | awk -F\, '{print $1, $2}'
