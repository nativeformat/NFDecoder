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

# Install system dependencies
# sudo apt-get update
sudo apt-get install -y ninja-build
sudo apt-get install -y clang-3.8
sudo apt-get install -y libc++-dev
sudo apt-get install -y libcurl4-openssl-dev
sudo apt-get install -y wget
sudo apt-get install -y libsndfile1

export CC=clang-3.8
export CXX=clang++-3.8

# Install virtualenv
VIRTUALENV_LOCAL_PATH='/virtualenv-15.1.0/virtualenv.py'
VIRTUALENV_PATH=`python tools/vulcan/bin/vulcan.py -v -f tools/virtualenv.vulcan -p virtualenv-15.1.0`
VIRTUALENV_PATH=$VIRTUALENV_PATH$VIRTUALENV_LOCAL_PATH
$VIRTUALENV_PATH nfdecoder_env
. nfdecoder_env/bin/activate

# Install Python Packages
pip install pyyaml
pip install flake8
pip install cmakelint
pip install numpy
pip install numba==0.35.0
pip install pysoundfile
pip install numpy

# Install gyp
cd tools/gyp
python setup.py install
cd ../..

# Execute our python build tools
python ci/linux.py "$@"
