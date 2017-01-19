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

import argparse
import os
import shlex
import hashlib
import subprocess

import ffmpeg
from ruamel import yaml

if __name__ == '__main__':
    parser = argparse.ArgumentParser(description='Generate all CI CLI')
    parser.add_argument('cli_path', help='Path to CLI')
    parser.add_argument('ci_yaml', help='Path to ci.yaml')
    parser.add_argument('tmp_path', help='Path to cli output', default='/tmp/nf-int-output.wav')
    parser.add_argument('flac_dir', help='Path to flac output')
    args = parser.parse_args()

    with open(args.ci_yaml, 'r') as stream:
        cis = yaml.load(stream)
    # one of these days I'll move the rest of the script into python
    # for everything else there's mastercard
    for i in range(len(cis['integration_tests'])):
        ci = cis['integration_tests'][i]
        cli = [args.cli_path, # NFDecoder CLI
               ci['file'], # File to decode
               args.tmp_path, # Path to decoder output
               ci.get('offset', None), # offset
               ci.get('duration', None)] # duration
        # Remove nones
        cli = [str(x) for x in cli if x is not None]
        print(cli)
        subprocess.check_call(cli)
        md5hash = hashlib.md5(open(args.tmp_path, 'rb').read()).hexdigest()
        flac_file = '{md5}.flac'.format(md5=md5hash)
        full_flac_file = os.path.join(args.flac_dir, flac_file)
        ffmpeg.input(args.tmp_path).output(full_flac_file).run(overwrite_output=True)
        # remove tmp file because uh it helps expose errors
        os.remove(args.tmp_path)
        cis['integration_tests'][i]['audio'] = flac_file

    with open(os.path.join(args.flac_dir, 'ci.yaml'), 'w') as outyml:
        yml = yaml.YAML()
        yml.indent(mapping=2, sequence=4, offset=2)
        yml.dump(cis, outyml)

