#include "../LuaFunction.hpp"
#include<stdlib.h>
#include<string.h>
#include "uWS.h"
#include <queue>
#include <thread>
#include <mutex>
using namespace std;

#define uwebsocketMETA  "net.uwebsocket"

struct Command {
  int cmd;
  union {
    // onConnection onDisconnection
    uWS::WebSocket<uWS::SERVER>* ws;

    // onMessage
    struct {
      std::string payload;
      uWS::OpCode opCode;
      uWS::WebSocket<uWS::SERVER>* ws;
    } message;
  }content;
};

struct SendMessage {
  size_t size;
  char* data;
  uWS::OpCode opCode;
  uWS::WebSocket<uWS::SERVER>* ws;
};

class WebSocketServer: public uWS::Hub {
public:
  WebSocketServer(LuaFunction&& listener):listener_(std::move(listener))
  {}

  queue<Command*> cmds;
  queue<struct SendMessage*> sendMessage;
  LuaFunction listener_;
};

static WebSocketServer* instance_ = nullptr;
static std::mutex g_i_mutex;
static std::mutex g_s_mutex;
static bool done = false;

void WebSocket_run() {
  printf("WebSocket_run running on ther 3000 port\r\n");
  //instance_->run();
  while (!done) {
    std::lock_guard<std::mutex> lock(g_s_mutex);

    while (!instance_->sendMessage.empty())
    {
       struct SendMessage* message = instance_->sendMessage.front();
       message->ws->send((char*)message->data, message->size, message->opCode);

       delete(message->data);
       delete(message);
       instance_->sendMessage.pop();
    }
    instance_->poll();
  }
}

static int uwebsocket_create(lua_State* L)
{
	WebSocketServer** w = (WebSocketServer**)lua_newuserdata(L, sizeof(*w));
	*w = new WebSocketServer(LuaFunction(L, 1));
  instance_ = *w;

  instance_->onMessage([](uWS::WebSocket<uWS::SERVER> *ws, char *message, size_t length, uWS::OpCode opCode) {
    //printf("message is %s length is %d \n", message,(int)length);
    // ws->send(message, length, opCode);
    std::lock_guard<std::mutex> lock(g_i_mutex);

    Command* cmd = (Command*)malloc(sizeof(Command));
    cmd->cmd = 2;

    cmd->content.message.ws = ws;
    cmd->content.message.opCode = opCode;
    cmd->content.message.payload.assign(message,length);

    instance_->cmds.push(cmd);
  });

  instance_->onConnection([](uWS::WebSocket<uWS::SERVER> *ws, uWS::HttpRequest req) {
    //printf("onConnection is %x \n", ws);
    std::lock_guard<std::mutex> lock(g_i_mutex);

    Command* cmd = (Command*)malloc(sizeof(Command));
    cmd->cmd = 0;
    cmd->content.ws = ws;

    instance_->cmds.push(cmd);
  });

  instance_->onDisconnection([](uWS::WebSocket<uWS::SERVER>* ws, int code, char *message, size_t length){
    std::lock_guard<std::mutex> lock(g_i_mutex);
    Command* cmd = (Command*)malloc(sizeof(Command));
    cmd->cmd = 1;
    cmd->content.ws = ws;

    instance_->cmds.push(cmd);
  });

	luaL_getmetatable(L, uwebsocketMETA);
	lua_setmetatable(L, -2);
  instance_->listen(3000);

  std::thread* th1 = new std::thread(WebSocket_run);
  printf("create thread for run the websocket\r\n");

	return 1;
}

static inline WebSocketServer** touwebsocketp(lua_State* L){
	return (WebSocketServer**)luaL_checkudata(L, 1, uwebsocketMETA);
}

WebSocketServer* touwebsocket(lua_State* L) {
	auto w = touwebsocketp(L);
	if (*w == NULL)
		luaL_error(L, "uwebsocket already closed");
	return *w;
}

static int uwebsocket_tostring(lua_State* L)
{
	auto w = touwebsocketp(L);
	if (*w)
		lua_pushfstring(L, "uwebsocket (%p)", *w);
	else
		lua_pushliteral(L, "uwebsocket (closed)");
	return 1;
}


static int uwebsocket_gc(lua_State* L)
{
	auto w = touwebsocketp(L);

	printf("finalizing LUA object (%s)\n", uwebsocketMETA);

	if (!*w)
		return 0;

	delete *w;
	*w = nullptr; // mark as closed
	return 0;
}

static int uwebsocket_write(lua_State* L)
{
	auto socket = touwebsocket(L);
  uWS::WebSocket<uWS::SERVER>* ws = (uWS::WebSocket<uWS::SERVER>*)lua_touserdata(L,2);

	size_t size = 0;
	const char* data = luaL_checklstring(L, 3, &size);
	uWS::OpCode opCode = (uWS::OpCode)lua_tointeger(L, 4);

  //printf("call uwebsocket_write %s \n",data);

  struct SendMessage* msg = new struct SendMessage();
  msg->size = size;
  msg->data = new char[size];//data;
  memcpy(msg->data,(const void*)data,size);
  msg->opCode = opCode;
  msg->ws = ws;

	//ws->send((char*)data, size, opCode);

  {
    std::lock_guard<std::mutex> lock(g_s_mutex);
    instance_->sendMessage.push(msg);
  }
  //printf("push to sendMessage\n");
	return 0;
}

static int uwebsocket_poll(lua_State* L) {
  std::lock_guard<std::mutex> lock(g_i_mutex);
  queue<Command*>& cmds = instance_->cmds;
  LuaFunction& listener = instance_->listener_;

  while (!cmds.empty())
  {
     Command* command = cmds.front();
     if (command->cmd == 0) {
       void* ptr = (void*)command->content.ws;
       listener("connect",ptr);
     } else if (command->cmd == 1) {
       void* ptr = (void*)command->content.ws;
       listener("close",ptr);
     } else {
       void* ptr = (void*)command->content.message.ws;
       int i_opCode = (int)command->content.message.opCode;
       listener("message",command->content.message.payload,i_opCode,ptr);
     }

     free(command);
     cmds.pop();
  }

  return 0;
}

static luaL_Reg methods[] = {
	{ "__gc", uwebsocket_gc },
	{ "__tostring", uwebsocket_tostring },
  { "delete", uwebsocket_gc},
	{ "write", uwebsocket_write },
  { "poll", uwebsocket_poll },

	{ NULL, NULL },
};

static luaL_Reg api[] = {
	{ "server", uwebsocket_create },

	{ NULL, NULL },
};

extern "C" int luaopen_uwebsocket(lua_State *L)
{
	// create the class Metatable
	luaL_newmetatable(L, uwebsocketMETA);
	lua_pushvalue(L, -1);
	lua_setfield(L, -2, "__index");
	luaL_register(L, NULL, methods);
	lua_pop(L, 1);

	// register the net api
  lua_newtable(L);
	luaL_register(L, NULL, api);

	return 1;
}
