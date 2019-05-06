cmake_minimum_required(VERSION 2.8.12)

project(rtprtcp)

if(WIN32)
	SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /source-charset:utf-8")
	SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /source-charset:utf-8")
endif()

set(header_files
	myrtprtcp.h
)

set(source_files
	myrtprtcp.cpp
)

if(MSVC)
        set(CMAKE_C_FLAGS_DEBUG "/D_CRT_SECURE_NO_WARNINGS /DDEBUG=1 /D_DEBUG=1 ${CMAKE_C_FLAGS_DEBUG}")
        set(CMAKE_CXX_FLAGS_DEBUG "/D_CRT_SECURE_NO_WARNINGS /DDEBUG=1 /D_DEBUG=1 ${CMAKE_C_FLAGS_DEBUG}")

        if(NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
                set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /SAFESEH:NO")
                set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} /SAFESEH:NO")
                set(CMAKE_MODULE_LINKER_FLAGS "${CMAKE_MODULE_LINKER_FLAGS} /SAFESEH:NO")
        endif()
	set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /NODEFAULTLIB:msvcrt")
endif()

	
if (APPLE)
	ADD_EXECUTABLE(rtptest ${source_files} ${header_files} ${qt_UI_HEADERS} ${qt_QRC_SOURCES})
elseif(WIN32)
	#	ADD_EXECUTABLE(rtptest WIN32 ${source_files} ${header_files} ${qt_UI_HEADERS} ${qt_QRC_SOURCES})
	ADD_EXECUTABLE(rtptest ${source_files} ${header_files} ${qt_UI_HEADERS} ${qt_QRC_SOURCES})
endif()

target_link_libraries(rtptest
#	rtp_rtcp
#	rtc_base
        webrtc
	${LINK_LIBS}
)
