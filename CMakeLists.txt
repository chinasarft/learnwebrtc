cmake_minimum_required(VERSION 3.5)
project(webRTClearn)

if (WEBRTC_LIB_PATH)
    link_directories("${WEBRTC_LIB_PATH}/obj")
    link_directories("${WEBRTC_LIB_PATH}/obj/rtc_base")
#link_directories("${WEBRTC_LIB_PATH}/obj/testing/gtest")
#link_directories("${WEBRTC_LIB_PATH}/obj/third_party/googletest")
else()
    message(FATAL_ERROR "should specify WEBRTC_LIB_PATH")
endif()

if (APPLE)
    # -fno-rtti 禁止运行时类型信息 不加会出现某些包含纯虚函数的类 undefine reference 暂时也没搞懂原因
    add_compile_options(-fno-rtti)

    add_definitions(-DWEBRTC_POSIX)
    add_definitions(-DWEBRTC_MAC)
elseif(WIN32)
    add_definitions(-DWEBRTC_WIN)
    add_definitions(-DWIN32_LEAN_AND_MEAN)
    add_definitions(-DNOMINMAX)
    add_definitions(-DWIN32)
    message("-${CMAKE_CXX_FLAGS_RELEASE}-${CMAKE_CXX_FLAGS_DEBUG}")
    string(REPLACE "/MD" "/MT" CMAKE_CXX_FLAGS_RELEASE ${CMAKE_CXX_FLAGS_RELEASE})
    string(REPLACE "/MDd" "/MTd" CMAKE_CXX_FLAGS_DEBUG ${CMAKE_CXX_FLAGS_DEBUG})
	string(REPLACE "/MD" "/MT" CMAKE_C_FLAGS_RELEASE ${CMAKE_CXX_FLAGS_RELEASE})
    string(REPLACE "/MDd" "/MTd" CMAKE_C_FLAGS_DEBUG ${CMAKE_CXX_FLAGS_DEBUG})
    message("-${CMAKE_CXX_FLAGS_RELEASE}-${CMAKE_CXX_FLAGS_DEBUG}")
	message("-${CMAKE_C_FLAGS_RELEASE}-${CMAKE_C_FLAGS_DEBUG}")
	add_definitions(-D_ITERATOR_DEBUG_LEVEL=0)
endif()

if (WEBRTC_INC_PATH)
    include_directories("${WEBRTC_INC_PATH}")
    include_directories("${WEBRTC_INC_PATH}/third_party/abseil-cpp")
    #include_directories("${WEBRTC_INC_PATH}/third_party/jsoncpp/source/include")
    include_directories("${WEBRTC_INC_PATH}/third_party/googletest/src/googletest/include/")
    include_directories("${WEBRTC_INC_PATH}/third_party/googletest/src/googlemock/include/")
else()
    message(FATAL_ERROR "should specify WEBRTC_INC_PATH")
endif()
#include_directories("/Users/liuye/google/webrtc-checkout/src")
#include_directories("/Users/liuye/google/webrtc-checkout/src/third_party/abseil-cpp")
#include_directories("/Users/liuye/mycode/testrep/tools/readavfile")
#link_directories("/Users/liuye/google/webrtc-checkout/src/out/Default/obj")
#link_directories("/Users/liuye/google/webrtc-checkout/src/out/Default/obj/rtc_base")
#link_directories("/Users/liuye/mycode/testrep/tools/readavfile/build")

if (APPLE)
    set(CMAKE_EXE_LINKER_FLAGS
                "-framework Foundation -framework AudioToolbox -framework VideoToolbox -framework CoreGraphics -framework QuartzCore -framework CoreFoundation -framework CoreMedia -framework Security")
elseif(WIN32)
    set(LINK_LIBS
	advapi32
        comdlg32
        dbghelp
        dnsapi
        gdi32
        msimg32
        odbc32
        odbccp32
        oleaut32
        psapi
        shell32
        shlwapi
        user32
        usp10
        uuid
        version
        wininet
        winmm
        winspool
        ws2_32
        delayimp
        kernel32
        ole32
        crypt32
        iphlpapi
        secur32
        dmoguids
        wmcodecdspuuid
        amstrmid
        msdmo
        strmiids
	)
endif()

add_subdirectory(mypeerclient)

set(SOURCE_FILES rtp_rtcp_impl_unittest.cc
	"${WEBRTC_LIB_PATH}/../../test/rtcp_packet_parser.cc"
	)
add_executable(testunit ${SOURCE_FILES})
add_executable(testrtprtcp testrtprtcp.cpp)
add_executable(threadtest threadtest.cpp)

if (APPLE)
    target_link_libraries(testunit webrtc pthread)
    target_link_libraries(testrtprtcp webrtc rtc_base simreader pthread)
    target_link_libraries(threadtest webrtc rtc_base pthread)
elseif(WIN32)
    target_link_libraries(testunit webrtc rtc_base ${LINK_LIBS})
    target_link_libraries(testrtprtcp webrtc rtc_base simreader ${LINK_LIBS})
    target_link_libraries(threadtest webrtc rtc_base ${LINK_LIBS})
endif()
