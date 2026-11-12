cmake_minimum_required(VERSION 3.10)

if(NOT DEFINED CMAKE_TOOLCHAIN_FILE)
    if(DEFINED ENV{VITASDK})
        set(CMAKE_TOOLCHAIN_FILE "$ENV{VITASDK}/share/vita.toolchain.cmake" CACHE PATH "toolchain file")
    else()
        message(FATAL_ERROR "Please define VITASDK to point to your VitaSDK path.")
    endif()
endif()

project(VitaGram LANGUAGES CXX C)

include("${VITASDK}/share/vita.cmake" REQUIRED)

set(VITA_APP_NAME "VitaGram")
set(VITA_TITLEID  "VGRAM0001")
set(VITA_VERSION  "01.00")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -O2")

add_executable(${PROJECT_NAME}
    src/main.cpp
    src/json.cpp
    src/net_client.cpp
    src/api_client.cpp
    src/texture_cache.cpp
    src/osk.cpp
    src/ui.cpp
)

target_link_libraries(${PROJECT_NAME}
    vita2d
    freetype
    png
    jpeg
    z
    m
    SceAppMgr_stub
    SceSharedFb_stub
    ScePgf_stub
    ScePvf_stub
    SceGxm_stub
    SceDisplay_stub
    SceCtrl_stub
    SceTouch_stub
    SceCommonDialog_stub
    SceSysmodule_stub
    SceNet_stub
    SceNetCtl_stub
    SceHttp_stub
)

vita_create_self(${PROJECT_NAME}.self ${PROJECT_NAME})
vita_create_vpk(${PROJECT_NAME}.vpk ${VITA_TITLEID} ${PROJECT_NAME}.self
    VERSION ${VITA_VERSION}
    NAME "${VITA_APP_NAME}"
    FILE ${CMAKE_CURRENT_SOURCE_DIR}/sce_sys/icon0.png sce_sys/icon0.png
)

