//
//  main.cpp
//  uwebsocket-tool
//
//  Created by liuyuntao on 2018/4/17.
//  Copyright © 2018年 liuyuntao. All rights reserved.
//

#include <iostream>

#include <uWS.h>
using namespace uWS;

int main(int argc, const char * argv[]) {
    Hub h;
    std::string response = "Hello!";
    
    h.onMessage([](WebSocket<SERVER> *ws, char *message, size_t length, OpCode opCode) {
        const char* ret = "{\"msgID\":\"GetGtServer\"}";
        ws->send(message, length, opCode);
    });
    
    h.onDisconnection([](WebSocket<SERVER> *ws, int opCode,char*message,size_t size) {
        printf("on disconnect");
    });
    
    h.onHttpRequest([&](HttpResponse *res, HttpRequest req, char *data, size_t length,
                        size_t remainingBytes) {
        res->end(response.data(), response.length());
    });
    
    if (h.listen(28303)) {
        while (true) {
            h.poll();
        }
        
    }
}
