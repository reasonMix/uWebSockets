package = "uwebsocket"
version = "0.0.1-1"

source = {
  url = "git://github.com:reasonMix/uWebSockets.git",
  tag = "v0.0.1"
}
description={
   summary = 'uWebSockets',
   detailed = 'uWebSockets',
   homepage = "https://github.com/reasonMix/uWebSockets",
   license = "The MIT License"
}
dependencies = { "lua >= 5.1" }
build = {
  type = 'cmake',
  platforms = {
     windows = {
        variables = {
           LUA_LIBRARIES = '$(LUA_LIBDIR)/$(LUALIB)'
        }
     }
  },
  variables = {
    BUILD_SHARED_LIBS = 'ON',
    CMAKE_INSTALL_PREFIX = '$(PREFIX)',
    LUA_INCLUDE_DIR = '$(LUA_INCDIR)',
  }
}
