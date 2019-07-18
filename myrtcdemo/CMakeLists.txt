cmake_minimum_required(VERSION 2.8.12)

project(myrtcdemo)

if (QTDIR OR DEFINED ENV{QTDIR})
	# Qt path set by user or env var
	list(APPEND CMAKE_PREFIX_PATH "${QTDIR}")
else()
	message(WARNING "QTDIR variable is missing.  Please set this variable")
endif()

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${QTDIR}/lib/cmake/" "${QTDIR}/lib/cmake/Qt5Widgets")
message("${CMAKE_MODULE_PATH}")

find_package(Qt5Widgets REQUIRED)

if(NOT Qt5Widgets_FOUND)
	message(FATAL_ERROR "Failed to find Qt5")
endif()
set(CMAKE_AUTOMOC ON)
include_directories("${Qt5Widgets_INCLUDE_DIRS}")
message("qtinc:${Qt5Widgets_INCLUDE_DIRS}")

if(WIN32)
	SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /source-charset:utf-8")
	SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /source-charset:utf-8")
endif()
link_directories("${QTDIR}/lib")
message("=-${CMAKE_CXX_FLAGS_RELEASE}-${CMAKE_CXX_FLAGS_DEBUG}")
message("=-${CMAKE_C_FLAGS_RELEASE}-${CMAKE_C_FLAGS_DEBUG}")
set(CMAKE_INCLUDE_CURRENT_DIR TRUE)

set(CMAKE_AUTOMOC TRUE)

set(header_files
	mainwindow.h
	defaults.h
	conductor.h
	fixjson.h
	peer_connection_client.h
	${WEBRTC_INC_PATH}/test/vcm_capturer.h
	${WEBRTC_INC_PATH}/test/test_video_capturer.h
)

set(source_files
	defaults.cpp
	mainwindow.cpp
	main.cpp
	conductor.cpp
	fixjson.cpp
	peer_connection_client.cpp
	${WEBRTC_INC_PATH}/test/vcm_capturer.cc
	${WEBRTC_INC_PATH}/test/test_video_capturer.cc
)

set(qt_UI
	mainwindow.ui
)


qt5_wrap_ui(qt_UI_HEADERS ${qt_UI})

#set(qt_QRC rtc.qrc)
#qt5_add_resources(qt_QRC_SOURCES ${qt_QRC})

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
	ADD_EXECUTABLE(myrtcdemo ${source_files} ${header_files} ${qt_UI_HEADERS} ${qt_QRC_SOURCES})
elseif(WIN32)
	#ADD_EXECUTABLE(myrtcdemo WIN32 ${source_files} ${header_files} ${qt_UI_HEADERS} ${qt_QRC_SOURCES} dbxt.rc)
	ADD_EXECUTABLE(myrtcdemo WIN32 ${source_files} ${header_files} ${qt_UI_HEADERS} ${qt_QRC_SOURCES})
endif()
message("in rtclient:${LINK_LIBS}")

target_link_libraries(myrtcdemo
	Qt5::Widgets
	webrtc
	rtc_base
	${LINK_LIBS}
)


#set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT dbt)
