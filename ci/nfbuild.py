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
import numpy
import pprint
import shutil
import soundfile as sf
import subprocess
import sys
import yaml


class NFBuild(object):
    def __init__(self):
        ci_yaml_file = os.path.join('ci', 'ci.yaml')
        self.build_configuration = yaml.load(open(ci_yaml_file, 'r'))
        self.pretty_printer = pprint.PrettyPrinter(indent=4)
        self.current_working_directory = os.getcwd()
        self.build_directory = 'build'
        self.build_type = 'Release'
        self.output_directory = os.path.join(self.build_directory, 'output')
        self.statically_analyzed_files = []
        self.nfdecoder_test_audio_extraction_folder = 'resources'
        self.ffmpeg_binary = 'ffmpeg'
        self.clang_format_binary = 'clang-format'

    def build_print(self, print_string):
        print print_string
        sys.stdout.flush()

    def makeBuildDirectory(self):
        if not os.path.exists(self.build_directory):
            os.makedirs(self.build_directory)
        else:
            shutil.rmtree(os.path.join(self.build_directory, 'output'))
        os.makedirs(self.output_directory)

    def addLibraryPath(self, library_path):
        assert True, "addLibraryPath should be overridden by subclass"

    def installDependencies(self, android=False):
        pass

    def generateProject(self,
                        ios=False,
                        use_ffmpeg=False,
                        android=False,
                        android_arm=False):
        assert True, "generateProject should be overridden by subclass"

    def buildTarget(self, target, sdk='macosx'):
        assert True, "buildTarget should be overridden by subclass"

    def targetBinary(self, target):
        for root, dirnames, filenames in os.walk(self.build_directory):
            for filename in fnmatch.filter(filenames, target):
                full_target_file = os.path.join(root, filename)
                return full_target_file
        return ''

    def lintCPPFile(self, filepath, make_inline_changes=False):
        current_source = open(filepath, 'r').read()
        clang_format_call = [self.clang_format_binary]
        if make_inline_changes:
            clang_format_call.append('-i')
        clang_format_call.append(filepath)
        new_source = subprocess.check_output(clang_format_call)
        if current_source != new_source and not make_inline_changes:
            self.build_print(
                filepath + " failed C++ lint, file should look like:")
            self.build_print(new_source)
            return False
        return True

    def lintCPPDirectory(self, directory, make_inline_changes=False):
        passed = True
        for root, dirnames, filenames in os.walk(directory):
            for filename in filenames:
                if not filename.endswith(('.cpp', '.h', '.m', '.mm')):
                    continue
                full_filepath = os.path.join(root, filename)
                if not self.lintCPPFile(full_filepath, make_inline_changes):
                    passed = False
        return passed

    def lintCPP(self, make_inline_changes=False):
        lint_result = self.lintCPPDirectory('source', make_inline_changes)
        lint_result &= self.lintCPPDirectory('include', make_inline_changes)
        if not lint_result:
            sys.exit(1)

    def lintCmakeFile(self, filepath):
        self.build_print("Linting: " + filepath)
        return subprocess.call(['cmakelint', filepath]) == 0

    def lintCmakeDirectory(self, directory):
        passed = True
        for root, dirnames, filenames in os.walk(directory):
            for filename in filenames:
                if not filename.endswith('CMakeLists.txt'):
                    continue
                full_filepath = os.path.join(root, filename)
                if not self.lintCmakeFile(full_filepath):
                    passed = False
        return passed

    def lintCmake(self):
        lint_result = self.lintCmakeFile('CMakeLists.txt')
        lint_result &= self.lintCmakeDirectory('source')
        if not lint_result:
            sys.exit(1)

    def runIntegrationTests(self):
        cli_target_name = 'NFDecoderCLI'
        cli_binary = self.targetBinary(cli_target_name)
        integration_test_output_file = 'output.flac'
        self.buildTarget(cli_target_name)
        nfdecoder_wav = 'nfdecoder.wav'
        input_wav = 'input.wav'
        output_wav = 'output.wav'
        loglevel = 'panic'
        os.environ['UBSAN_OPTIONS'] = 'print_stacktrace=1'
        os.environ['ASAN_OPTIONS'] = 'symbolize=1'
        os.environ['ASAN_SYMBOLIZER_PATH'] = '/usr/lib/llvm-6.0/bin/llvm-symbolizer'
        for integration_test in self.build_configuration['integration_tests']:
            self.build_print(
                'Running Integration Test: ' + integration_test['audio'])
            # Render the audio
            test_input_path = integration_test['file']
            test_offset = str(integration_test['offset'])
            is_http = test_input_path.startswith('http')
            is_midi = test_input_path.startswith('midi')
            if (not is_http) and (not is_midi):
                test_input_path = os.path.abspath(test_input_path)
            cli_binary_call = [
                cli_binary,
                test_input_path,
                os.path.abspath(nfdecoder_wav),
                test_offset]
            if 'duration' in integration_test:
                cli_binary_call.append(str(integration_test['duration']))
            cli_result = subprocess.call(cli_binary_call)
            if cli_result:
                sys.exit(cli_result)
            # Convert and prepare the input/output audio files
            audio_file = os.path.join(
                os.path.join(
                    self.nfdecoder_test_audio_extraction_folder,
                    'integration-test-audio'),
                integration_test['audio'])
            if os.path.isfile(integration_test_output_file):
                os.remove(integration_test_output_file)
            ffmpeg_result = subprocess.call([
                self.ffmpeg_binary,
                '-hide_banner',
                '-loglevel',
                loglevel,
                '-i',
                nfdecoder_wav,
                '-c:a',
                'flac',
                integration_test_output_file])
            if ffmpeg_result:
                self.build_print("Failed to convert render to flac")
                sys.exit(ffmpeg_result)
            if os.path.isfile(input_wav):
                os.remove(input_wav)
            ffmpeg_result = subprocess.call([
                self.ffmpeg_binary,
                '-hide_banner',
                '-loglevel',
                loglevel,
                '-i',
                integration_test_output_file,
                input_wav])
            if ffmpeg_result:
                self.build_print("Failed to convert render flac to wav")
                sys.exit(ffmpeg_result)
            if os.path.isfile(output_wav):
                os.remove(output_wav)
            ffmpeg_result = subprocess.call([
                self.ffmpeg_binary,
                '-hide_banner',
                '-loglevel',
                loglevel,
                '-i',
                audio_file,
                output_wav])
            if ffmpeg_result:
                self.build_print("Failed to convert recorded flac to wav")
                sys.exit(ffmpeg_result)
            # Check that the output is the same audio
            input_y, _ = sf.read(input_wav)
            input_y = (input_y[:,0] + input_y[:,1]) / 2
            output_y, _ = sf.read(output_wav)
            output_y = (output_y[:,0] + output_y[:,1]) / 2
            input_z = numpy.absolute(input_y.flatten())
            output_z = numpy.absolute(output_y.flatten())
            if abs(input_z.size - output_z.size) > 0:
                self.build_print(
                    "ERROR: Integration Test Fails on Length Check: " +
                    integration_test['audio'])
                self.build_print(
                    "Actual render length is: " +
                    str(input_z.size))
                self.build_print(
                    "Expected render length is: " +
                    str(output_z.size))
                sys.exit(1)
            input_z = numpy.resize(input_z, max(input_z.size, output_z.size))
            output_z = numpy.resize(output_z, max(input_z.size, output_z.size))
            audio_error = numpy.mean(
                numpy.absolute(
                    numpy.subtract(input_z, output_z)))
            if audio_error > integration_test['error']:
                self.build_print(
                    "ERROR: Integration Test Fails: " +
                    integration_test['audio'])
                self.build_print("Error rate: " + str(audio_error))
                sys.exit(1)
            else:
                self.build_print(
                    "Integration Test Passed: " + integration_test['audio'])

    def collectCodeCoverage(self):
        for root, dirnames, filenames in os.walk('build'):
            for filename in fnmatch.filter(filenames, '*.gcda'):
                full_filepath = os.path.join(root, filename)
                if full_filepath.startswith('build/source/') and \
                   '/tests/' not in full_filepath:
                    continue
                os.remove(full_filepath)
        llvm_run_script = os.path.join(
            os.path.join(
                self.current_working_directory,
                'ci'),
            'llvm-run.sh')
        cov_info = os.path.join('build', 'cov.info')
        lcov_result = subprocess.call([
            self.lcov_binary,
            '--directory',
            '.',
            '--base-directory',
            '.',
            '--gcov-tool',
            llvm_run_script,
            '--capture',
            '-o',
            cov_info])
        if lcov_result:
            sys.exit(lcov_result)
        coverage_output = os.path.join(self.output_directory, 'code_coverage')
        genhtml_result = subprocess.call([
            self.genhtml_binary,
            cov_info,
            '-o',
            coverage_output])
        if genhtml_result:
            sys.exit(genhtml_result)
