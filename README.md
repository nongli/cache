# cache

## Download thirdparty dependencies

Typically just need to run this once:

```shell
dev/download-thirdparty.sh
```

## Build

```shell
mkdir -p build && cd build && cmake -H.. -B.
make -j8
./exe/build-check
```

### Test

```shell
cd build
make test
```

## CI
[![nongli](https://circleci.com/gh/nongli/cache.svg?style=svg)](https://app.circleci.com/pipelines/github/nongli)
