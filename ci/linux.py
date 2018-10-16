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

import os
import sys

from nfbuildlinux import NFBuildLinux
from build_options import BuildOptions


def main():
    buildOptions = BuildOptions()
    buildOptions.addOption("debug", "Enable Debug Mode")
    buildOptions.addOption("lintCmake", "Lint cmake files")
    buildOptions.addOption("makeBuildDirectory",
                           "Wipe existing build directory")
    buildOptions.addOption("generateProject", "Regenerate xcode project")
    buildOptions.addOption("buildTargetLibrary", "Build Target: Library")
    buildOptions.addOption("gnuToolchain", "Build with gcc and libstdc++")
    buildOptions.addOption("llvmToolchain", "Build with clang and libc++")
    buildOptions.addOption("integrationTests", "Run Integration Tests")
    buildOptions.addOption("addressSanitizer",
                           "Enable Address Sanitizer in generate project")
    # buildOptions.addOption("packageArtifacts", "Package the Artifacts")
    buildOptions.addOption("ubSanitizer",
                           "Enable UB Sanitizer in generate project")
    buildOptions.addOption("memSanitizer",
                           "Enable Mem Sanitizer in generate project")

    buildOptions.addOption("printStackTrace",
                           "Enable print stack trace")

    buildOptions.addOption("buildTargetCLI", "Build Target: CLI")
    buildOptions.addOption("buildTargetLibrary", "Build Target: Library")

    buildOptions.setDefaultWorkflow("Empty workflow", [])

    # TODO: Check if we need package artifacts
    buildOptions.addWorkflow("clang_build", "Production Clang Build", [
        'llvmToolchain',
        'lintCmake',
        'makeBuildDirectory',
        'generateProject',
        'buildTargetCLI',
        'buildTargetLibrary',
        'integrationTests'
    ])

    buildOptions.addWorkflow("gcc_build", "Production build with gcc", [
        'gnuToolchain',
        'lintCmake',
        'makeBuildDirectory',
        'generateProject',
        'buildTargetCLI',
        'buildTargetLibrary',
        'integrationTests',
    ])

    buildOptions.addWorkflow("local_it", "Run local integration tests", [
        'debug',
        'lintCmake',
        'integrationTests'
    ])

    buildOptions.addWorkflow("ub_sanitizer", "Run UB sanitizer", [
        'printStackTrace',
        'lintCmake',
        'makeBuildDirectory',
        'generateProject',
        'ubSanitizer',
        'buildTargetCLI',
        'integrationTests'
    ])

    buildOptions.addWorkflow("mem_sanitizer", "Run Mem sanitizer", [
        'lintCmake',
        'makeBuildDirectory',
        'generateProject',
        'memSanitizer',
        'buildTargetCLI',
        'integrationTests'
    ])

    buildOptions.addWorkflow("address_sanitizer", "Run address sanitizer", [
        'lintCmake',
        'makeBuildDirectory',
        'generateProject',
        'addressSanitizer',
        'buildTargetCLI',
        'integrationTests'
    ])

    buildOptions.addWorkflow("build", "Production Build", [
        'lintCmake',
        'makeBuildDirectory',
        'generateProject',
        'buildTargetLibrary',
        'buildTargetCLI'
    ])

    options = buildOptions.parseArgs()
    buildOptions.verbosePrintBuildOptions(options)

    library_target = 'NFDecoder'
    cli_target = 'NFDecoderCLI'
    nfbuild = NFBuildLinux()

    if buildOptions.checkOption(options, 'debug'):
        nfbuild.build_type = 'Debug'

    if buildOptions.checkOption(options, 'lintCmake'):
        nfbuild.lintCmake()

    if buildOptions.checkOption(options, 'makeBuildDirectory'):
        nfbuild.makeBuildDirectory()

    if buildOptions.checkOption(options, 'printStackTrace'):
        os.environ['UBSAN_OPTIONS=print_stacktrace'] = '1'

    if buildOptions.checkOption(options, 'generateProject'):
        if buildOptions.checkOption(options, 'gnuToolchain'):
            os.environ['CC'] = 'gcc'
            os.environ['CXX'] = 'g++'
            nfbuild.generateProject(address_sanitizer='addressSanitizer' in options,
                                    ub_sanitizer='ubSanitizer' in options,
                                    mem_sanitizer='memSanitizer' in options,
                                    gcc=True)
        elif buildOptions.checkOption(options, 'llvmToolchain'):
            os.environ['CC'] = 'clang'
            os.environ['CXX'] = 'clang++'
            nfbuild.generateProject(address_sanitizer='addressSanitizer' in options,
                                    ub_sanitizer='ubSanitizer' in options,
                                    mem_sanitizer='memSanitizer' in options,
                                    gcc=False)
        else:
            nfbuild.generateProject(address_sanitizer='addressSanitizer' in options,
                                    ub_sanitizer='ubSanitizer' in options,
                                    mem_sanitizer='memSanitizer' in options)

    if buildOptions.checkOption(options, 'buildTargetLibrary'):
        nfbuild.buildTarget(library_target)

    if buildOptions.checkOption(options, 'buildTargetCLI'):
        nfbuild.buildTarget(cli_target)

    if buildOptions.checkOption(options, 'integrationTests'):
        nfbuild.runIntegrationTests()

    # if buildOptions.checkOption(options, 'packageArtifacts'):
    #     nfbuild.packageArtifacts()

if __name__ == "__main__":
    main()
