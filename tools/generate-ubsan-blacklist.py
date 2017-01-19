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

from __future__ import print_function

import os
import subprocess
import sys


# list of source files to blacklist for ubsan
# relative to main NFDecoder project directory
bad_src_list = [
	'libraries/universal-dash-transmuxer/library/dash_to_hls_api.cc',
	'libraries/universal-dash-transmuxer/library/dash/dash_parser.cc'
]

bad_fun_list = [
	'dash2hls::DashParser::AddSpillover',
	'__ZN8dash2hls10DashParser12AddSpilloverEPKhm'
]


def main():
	if len(sys.argv) < 2:
		print('Must specify path for ubsan blacklist')
		sys.exit(1)
	path_prefix = sys.argv[1]
	blacklist_path = os.path.join(path_prefix, 'ubsan.blacklist')

	with open(blacklist_path, 'w') as f:
		print('# Generated with %s' % sys.argv[0], file=f)
		print('# Do not edit directly', file=f)
		for src in bad_src_list:
			abs_path = os.path.abspath(src)
			print('src:%s' % abs_path, file=f)
		for fun in bad_fun_list:
			print('fun:%s' % fun, file=f)


if __name__ == "__main__":
    main()

