cmake_minimum_required(VERSION 2.8.12)

project(videocaptest)

if(WIN32)
	SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /source-charset:utf-8")
	SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /source-charset:utf-8")
endif()

set(header_files

) 
set(source_files
	main.cpp
)


if(MSVC)
        if(NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
                set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /SAFESEH:NO")
                set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} /SAFESEH:NO")
                set(CMAKE_MODULE_LINKER_FLAGS "${CMAKE_MODULE_LINKER_FLAGS} /SAFESEH:NO")
        endif()
	set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /NODEFAULTLIB:msvcrt")
endif()

	
if (APPLE)
	ADD_EXECUTABLE(videocaptest ${source_files} ${header_files} ${qt_UI_HEADERS} ${qt_QRC_SOURCES})
	set_target_properties(videocaptest PROPERTIES MACOSX_BUNDLE TRUE MACOSX_BUNDLE_INFO_PLIST ${CMAKE_CURRENT_SOURCE_DIR}/Info.plist)
elseif(WIN32)
	#ADD_EXECUTABLE(videocaptest WIN32 ${source_files} ${header_files} ${qt_UI_HEADERS} ${qt_QRC_SOURCES} dbxt.rc)
	ADD_EXECUTABLE(videocaptest WIN32 ${source_files} ${header_files} ${qt_UI_HEADERS} ${qt_QRC_SOURCES})
endif()
message("in rtclient:${LINK_LIBS}")

target_link_libraries(videocaptest
	webrtc
	${LINK_LIBS}
)
