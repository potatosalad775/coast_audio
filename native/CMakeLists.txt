cmake_minimum_required(VERSION 3.22.0)
project(coast_audio)

set(HEADER_FILES "src/coast_audio.h" "src/ca_dart.h" "src/ca_context.h" "src/ca_device.h" "src/ca_log.h" "src/dart_types.h" "miniaudio/extras/miniaudio_split/miniaudio.h")
set(SOURCE_FILES "src/coast_audio.c" "src/ca_dart.c" "src/ca_context.c" "src/ca_device.c" "src/ca_log.c" "miniaudio/extras/miniaudio_split/miniaudio.c")

if ("${OS}" STREQUAL "APPLE")
  set(CMAKE_C_FLAGS "-x objective-c")
endif()

# Always build as shared library for Windows
if ("${OS}" STREQUAL "WIN32")
  set(BUILD_SHARED_LIBS ON)
  add_library(${PROJECT_NAME} SHARED ${SOURCE_FILES})
  set_target_properties(${PROJECT_NAME} PROPERTIES WINDOWS_EXPORT_ALL_SYMBOLS ON)
else()
  if ("${SHARED}" STREQUAL "YES")
    add_library(${PROJECT_NAME} SHARED ${SOURCE_FILES})
  else()
    add_library(${PROJECT_NAME} STATIC ${SOURCE_FILES})
  endif()
endif()

target_include_directories(${PROJECT_NAME} PRIVATE "miniaudio/extras/miniaudio_split")
set_target_properties(${PROJECT_NAME} PROPERTIES LINKER_LANGUAGE CXX)

if ("${OS}" STREQUAL "APPLE")
  find_library(CORE_FOUNDATION CoreFoundation)
  find_library(AVFOUNDATION AVFoundation)
  find_library(AUDIO_TOOLBOX AudioToolbox)
  target_link_libraries(${PROJECT_NAME} PRIVATE ${CORE_FOUNDATION} ${AVFOUNDATION} ${AUDIO_TOOLBOX})

  set_target_properties(${PROJECT_NAME} PROPERTIES
    FRAMEWORK TRUE
    MACOSX_FRAMEWORK_IDENTIFIER "work.kscafe.coast_audio"
  )

  install (TARGETS ${PROJECT_NAME} FRAMEWORK DESTINATION "prebuilt/apple/${PLATFORM}")
  install(FILES ${HEADER_FILES} DESTINATION "prebuilt/apple/${PLATFORM}/coast_audio.framework/Headers")
  install(FILES "prebuilt/apple/module.modulemap" DESTINATION "prebuilt/apple/${PLATFORM}/coast_audio.framework/Modules")
elseif ("${OS}" STREQUAL "ANDROID")
  install (TARGETS ${PROJECT_NAME}
    LIBRARY DESTINATION .
  )
elseif ("${OS}" STREQUAL "LINUX")
  install (TARGETS ${PROJECT_NAME}
    LIBRARY DESTINATION .
  )
elseif ("${OS}" STREQUAL "WIN32")
  # Set C++ standard
  set(CMAKE_CXX_STANDARD 11)
  set(CMAKE_CXX_STANDARD_REQUIRED ON)

  # Install rules
  install(TARGETS ${PROJECT_NAME}
    RUNTIME DESTINATION bin  # This is for the .dll file
    LIBRARY DESTINATION lib  # This is for .so files on Unix-like systems
    ARCHIVE DESTINATION lib  # This is for the .lib file on Windows
  )
endif()
