#include <string.h>
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_netif.h"
#include "esp_mac.h"

#include "wifi_manager.h"
#include "wifi_credentials.h"

#ifndef MACSTR
#define MACSTR "%02x:%02x:%02x:%02x:%02x:%02x"
#endif

#ifndef MAC2STR
#define MAC2STR(a) (a)[0], (a)[1], (a)[2], (a)[3], (a)[4], (a)[5]
#endif

static const char *TAG = "WIFI_MGR";

static bool s_sta_connected = false;
static char s_sta_ip_str[20] = "Desconectado";
static char s_ap_ip_str[20]  = "192.168.4.1";

static esp_netif_t *s_netif_sta = NULL;
static esp_netif_t *s_netif_ap  = NULL;

static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT) {
        switch (event_id) {
            case WIFI_EVENT_STA_START:
                ESP_LOGI(TAG, "WiFi STA iniciado. Conectando a red: %s...", WIFI_SSID);
                esp_wifi_connect();
                break;

            case WIFI_EVENT_STA_DISCONNECTED: {
                wifi_event_sta_disconnected_t *disc = (wifi_event_sta_disconnected_t *)event_data;
                s_sta_connected = false;
                snprintf(s_sta_ip_str, sizeof(s_sta_ip_str), "Desconectado");
                ESP_LOGW(TAG, "WiFi STA desconectado (razon: %d). Reintentando en segundo plano...", disc->reason);
                vTaskDelay(pdMS_TO_TICKS(2000));
                esp_wifi_connect();
                break;
            }

            case WIFI_EVENT_AP_STACONNECTED: {
                wifi_event_ap_staconnected_t *event = (wifi_event_ap_staconnected_t *)event_data;
                ESP_LOGI(TAG, "Nuevo cliente conectado al punto de acceso local (MAC: " MACSTR ")",
                         MAC2STR(event->mac));
                break;
            }

            case WIFI_EVENT_AP_STADISCONNECTED: {
                wifi_event_ap_stadisconnected_t *event = (wifi_event_ap_stadisconnected_t *)event_data;
                ESP_LOGI(TAG, "Cliente desconectado del punto de acceso local (MAC: " MACSTR ")",
                         MAC2STR(event->mac));
                break;
            }

            default:
                break;
        }
    } else if (event_base == IP_EVENT) {
        if (event_id == IP_EVENT_STA_GOT_IP) {
            ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
            snprintf(s_sta_ip_str, sizeof(s_sta_ip_str), IPSTR, IP2STR(&event->ip_info.ip));
            s_sta_connected = true;

            ESP_LOGI(TAG, "==========================================================");
            ESP_LOGI(TAG, ">>> ¡WIFI CONECTADO EXITOSAMENTE A %s! <<<", WIFI_SSID);
            ESP_LOGI(TAG, ">>> IP STA (Router): http://%s/                    <<<", s_sta_ip_str);
            ESP_LOGI(TAG, ">>> IP AP (Directo): http://%s/                    <<<", s_ap_ip_str);
            ESP_LOGI(TAG, "==========================================================");
        }
    }
}

esp_err_t wifi_manager_init(void)
{
    ESP_LOGI(TAG, "Inicializando subsistema de red WiFi (Modo AP+STA)...");

    // 1. Inicializar almacenamiento no volátil NVS para calibraciones y WiFi
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Fallo al inicializar NVS Flash: %s", esp_err_to_name(ret));
        return ret;
    }

    // 2. Inicializar pila TCP/IP y event loop por defecto
    ESP_ERROR_CHECK(esp_netif_init());
    esp_err_t el_ret = esp_event_loop_create_default();
    if (el_ret != ESP_OK && el_ret != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "Fallo al crear event loop: %s", esp_err_to_name(el_ret));
        return el_ret;
    }

    // 3. Crear interfaces de red por defecto para AP y STA
    s_netif_sta = esp_netif_create_default_wifi_sta();
    s_netif_ap  = esp_netif_create_default_wifi_ap();

    // 4. Inicializar driver WiFi con configuración por defecto
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    // 5. Registrar manejadores de eventos WiFi e IP
    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL, NULL));

    // 6. Configurar Modo Dual: AP + STA
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));

    // Configuración SoftAP local (para conexión directa celular / tablet)
    wifi_config_t ap_config = {
        .ap = {
            .ssid = WIFI_AP_SSID,
            .ssid_len = strlen(WIFI_AP_SSID),
            .channel = WIFI_AP_CHANNEL,
            .password = WIFI_AP_PASS,
            .max_connection = WIFI_AP_MAX_CONN,
            .authmode = WIFI_AUTH_WPA2_PSK,
            .pmf_cfg = {
                .required = false,
            },
        },
    };
    if (strlen(WIFI_AP_PASS) == 0) {
        ap_config.ap.authmode = WIFI_AUTH_OPEN;
    }
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &ap_config));

    // Configuración STA (conexión a la red WiFi del hogar)
    wifi_config_t sta_config = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASS,
            .threshold.authmode = WIFI_AUTH_WPA2_PSK,
        },
    };
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &sta_config));

    // 7. Arrancar WiFi
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_PS_NONE)); // Máximo rendimiento y latencia mínima para la interfaz web

    ESP_LOGI(TAG, "Punto de acceso SoftAP activo: SSID '%s' (IP: http://%s/)", WIFI_AP_SSID, s_ap_ip_str);
    ESP_LOGI(TAG, "Buscando red WiFi router: SSID '%s'...", WIFI_SSID);

    return ESP_OK;
}

bool wifi_manager_is_sta_connected(void)
{
    return s_sta_connected;
}

void wifi_manager_get_sta_ip(char *buf, size_t maxlen)
{
    if (buf == NULL || maxlen == 0) return;
    strncpy(buf, s_sta_ip_str, maxlen - 1);
    buf[maxlen - 1] = '\0';
}

void wifi_manager_get_ap_ip(char *buf, size_t maxlen)
{
    if (buf == NULL || maxlen == 0) return;
    strncpy(buf, s_ap_ip_str, maxlen - 1);
    buf[maxlen - 1] = '\0';
}
