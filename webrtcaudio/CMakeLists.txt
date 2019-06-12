cmake_minimum_required(VERSION 2.8.12)

project(audio_engine_test CXX)


if(WIN32)
	SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /source-charset:utf-8")
	SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /source-charset:utf-8")
	message("=-${CMAKE_CXX_FLAGS_RELEASE}-${CMAKE_CXX_FLAGS_DEBUG}")
	message("=-${CMAKE_C_FLAGS_RELEASE}-${CMAKE_C_FLAGS_DEBUG}")
endif()
set(CMAKE_INCLUDE_CURRENT_DIR TRUE)


set(source_files
	WebRtcAudioTest.cpp
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
	ADD_EXECUTABLE(audiotest ${source_files})
elseif(WIN32)
	ADD_EXECUTABLE(audiotest WIN32 ${source_files})
endif()

#target_link_libraries(audiotest audio_processing )
target_link_libraries(audiotest webrtc)
