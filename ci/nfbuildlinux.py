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


class NFBuildLinux(NFBuild):
    def __init__(self):
        super(self.__class__, self).__init__()
        self.project_file = 'build.ninja'
        self.cmake_binary = 'cmake'
        self.android_ndk_folder = '~/ndk'
        self.clang_format_binary = 'clang-format-3.9'

    def generateProject(self,
                        ub_sanitizer=False,
                        address_sanitizer=False,
                        mem_sanitizer=False,
                        gcc=False):
        cmake_call = [
            self.cmake_binary,
            '..',
            '-GNinja',
            '-DINCLUDE_LGPL=1',
            '-DUSE_FFMPEG=1']
        if ub_sanitizer:
            cmake_call.append('-DUSE_UB_SANITIZER=1')
        if mem_sanitizer:
            cmake_call.append('-DUSE_MEM_SANITIZER=1')
        if address_sanitizer:
            cmake_call.append('-DUSE_ADDRESS_SANITIZER=1')
        if gcc:
            cmake_call.extend(['-DLLVM_STDLIB=0'])
        else:
            cmake_call.extend(['-DLLVM_STDLIB=1'])
        cmake_result = subprocess.call(cmake_call, cwd=self.build_directory)
        if cmake_result != 0:
            sys.exit(cmake_result)

    def buildTarget(self, target, sdk='', arch='x86_64'):
        ninja_result = subprocess.call([
            'ninja',
            '-C',
            self.build_directory,
            '-f',
            self.project_file,
            target])
        if ninja_result != 0:
            sys.exit(ninja_result)

    def addLibraryPath(self, library_path):
        library_path_environment_variable = "LD_LIBRARY_PATH"
        if library_path_environment_variable in os.environ:
            os.environ[library_path_environment_variable] += os.pathsep + library_path
        else:
            os.environ[library_path_environment_variable] = library_path
