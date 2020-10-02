cmake_minimum_required(VERSION 3.5)

project(NFDecoder)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_FLAGS
  "${CMAKE_CXX_FLAGS} -Wno-deprecated-declarations")
set(CMAKE_CXX_FLAGS
  "${CMAKE_CXX_FLAGS} -Wno-unused-local-typedefs")
if(LLVM_STDLIB)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libc++ \
  -Wno-tautological-undefined-compare -Wno-shorten-64-to-32")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
else()
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=gnu++11")
endif()
set(CMAKE_MODULE_PATH
  ${CMAKE_MODULE_PATH}
  ${CMAKE_CURRENT_SOURCE_DIR}/cmake-modules)

if(DEFINED  CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE ${CMAKE_BUILD_TYPE} CACHE STRING
    "Choose the type of build: None, Debug, or Release"
    FORCE)
else()
  set(CMAKE_BUILD_TYPE Debug CACHE STRING
    "Choose the type of build: None, Debug, or Release"
    FORCE)
endif()

if(INCLUDE_LGPL)
  add_definitions(-DINCLUDE_LGPL=1)
  message("Including LGPL code")
endif()

if(USE_UB_SANITIZER)
  message("Using Undefined Behavior Sanitizer")
  execute_process(COMMAND python
    tools/generate-ubsan-blacklist.py "${CMAKE_CURRENT_SOURCE_DIR}/tools"
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")
  set(UB_SAN_BLACKLIST "${CMAKE_CURRENT_SOURCE_DIR}/tools/ubsan.blacklist")
  execute_process(COMMAND cat "${UB_SAN_BLACKLIST}"
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")
  set(UB_SAN_FLAGS " -g -fsanitize=undefined -fno-omit-frame-pointer")
  set(UB_SAN_FLAGS "${UB_SAN_FLAGS} -fno-sanitize-recover=all")
  set(UB_SAN_FLAGS "${UB_SAN_FLAGS} -fsanitize-blacklist=${UB_SAN_BLACKLIST}")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${UB_SAN_FLAGS}")
  set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${UB_SAN_FLAGS}")
endif()

if(USE_MEM_SANITIZER AND ${CMAKE_SYSTEM_NAME} MATCHES "Linux")
  # This isn't really helpful since we have too many
  # dependencies that aren't compiled from source
  message("Using Memory Sanitizer")
  set(MEM_SAN_FLAGS " -g -fsanitize=memory -fno-omit-frame-pointer")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${MEM_SAN_FLAGS}")
  set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${MEM_SAN_FLAGS}")
  set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -lc++abi")
endif()

if(USE_ADDRESS_SANITIZER)
  message("Using Address & Leak Sanitizer")
  set(
    CMAKE_CXX_FLAGS
    "${CMAKE_CXX_FLAGS} -fsanitize=address -g -fno-omit-frame-pointer")
  set(
    CMAKE_EXE_LINKER_FLAGS
    "${CMAKE_EXE_LINKER_FLAGS} -fsanitize=address -g -fno-omit-frame-pointer")
endif()

if(CODE_COVERAGE)
  message("Using Code Coverage")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fprofile-arcs -ftest-coverage")
  set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} --coverage")
endif()

if(IOS)
  message("Building a project for iOS")
  set(CMAKE_XCODE_ATTRIBUTE_SDKROOT iphoneos)
  set(CMAKE_XCODE_SUPPORTED_PLATFORMS "iphonesimulator iphoneos")
endif()

execute_process(COMMAND python
  "${CMAKE_CURRENT_SOURCE_DIR}/tools/generate-version.py"
  ${CMAKE_BINARY_DIR}/output
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})

add_subdirectory(libraries)
add_subdirectory(source)
