cmake_minimum_required(VERSION 3.23)
project(bank_app)

set(CMAKE_CXX_STANDARD 20)

set(SOURCE_FILES main.cpp)
#set(CMAKE_EXE_LINKER_FLAGS "-static")
#set(CMAKE_CXX_FLAGS "-fpermissive")

include_directories(${PROJECT_SOURCE_DIR}/boost)
include_directories(${PROJECT_SOURCE_DIR}/lexbor/source)

if(WIN32)
    include_directories(${PROJECT_SOURCE_DIR}/zeroc.openssl/include)
else()
    include_directories(${PROJECT_SOURCE_DIR}/openssl/include)
endif()

add_executable( ${PROJECT_NAME} ${SOURCE_FILES} )

if(WIN32)
    target_compile_options(${PROJECT_NAME} PUBLIC /bigobj)

    target_link_libraries(${PROJECT_NAME} PUBLIC wsock32)
    target_link_libraries(${PROJECT_NAME} PUBLIC ws2_32)
    target_link_libraries(${PROJECT_NAME} PUBLIC crypt32)
    target_link_libraries(${PROJECT_NAME} PUBLIC bcrypt)
    target_link_libraries(${PROJECT_NAME} PUBLIC ${PROJECT_SOURCE_DIR}/lexbor/build/Release/lexbor.lib)
    target_link_libraries(${PROJECT_NAME} PUBLIC ${PROJECT_SOURCE_DIR}/lexbor/build/Release/lexbor_static.lib)
    target_link_libraries(${PROJECT_NAME} PUBLIC ${PROJECT_SOURCE_DIR}/zeroc.openssl/lib/x64/Release/libcrypto.lib)
    target_link_libraries(${PROJECT_NAME} PUBLIC ${PROJECT_SOURCE_DIR}/zeroc.openssl/lib/x64/Release/libssl.lib)

    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${PROJECT_SOURCE_DIR}/lexbor/build/Release/lexbor.dll"
            $<TARGET_FILE_DIR:${PROJECT_NAME}>
            )

    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${PROJECT_SOURCE_DIR}/zeroc.openssl/bin/x64/Release/libssl-1_1-x64.dll"
            $<TARGET_FILE_DIR:${PROJECT_NAME}>
            )
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${PROJECT_SOURCE_DIR}/zeroc.openssl/bin/x64/Release/libcrypto-1_1-x64.dll"
            $<TARGET_FILE_DIR:${PROJECT_NAME}>
            )
else()
	set(OPENSSL_USE_STATIC_LIBS TRUE)
	find_package(OpenSSL REQUIRED)
	target_link_libraries(${PROJECT_NAME} PRIVATE OpenSSL::applink OpenSSL::SSL OpenSSL::Crypto)

    target_link_libraries(${PROJECT_NAME} PUBLIC ${PROJECT_SOURCE_DIR}/lexbor/liblexbor_static.a)
    target_link_libraries(${PROJECT_NAME} PUBLIC ${PROJECT_SOURCE_DIR}/openssl/libssl.a)
    target_link_libraries(${PROJECT_NAME} PUBLIC ${PROJECT_SOURCE_DIR}/openssl/libcrypto.a)
endif()