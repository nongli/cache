# cache

## Download thirdparty dependencies

Typically just need to run this once:

```shell
dev/download-thirdparty.sh
```

## Build

### Dev build

```shell
mkdir -p build/debug && cd build/debug && cmake -H../.. -B.
make -j8
./exe/build-check
```

### Release build

```shell
mkdir -p build/release && cd build/release && cmake -DCMAKE_BUILD_TYPE=RELEASE -H../.. -B.
make -j8
./exe/build-check
```

### Test

```shell
cd build/debug
make test
```

## CI
[![nongli](https://circleci.com/gh/nongli/cache.svg?style=svg)](https://app.circleci.com/pipelines/github/nongli)
