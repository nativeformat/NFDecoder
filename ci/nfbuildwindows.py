#!/usr/bin/env python
'''
 * Copyright (c) 2018 Spotify AB.
 *
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 '''


import fnmatch
import os
import plistlib
import re
import shutil
import subprocess
import sys

from distutils import dir_util
from nfbuild import NFBuild


class NFBuildWindows(NFBuild):
    def __init__(self):
        super(self.__class__, self).__init__()
        self.project_file = 'build.ninja'

    def installClangFormat(self):
        clang_format_vulcan_file = os.path.join('tools', 'clang-format.vulcan')
        clang_format_extraction_folder = self.vulcanDownload(
            clang_format_vulcan_file,
            'clang-format-5.0.0')
        self.clang_format_binary = os.path.join(
            os.path.join(
                os.path.join(
                    clang_format_extraction_folder,
                    'clang-format'),
                'bin'),
            'clang-format')

    def installNinja(self):
        ninja_vulcan_file = os.path.join(
            os.path.join(
                os.path.join(
                    os.path.join('tools', 'buildtools'),
                    'spotify_buildtools'),
                'software'),
            'ninja.vulcan')
        ninja_extraction_folder = self.vulcanDownload(
            ninja_vulcan_file,
            'ninja-1.6.0')
        self.ninja_binary = os.path.join(
            ninja_extraction_folder,
            'ninja')
        if 'PATH' not in os.environ:
            os.environ['PATH'] = ''
        if len(os.environ['PATH']) > 0:
            os.environ['PATH'] += os.pathsep
        os.environ['PATH'] += ninja_extraction_folder

    def installMake(self):
        make_vulcan_file = os.path.join('tools', 'make.vulcan')
        make_extraction_folder = self.vulcanDownload(
            make_vulcan_file,
            'make-4.2.1')
        make_bin_folder = os.path.join(
            make_extraction_folder,
            'bin')
        os.environ['PATH'] += os.pathsep + make_bin_folder

    def installVulcanDependencies(self, android=False):
        super(self.__class__, self).installVulcanDependencies(android)
        self.installClangFormat()
        self.installMake()
        if android:
          self.installNinja()

    def generateProject(self,
                        ios=False,
                        android=False,
                        android_arm=False):
        self.use_ninja = android or android_arm
        cmake_call = [
            self.cmake_binary,
            '..',
            '-GNinja']
        if android or android_arm:
            android_abi = 'x86_64'
            android_toolchain_name = 'x86_64-llvm'
            if android_arm:
                android_abi = 'arm64-v8a'
                android_toolchain_name = 'arm64-llvm'
            cmake_call.extend([
                '-DANDROID=1',
                '-DCMAKE_TOOLCHAIN_FILE=' + self.android_ndk_folder + '/build/cmake/android.toolchain.cmake',
                '-DANDROID_NDK=' + self.android_ndk_folder,
                '-DANDROID_ABI=' + android_abi,
                '-DANDROID_NATIVE_API_LEVEL=21',
                '-DANDROID_TOOLCHAIN_NAME=' + android_toolchain_name,
                '-DANDROID_WINDOWS=1',
                '-DANDROID_STL=c++_shared'])
        cmake_result = subprocess.call(cmake_call, cwd=self.build_directory)
        if cmake_result != 0:
            sys.exit(cmake_result)

    def buildTarget(self, target, sdk='macosx', arch='x86_64'):
        result = subprocess.call([
            self.ninja_binary,
            '-C',
            self.build_directory,
            '-f',
            self.project_file,
            target])
        if result != 0:
            sys.exit(result)
