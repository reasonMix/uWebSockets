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
      char* payload;
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
static int g_port = 3000;

void WebSocket_run() {
  //instance_->run();
  printf("WebSocket_run running on ther %d port\r\n",g_port);
  while (!done) {
    std::lock_guard<std::mutex> lock(g_s_mutex);

    instance_->poll();

    while (!instance_->sendMessage.empty())
    {
       struct SendMessage* message = instance_->sendMessage.front();
       //printf("pre call xxxxx send \r\n");
       message->ws->send((char*)message->data, message->size, message->opCode);

       free(message->data);
       free(message);
       instance_->sendMessage.pop();
       //printf("post call xxxxx send \r\n");
    }
  }
}

static int uwebsocket_create(lua_State* L)
{
	WebSocketServer** w = (WebSocketServer**)lua_newuserdata(L, sizeof(*w));
	*w = new WebSocketServer(LuaFunction(L, 1));
  instance_ = *w;

  int port = lua_tointeger(L, 2);
  g_port = port;

  instance_->onMessage([](uWS::WebSocket<uWS::SERVER> *ws, char *message, size_t length, uWS::OpCode opCode) {
    //printf("111 message is %s length is %d \n", message,(int)length);
    // ws->send(message, length, opCode);
    std::lock_guard<std::mutex> lock(g_i_mutex);
    //printf("onMessage  111\n");

    Command* cmd = (Command*)malloc(sizeof(Command));
    cmd->cmd = 2;

    cmd->content.message.ws = ws;
    cmd->content.message.opCode = opCode;

    cmd->content.message.payload = (char*)malloc(length+1);
    memcpy(message,(const void*)cmd->content.message.payload,length);
    cmd->content.message.payload[length] = 0;
    //cmd->content.message.payload.assign(message,length);

    instance_->cmds.push(cmd);
    printf("onMessage  222\n");
  });

  instance_->onConnection([](uWS::WebSocket<uWS::SERVER> *ws, uWS::HttpRequest req) {
    //printf("onConnection is %x \n", ws);
    std::lock_guard<std::mutex> lock(g_i_mutex);
    printf("onConnection  111\n");

    Command* cmd = (Command*)malloc(sizeof(Command));
    cmd->cmd = 0;
    cmd->content.ws = ws;

    instance_->cmds.push(cmd);
    printf("onConnection  222\n");
  });

  instance_->onDisconnection([](uWS::WebSocket<uWS::SERVER>* ws, int code, char *message, size_t length){
    std::lock_guard<std::mutex> lock(g_i_mutex);
    printf("onDisconnection  111\n");
    Command* cmd = (Command*)malloc(sizeof(Command));
    cmd->cmd = 1;
    cmd->content.ws = ws;

    instance_->cmds.push(cmd);
    printf("onDisconnection  222\n");
  });

	luaL_getmetatable(L, uwebsocketMETA);
	lua_setmetatable(L, -2);
  instance_->listen(port);

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

static int uwebsocket_getAddress(lua_State* L) {
  auto socket = touwebsocket(L);
  uWS::WebSocket<uWS::SERVER>* ws = (uWS::WebSocket<uWS::SERVER>*)lua_touserdata(L,2);
  uWS::WebSocket<uWS::SERVER>::Address addr = ws->getAddress();
  lua_pushstring(L,(const char*)addr.address);
  return 1;
}

static int uwebsocket_write(lua_State* L)
{
	auto socket = touwebsocket(L);
  uWS::WebSocket<uWS::SERVER>* ws = (uWS::WebSocket<uWS::SERVER>*)lua_touserdata(L,2);

	size_t size = 0;
	const char* data = luaL_checklstring(L, 3, &size);
	uWS::OpCode opCode = (uWS::OpCode)lua_tointeger(L, 4);

  //printf("pre call uwebsocket_write %s \n",data);

  struct SendMessage* msg = (struct SendMessage*)malloc(sizeof(SendMessage));
  msg->size = size;
  msg->data = (char*)malloc(size+1);//data;
  memcpy(msg->data,(const void*)data,size);
  msg->opCode = opCode;
  msg->ws = ws;


	//ws->send((char*)data, size, opCode);

  {
    std::lock_guard<std::mutex> lock(g_s_mutex);
    instance_->sendMessage.push(msg);
  }

  //printf("post call uwebsocket_write \n");
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
       string payload = command->content.message.payload;
       listener("message",payload,i_opCode,ptr);

       free(command->content.message.payload);
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
  { "getAddress", uwebsocket_getAddress },
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
