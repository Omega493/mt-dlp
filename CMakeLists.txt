cmake_minimum_required (VERSION 4.3)

project("mt-dlp")

add_executable(mt-dlp "include/panic_handler/panic_handler.cpp"
  "src/main.cpp" "src/core/downloader/downloader.cpp" "src/core/ui/ui.cpp"
)

# C/C++ Standards
set_property(TARGET mt-dlp PROPERTY C_STANDARD 23)
set_property(TARGET mt-dlp PROPERTY CXX_STANDARD 26)

set_property(TARGET mt-dlp PROPERTY C_STANDARD_REQUIRED ON)
set_property(TARGET mt-dlp PROPERTY C_EXTENSIONS OFF)

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
  set_property(TARGET mt-dlp PROPERTY CXX_STANDARD_REQUIRED ON)
  set_property(TARGET mt-dlp PROPERTY CXX_EXTENSIONS ON)
else()
  set_property(TARGET mt-dlp PROPERTY CXX_STANDARD_REQUIRED OFF)
  set_property(TARGET mt-dlp PROPERTY CXX_EXTENSIONS OFF)
endif()

if(MSVC)
  include(CheckCXXCompilerFlag)
  
  check_cxx_compiler_flag("/arch:AVX512" COMPILER_SUPPORTS_AVX512)
  check_cxx_compiler_flag("/arch:AVX2" COMPILER_SUPPORTS_AVX2)
  check_cxx_compiler_flag("/arch:AVX" COMPILER_SUPPORTS_AVX)

  if(COMPILER_SUPPORTS_AVX512)
    set(MSVC_ARCH_FLAG "/arch:AVX512")
  elseif(COMPILER_SUPPORTS_AVX2)
    set(MSVC_ARCH_FLAG "/arch:AVX2")
  elseif(COMPILER_SUPPORTS_AVX)
    set(MSVC_ARCH_FLAG "/arch:AVX")
  endif()

  if(MSVC_ARCH_FLAG)
    target_compile_options(mt-dlp PRIVATE 
      "$<$<AND:$<CONFIG:Release>,$<COMPILE_LANGUAGE:C,CXX>>:${MSVC_ARCH_FLAG}>"
    )
    message(STATUS "cl/clang-cl: Optimizer flag ${MSVC_ARCH_FLAG} will be applied to C/CXX in Release mode.")
  else()
    message(STATUS "cl/clang-cl: Using default baseline architecture")
  endif()
endif()

# curl
find_package(CURL REQUIRED)
target_link_libraries(mt-dlp PRIVATE CURL::libcurl)

# FTXUI
find_package(ftxui CONFIG REQUIRED)
target_link_libraries(mt-dlp PRIVATE ftxui::dom ftxui::screen ftxui::component)

target_include_directories(mt-dlp PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}/include
  ${CMAKE_CURRENT_SOURCE_DIR}/src
  ${CMAKE_CURRENT_SOURCE_DIR}/tests
)
