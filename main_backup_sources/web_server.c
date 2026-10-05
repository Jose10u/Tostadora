#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "esp_http_server.h"
#include "esp_log.h"

#include "web_server.h"
#include "web_dashboard.h"

static const char *TAG = "WEB_SRV";

static void (*s_get_telem_cb)(web_telemetry_t *t) = NULL;
static web_cmd_callback_t s_cmd_cb = NULL;

static esp_err_t root_get_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/html");
    return httpd_resp_send(req, WEB_DASHBOARD_HTML, HTTPD_RESP_USE_STRLEN);
}

static esp_err_t status_get_handler(httpd_req_t *req)
{
    web_telemetry_t telem;
    memset(&telem, 0, sizeof(telem));

    if (s_get_telem_cb != NULL) {
        s_get_telem_cb(&telem);
    }

    char json[700];
    snprintf(json, sizeof(json),
        "{"
        "\"state\":\"%s\","
        "\"state_desc\":\"%s\","
        "\"step_index\":%d,"
        "\"mode\":%d,"
        "\"mode_name\":\"%s\","
        "\"temp_actual\":%.1f,"
        "\"temp_target\":%.1f,"
        "\"weight_g\":%.1f,"
        "\"time_config_sec\":%d,"
        "\"time_remaining_sec\":%d,"
        "\"time_elapsed_sec\":%d,"
        "\"power_w\":%.1f,"
        "\"energy_kwh\":%.4f,"
        "\"pwm_ptc\":%d,"
        "\"pwm_blw\":%d,"
        "\"pwm_mtr\":%d,"
        "\"sd_ok\":%s,"
        "\"tc_connected\":%s,"
        "\"emergency\":%s,"
        "\"wifi_ip\":\"%s\","
        "\"wifi_connected\":%s,"
        "\"rtc_time\":\"%s\""
        "}",
        telem.state_str ? telem.state_str : "STANDBY",
        telem.state_desc ? telem.state_desc : "Listo",
        telem.step_index,
        telem.mode,
        telem.mode_name ? telem.mode_name : "Bajo",
        telem.temp_actual,
        telem.temp_target,
        telem.weight_g,
        telem.time_config_sec,
        telem.time_remaining_sec,
        telem.time_elapsed_sec,
        telem.power_w,
        telem.energy_kwh,
        telem.pwm_ptc,
        telem.pwm_blw,
        telem.pwm_mtr,
        telem.sd_ok ? "true" : "false",
        telem.tc_connected ? "true" : "false",
        telem.emergency ? "true" : "false",
        telem.wifi_ip ? telem.wifi_ip : "192.168.4.1",
        telem.wifi_connected ? "true" : "false",
        telem.rtc_time ? telem.rtc_time : "--:--:--"
    );

    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    return httpd_resp_send(req, json, HTTPD_RESP_USE_STRLEN);
}

static esp_err_t command_post_handler(httpd_req_t *req)
{
    char buf[256];
    int ret = httpd_req_recv(req, buf, sizeof(buf) - 1);
    if (ret <= 0) {
        if (ret == HTTPD_SOCK_ERR_TIMEOUT) {
            httpd_resp_send_408(req);
        }
        return ESP_FAIL;
    }
    buf[ret] = '\0';

    ESP_LOGI(TAG, "Comando recibido via HTTP POST: %s", buf);

    if (s_cmd_cb != NULL) {
        web_command_t cmd;
        memset(&cmd, 0, sizeof(cmd));

        if (strstr(buf, "\"start_preheat\"") != NULL) {
            cmd.type = WEB_CMD_START_PREHEAT;
            s_cmd_cb(&cmd);
        } else if (strstr(buf, "\"start_roast\"") != NULL) {
            cmd.type = WEB_CMD_START_ROAST;
            s_cmd_cb(&cmd);
        } else if (strstr(buf, "\"pause_resume\"") != NULL) {
            cmd.type = WEB_CMD_PAUSE_RESUME;
            s_cmd_cb(&cmd);
        } else if (strstr(buf, "\"stop\"") != NULL) {
            cmd.type = WEB_CMD_STOP;
            s_cmd_cb(&cmd);
        } else if (strstr(buf, "\"tare\"") != NULL) {
            cmd.type = WEB_CMD_TARE;
            s_cmd_cb(&cmd);
        } else if (strstr(buf, "\"set_mode\"") != NULL) {
            cmd.type = WEB_CMD_SET_MODE;
            char *p = strstr(buf, "\"mode\":");
            if (p) {
                cmd.mode = atoi(p + 7);
            }
            s_cmd_cb(&cmd);
        } else if (strstr(buf, "\"set_pwm\"") != NULL) {
            cmd.type = WEB_CMD_SET_PWM;
            char *p_ptc = strstr(buf, "\"ptc\":");
            char *p_blw = strstr(buf, "\"blw\":");
            char *p_mtr = strstr(buf, "\"mtr\":");

            cmd.ptc_pct = p_ptc ? atoi(p_ptc + 6) : -1;
            cmd.blw_pct = p_blw ? atoi(p_blw + 6) : -1;
            cmd.mtr_pct = p_mtr ? atoi(p_mtr + 6) : -1;

            s_cmd_cb(&cmd);
        }
    }

    const char *resp = "{\"status\":\"ok\"}";
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    return httpd_resp_send(req, resp, HTTPD_RESP_USE_STRLEN);
}

httpd_handle_t web_server_start(void (*get_telem_cb)(web_telemetry_t *t), web_cmd_callback_t cmd_cb)
{
    s_get_telem_cb = get_telem_cb;
    s_cmd_cb = cmd_cb;

    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.max_uri_handlers = 8;
    config.stack_size = 8192; // Pila generosa para atender peticiones web concurrentes

    httpd_handle_t server = NULL;
    if (httpd_start(&server, &config) == ESP_OK) {
        httpd_uri_t uri_root = {
            .uri = "/",
            .method = HTTP_GET,
            .handler = root_get_handler,
        };
        httpd_register_uri_handler(server, &uri_root);

        httpd_uri_t uri_status = {
            .uri = "/api/status",
            .method = HTTP_GET,
            .handler = status_get_handler,
        };
        httpd_register_uri_handler(server, &uri_status);

        httpd_uri_t uri_cmd = {
            .uri = "/api/command",
            .method = HTTP_POST,
            .handler = command_post_handler,
        };
        httpd_register_uri_handler(server, &uri_cmd);

        ESP_LOGI(TAG, ">>> SERVIDOR WEB DE LA TOSTADORA INICIADO EXITOSAMENTE (Puerto %d) <<<", config.server_port);
        return server;
    }

    ESP_LOGE(TAG, "No se pudo iniciar el servidor web");
    return NULL;
}

void web_server_stop(httpd_handle_t server)
{
    if (server) {
        httpd_stop(server);
    }
}
