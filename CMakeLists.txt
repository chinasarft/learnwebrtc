cmake_minimum_required(VERSION 3.5)
project(webRTClearn)

add_compile_options(-std=c++14)
add_definitions(-DWEBRTC_POSIX)
add_definitions(-DWEBRTC_MAC)
# -fno-rtti 禁止运行时类型信息 不加会出现某些包含纯虚函数的类 undefine reference 暂时也没搞懂原因
add_compile_options(-fno-rtti)

include_directories("/Users/liuye/google/webrtc-checkout/src")
include_directories("/Users/liuye/google/webrtc-checkout/src/third_party/abseil-cpp")
link_directories("/Users/liuye/google/webrtc-checkout/src/out/Default/obj")

set(CMAKE_EXE_LINKER_FLAGS
                "-framework Foundation -framework AudioToolbox -framework VideoToolbox -framework CoreGraphics -framework QuartzCore -framework CoreFoundation -framework CoreMedia -framework Security")
set(SOURCE_FILES main.cpp)
add_executable(testcx ${SOURCE_FILES})
target_link_libraries(testcx webrtc pthread)
