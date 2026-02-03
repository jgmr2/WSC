#include "crow.h"
#include <string>

int main()
{
    crow::SimpleApp app;
    
    CROW_ROUTE(app, "/")([](){
        return "Bienvenido a mi API con Crow";
    });

    app.port(8080).multithreaded().run();
}