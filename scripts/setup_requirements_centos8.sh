#!/bin/sh
cd ~/
dnf install -y make gcc-c++ openssl openssl-devel
dnf install -y libpcap libpcap-devel --enablerepo=PowerTools
# nodejs/12 on AppStream and nodesource have a noisy logging bug: https://bugs.centos.org/view.php?id=17047
#dnf module install -y nodejs:12/common
curl -sL https://rpm.nodesource.com/setup_14.x | bash -
dnf install -y nodejs 
dnf module install -y redis:5/common && systemctl enable redis && systemctl start redis
mkdir -p ~/voix9_setup_requirements
cd ~/voix9_setup_requirements && wget https://github.com/Kitware/CMake/releases/download/v3.17.3/cmake-3.17.3.tar.gz && tar zxf cmake-3.17.3.tar.gz && cd cmake-3.17.3/ && ./bootstrap && make && make install
cd ~/voix9_setup_requirements && wget https://dl.bintray.com/boostorg/release/1.73.0/source/boost_1_73_0.tar.gz && tar zxf boost_1_73_0.tar.gz && cd boost_1_73_0/ && ./bootstrap.sh && ./b2 install
cd ~/
