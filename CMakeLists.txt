cmake_minimum_required(VERSION 3.5)
project(webRTClearn)

add_compile_options(-std=c++14)
add_definitions(-DWEBRTC_POSIX)
add_definitions(-DWEBRTC_MAC)
# -fno-rtti 禁止运行时类型信息 不加会出现某些包含纯虚函数的类 undefine reference 暂时也没搞懂原因
add_compile_options(-fno-rtti)

include_directories("/Users/liuye/google/webrtc-checkout/src")
include_directories("/Users/liuye/google/webrtc-checkout/src/third_party/abseil-cpp")
include_directories("/Users/liuye/mycode/testrep/tools/readavfile")
link_directories("/Users/liuye/google/webrtc-checkout/src/out/Default/obj")
link_directories("/Users/liuye/google/webrtc-checkout/src/out/Default/obj/rtc_base")
link_directories("/Users/liuye/mycode/testrep/tools/readavfile/build")

set(CMAKE_EXE_LINKER_FLAGS
                "-framework Foundation -framework AudioToolbox -framework VideoToolbox -framework CoreGraphics -framework QuartzCore -framework CoreFoundation -framework CoreMedia -framework Security")
set(SOURCE_FILES main.cpp)
add_executable(testcx ${SOURCE_FILES})
target_link_libraries(testcx webrtc pthread)


add_executable(testrtprtcp testrtprtcp.cpp)
target_link_libraries(testrtprtcp webrtc rtc_base simreader pthread)

add_executable(threadtest threadtest.cpp)
target_link_libraries(threadtest webrtc rtc_base pthread)
