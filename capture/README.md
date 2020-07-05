# capture

voix9 に必要なパケットをキャプチャ・加工し、Redis Stream に追加します。

## 必要なソフトウェア

- libpcap-devel
- openssl-devel
- make
- gcc-c++
- [boost](https://www.boost.org/) >= 1.67.0
- [CMake](https://cmake.org/) >= 2.12.0


## ビルド方法

```Shell
# Create build directory
mkdir build
cd build/

# Configure
cmake ../

# Compile
make
```

