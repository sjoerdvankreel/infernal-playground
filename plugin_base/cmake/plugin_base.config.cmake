cmake_policy(SET CMP0091 NEW)

if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
add_compile_options(/W4;/WX;/external:W0;/wd4245;/wd4267;/wd4244;/wd4100;/wd26495;/wd4389;/fp:fast;/arch:AVX)
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
add_compile_options(-Wl,--no-undefined -fvisibility=hidden -fvisibility-inlines-hidden)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wl,--no-undefined -fvisibility=hidden -fvisibility-inlines-hidden")
else()
message(FATAL_ERROR)
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CONFIGURATION_TYPES "Debug;Release")