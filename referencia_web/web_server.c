/*
 * web_server.c
 *
 * Basado en tu ejemplo de "web server". Se agrega una ruta /status
 * que devuelve JSON con el estado actual (leido desde lux_control),
 * y la pagina HTML incluye JavaScript que consulta esa ruta cada
 * 1000 ms para actualizarse sin recargar toda la pagina.
 */

#include <stdio.h>
#include <string.h>
#include "esp_http_server.h"
#include "esp_log.h"

#include "web_server.h"
#include "lux_control.h"

static const char *TAG = "WEB_SERVER";

static const char *html_page =
"<!DOCTYPE html>"
"<html lang=\"es\">"
"<head>"
"<meta charset=\"UTF-8\">"
"<title>Monitor de Luz - ESP32</title>"
"<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">"
"<style>"
"body{font-family:Arial, sans-serif; text-align:center; margin-top:50px; background:#f4f4f4;}"
"h1{color:#333;}"
".card{display:inline-block; padding:20px 40px; border-radius:10px; background:#fff;"
"box-shadow:0 0 10px rgba(0,0,0,0.15); font-size:1.2em;}"
".ok{color:green; font-weight:bold;} .bad{color:red; font-weight:bold;}"
"</style>"
"</head>"
"<body>"
"<h1>Monitor de Iluminacion - ESP32</h1>"
"<div class=\"card\">"
"<p><b>Modo:</b> <span id=\"modo\">--</span></p>"
"<p><b>Nivel de Lux:</b> <span id=\"lux\">--</span></p>"
"<p><b>Estado:</b> <span id=\"estado\">--</span></p>"
"</div>"
"<script>"
"function actualizar(){"
"fetch('/status').then(function(r){ return r.json(); }).then(function(data){"
"document.getElementById('modo').innerText = data.modo;"
"document.getElementById('lux').innerText = data.lux.toFixed(1);"
"var estadoEl = document.getElementById('estado');"
"estadoEl.innerText = data.estado;"
"estadoEl.className = data.hay_luz ? 'ok' : 'bad';"
"}).catch(function(err){ console.error(err); });"
"}"
"setInterval(actualizar, 1000);"
"actualizar();"
"</script>"
"</body>"
"</html>";

static esp_err_t root_get_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/html");
    return httpd_resp_send(req, html_page, HTTPD_RESP_USE_STRLEN);
}

static esp_err_t status_get_handler(httpd_req_t *req)
{
    modo_t modo = lux_control_get_modo();
    float lux = lux_control_get_lux();
    bool hay_luz = lux_control_hay_luz_suficiente();

    char json[160];
    snprintf(json, sizeof(json),
        "{\"modo\":\"%s\",\"lux\":%.1f,\"hay_luz\":%s,\"estado\":\"%s\"}",
        modo == MODO_AUTO ? "AUTOMATICO" : "MANUAL",
        lux,
        hay_luz ? "true" : "false",
        hay_luz ? "Mucha luz" : "Poca luz");

    httpd_resp_set_type(req, "application/json");
    return httpd_resp_send(req, json, HTTPD_RESP_USE_STRLEN);
}

httpd_handle_t web_server_start(void)
{
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    httpd_handle_t server = NULL;

    if (httpd_start(&server, &config) == ESP_OK) {
        httpd_uri_t uri_root = {
            .uri = "/",
            .method = HTTP_GET,
            .handler = root_get_handler,
        };
        httpd_register_uri_handler(server, &uri_root);

        httpd_uri_t uri_status = {
            .uri = "/status",
            .method = HTTP_GET,
            .handler = status_get_handler,
        };
        httpd_register_uri_handler(server, &uri_status);

        ESP_LOGI(TAG, "Servidor web iniciado en puerto %d", config.server_port);
        return server;
    }

    ESP_LOGE(TAG, "No se pudo iniciar el servidor web");
    return NULL;
}
