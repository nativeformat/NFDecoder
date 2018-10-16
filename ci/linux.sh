#!/bin/bash
# Copyright (c) 2018 Spotify AB.
#
# Licensed to the Apache Software Foundation (ASF) under one
# or more contributor license agreements.  See the NOTICE file
# distributed with this work for additional information
# regarding copyright ownership.  The ASF licenses this file
# to you under the Apache License, Version 2.0 (the
# "License"); you may not use this file except in compliance
# with the License.  You may obtain a copy of the License at
#
#   http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing,
# software distributed under the License is distributed on an
# "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
# KIND, either express or implied.  See the License for the
# specific language governing permissions and limitations
# under the License.

# Exit on any non-zero status
set -e
set -x

# DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null && pwd )"

# Install system dependencies
sudo apt-get -q update
sudo apt-get install -y -q --no-install-recommends apt-utils \
    ninja-build \
    clang-3.9 \
    clang-format-3.9 \
    libc++-dev \
    libcurl4-openssl-dev \
    libc++-dev \
    wget \
    libsndfile1 \
    git \
    unzip \
    software-properties-common \
    python-software-properties \
    make \
    libyaml-dev \
    libssl-dev \
    python-dev \
    python3-dev \
    python-virtualenv \
    build-essential \
    autoconf \
    libtool

# Extra repo for gcc-4.9 so we don't have to use 4.8
sudo add-apt-repository -y ppa:ubuntu-toolchain-r/test
sudo apt-get update
sudo apt-get install -y --no-install-recommends gcc-4.9 \
    g++-4.9 \
    gobjc++-4.9

sudo apt-get install -y --reinstall binutils build-essential

# Install cmake 3.6.x
wget --no-check-certificate https://cmake.org/files/v3.6/cmake-3.6.3-Linux-x86_64.sh -O cmake-3.6.3-Linux-x86_64.sh
chmod +x cmake-3.6.3-Linux-x86_64.sh
sudo sh cmake-3.6.3-Linux-x86_64.sh --prefix=/usr/local --exclude-subdir

# Update submodules
git submodule sync
git submodule update --init --recursive

export CC=clang-3.9
export CXX=clang++-3.9

# Install virtualenv
virtualenv --python=$(which python2) nfdecoder_env
. nfdecoder_env/bin/activate

# Install Python Packages
pip install -r ${PWD}/ci/requirements.txt

# Install gyp
cd tools/gyp
python setup.py install
cd ../..

# Execute our python build tools
if [ -n "$BUILD_ANDROID" ]; then
    # Install Android NDK
    NDK='android-ndk-r17b-linux-x86_64'
    ZIP='zip'
    wget https://dl.google.com/android/repository/${NDK}.${ZIP} -O ${PWD}/${NDK}.${ZIP}
    unzip -o -q ${NDK}.${ZIP}
    rm -rf ~/ndk
    mv android-ndk-r17b ~/ndk

    chmod +x -R ~/ndk

    python ci/androidlinux.py "$@"
else
    python ci/linux.py "$@"
fi
