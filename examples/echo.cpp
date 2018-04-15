#include <uWS/uWS.h>

uWS::WebSocket<uWS::SERVER>* lst = nullptr;

int main()
{
    uWS::Hub h;

    h.onMessage([](uWS::WebSocket<uWS::SERVER> *ws, char *message, size_t length, uWS::OpCode opCode) {
        printf("message is %s length is %d \n", message,length);
        ws->send(message, length, opCode);
    });

    h.onConnection([](uWS::WebSocket<uWS::SERVER> *ws, uWS::HttpRequest req) {
        printf("onConnection is %x \n", ws);

        if (lst != ws) {
          printf("change new ws\n");
        }

        ws = lst;
    });

    h.onDisconnection([](uWS::WebSocket<uWS::SERVER> *ws, int code, char *message, size_t length){
      printf("onDisconnection is %x \n", ws);
    });

    if (h.listen(3000)) {
        h.run();
    }
}
