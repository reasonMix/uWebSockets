cmake_minimum_required(VERSION 3.0)

project (uwebsocket)


# compile in release  mode by default
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Choose the type of build, options are: Debug Release RelWithDebInfo MinSizeRel." FORCE)
endif()

option(BUILD_SHARED_LIBS "Build shared library" OFF)

if(UNIX)
  if(APPLE)
    set(PLAT "macosx")
    set(LINK_FLAGS "-bundle -undefined dynamic_lookup -all_load")
  else(APPLE)
    set(PLAT "linux")
    if(BUILD_SHARED_LIBS)
      #set(LINK_FLAGS "-shared -L /usr/local/lib/ -lz")
      set(LINK_FLAGS "-shared -lz")
    endif()
  endif(APPLE)
  add_definitions(-D_GNU_SOURCE)
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -g -Wall -fPIC -march=native -Wno-unused-parameter")
  set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS}  -L /usr/local/lib/ -lz")
else(UNIX)
  if(WIN32)
    set(PLAT "win32")
    add_definitions(-DLUA_BUILD_AS_DLL -DLUA_LIB _D_CRT_SECURE_NO_WARNINGS -D_CRT_NONSTDC_NO_DEPRECATE -D_LARGEFILE_SOURCE -D_FILE_OFFSET_BITS=64 -D_GNU_SOURCE)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /TP")
  else(WIN32)
    set(PLAT "unsupported")
  endif(WIN32)
endif(UNIX)

if (NOT LUA_INCLUDE_DIR OR NOT (BUILD_SHARED_LIBS OR LUA_LIBRARIES))
    message("Using system lua...")
    set(ENV{LUA_DIR} ~/.luaenv/versions/5.1.5) # hints for find lua in luaenv.
    find_package(Lua REQUIRED)
endif()

#add_definitions(-DUSE_EPOLL)
#add_definitions(-DUWS_THREADSAFE)


include_directories(${LUA_INCLUDE_DIR})
include_directories(/usr/local/include)
include_directories(/usr/local/opt/openssl/include)

list(APPEND SOURCES
  src/Epoll.cpp
  src/Extensions.cpp
  src/Group.cpp
  src/HTTPSocket.cpp
  src/Hub.cpp
  src/Networking.cpp
  src/Node.cpp
  src/Socket.cpp
  src/WebSocket.cpp
  src/lua_uwebsocket.cpp
  LuaFunction.cpp
)

if(BUILD_SHARED_LIBS)
  if(APPLE)
  else()
  LINK_LIBRARIES("/usr/local/lib/libz.so")
  endif()
  add_library(uwebsocket MODULE ${SOURCES})
  target_link_libraries(uwebsocket ${LUA_LIBRARIES} ${LIBUV_LIBRARIES} ${OPENSSL_LIBRARIES})
  set_target_properties(uwebsocket PROPERTIES OUTPUT_NAME "uwebsocket" PREFIX "")
else()
  add_library(uwebsocket ${SOURCES})
  set_target_properties(uwebsocket PROPERTIES OUTPUT_NAME "luauwebsocket")
endif()

target_compile_features(uwebsocket PRIVATE cxx_auto_type cxx_range_for cxx_nullptr)

if (LINK_FLAGS)
  set_target_properties(uwebsocket PROPERTIES LINK_FLAGS ${LINK_FLAGS})
endif()

if (CMAKE_INSTALL_PREFIX)
  install(TARGETS uwebsocket DESTINATION "${CMAKE_INSTALL_PREFIX}/lib")
endif()
