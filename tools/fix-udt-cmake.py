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

import fileinput
import os
import subprocess
import sys
import re

def main():
  if len(sys.argv) < 2:
    print('Missing input paths')
    return
  cmake_current_source_dir = sys.argv[1]
  cmakelists = os.path.join(cmake_current_source_dir,
    'universal-dash-transmuxer/library/out/Default/CMakeLists.txt')
  print('CMAKE CURRENT SOURCE DIR: ' + cmake_current_source_dir)
  print('CMAKELISTS: ' + cmakelists)
  print(os.listdir(cmake_current_source_dir))
  print(os.listdir(cmake_current_source_dir + '/universal-dash-transmuxer'))
  print(os.listdir(cmake_current_source_dir + '/universal-dash-transmuxer/library'))
  print(os.listdir(cmake_current_source_dir + '/universal-dash-transmuxer/library/out'))

  for line in fileinput.input(cmakelists, inplace=True, backup='.bak'):
    line = line.replace('USE_AVFRAMEWORK;', '')
    # let cmake figure out the arch
    line = line.replace('-arch i386', '')
    line = line.replace('-arch x86_64', '')
    line = line.replace('DashToHls_osx.pch',
      cmake_current_source_dir +
      '/universal-dash-transmuxer/library/DashToHls_osx.pch')
    line = line.replace('DashToHls_ios.pch',
      cmake_current_source_dir +
      '/universal-dash-transmuxer/library/DashToHls_ios.pch')
    if os.name == 'nt':
      line = line.replace('\\', '/')
    print(line)


if __name__ == "__main__":
    main()
