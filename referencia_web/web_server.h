#ifndef WEB_SERVER_H
#define WEB_SERVER_H

#include "esp_http_server.h"

// Arranca el servidor HTTP con dos rutas:
//   GET /        -> pagina HTML (se sirve una sola vez)
//   GET /status  -> JSON con {modo, lux, hay_luz, estado}, consultado
//                   por JavaScript cada 1 segundo
httpd_handle_t web_server_start(void);

#endif // WEB_SERVER_H
