# voix9

voix9 はオフィス等で使用されるIP電話での通話内容を、  
PC等でリアルタイムに再生および会話の内容をテキスト化して出力するソフトウェアです。

## 構成

voix9 は capture と webapi の2つで構成されます。
データストアには Redis を使用します。

capture はIP電話での通話内容を、ネットワークからキャプチャーし、Redisに保存します。
webapi は capture が Redis に保存したデータをPC等に配信します。

## 必要なソフトウェア

- Redis >= 5
- libpcap-devel
- openssl-devel
- make
- gcc-c++
- [boost](https://www.boost.org/) >= 1.67.0
- [CMake](https://cmake.org/) >= 2.12.0
- Node.js >= 12

また、submodue として、[libtins] と [redis-cpp] を使用しています。
`git submodule update -i` で submodule の取り込みをしてください。

CentOS8を使用している場合、`scripts/setup_requirements_centos8.sh` を使用して、必要なソフトウェアを一括でインストールできます。

## ビルドと実行

`capture/README.md` および `forntend/README.md` を参照してください。

