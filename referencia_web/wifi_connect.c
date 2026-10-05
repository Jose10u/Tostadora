#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"

#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"

#include "wifi_connect.h"
#include "wifi_credentials.h"

static const char *TAG = "WIFI_CONNECT";

static EventGroupHandle_t s_wifi_event_group;

#define WIFI_CONNECTED_BIT BIT0


static void event_handler(void *arg,
                          esp_event_base_t event_base,
                          int32_t event_id,
                          void *event_data)
{
    // =========================================================
    // WIFI START
    // =========================================================
    if (event_base == WIFI_EVENT &&
        event_id == WIFI_EVENT_STA_START) {

        ESP_LOGI(TAG,
                 "Conectando a %s...",
                 WIFI_SSID);

        esp_err_t err = esp_wifi_connect();

        if (err != ESP_OK) {
            ESP_LOGE(TAG,
                     "esp_wifi_connect() fallo: %s",
                     esp_err_to_name(err));
        }
    }

    // =========================================================
    // WIFI DISCONNECTED
    // =========================================================
    else if (event_base == WIFI_EVENT &&
             event_id == WIFI_EVENT_STA_DISCONNECTED) {

        wifi_event_sta_disconnected_t *disc =
            (wifi_event_sta_disconnected_t *)event_data;

        ESP_LOGW(TAG,
                 "WiFi desconectado. Razon: %d",
                 disc->reason);

        xEventGroupClearBits(
            s_wifi_event_group,
            WIFI_CONNECTED_BIT
        );

        ESP_LOGI(TAG,
                 "Intentando reconectar...");

        esp_err_t err = esp_wifi_connect();

        if (err != ESP_OK) {
            ESP_LOGE(TAG,
                     "Error reconectando: %s",
                     esp_err_to_name(err));
        }
    }

    // =========================================================
    // GOT IP
    // =========================================================
    else if (event_base == IP_EVENT &&
             event_id == IP_EVENT_STA_GOT_IP) {

        ip_event_got_ip_t *event =
            (ip_event_got_ip_t *)event_data;

        ESP_LOGI(TAG,
                 "IP obtenida: " IPSTR,
                 IP2STR(&event->ip_info.ip));

        ESP_LOGI(TAG,
                 "Pagina disponible en: http://" IPSTR "/",
                 IP2STR(&event->ip_info.ip));

        xEventGroupSetBits(
            s_wifi_event_group,
            WIFI_CONNECTED_BIT
        );
    }
}


void wifi_connect_init_sta(void)
{
    // Crear grupo de eventos
    s_wifi_event_group = xEventGroupCreate();

    // Inicializar TCP/IP
    ESP_ERROR_CHECK(esp_netif_init());

    // Crear event loop
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    // Crear interfaz STA
    esp_netif_create_default_wifi_sta();

    // Configuración WiFi
    wifi_init_config_t cfg =
        WIFI_INIT_CONFIG_DEFAULT();

    ESP_ERROR_CHECK(
        esp_wifi_init(&cfg)
    );

    // Registrar eventos WiFi
    ESP_ERROR_CHECK(
        esp_event_handler_instance_register(
            WIFI_EVENT,
            ESP_EVENT_ANY_ID,
            &event_handler,
            NULL,
            NULL
        )
    );

    // Registrar evento IP
    ESP_ERROR_CHECK(
        esp_event_handler_instance_register(
            IP_EVENT,
            IP_EVENT_STA_GOT_IP,
            &event_handler,
            NULL,
            NULL
        )
    );

    // Configuración de la red
    wifi_config_t wifi_config = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASS,
        },
    };

    ESP_ERROR_CHECK(
        esp_wifi_set_mode(WIFI_MODE_STA)
    );

    ESP_ERROR_CHECK(
        esp_wifi_set_config(
            WIFI_IF_STA,
            &wifi_config
        )
    );

    // Iniciar WiFi
    ESP_ERROR_CHECK(
        esp_wifi_start()
    );

    // Desactivar ahorro de energía
    ESP_ERROR_CHECK(
        esp_wifi_set_ps(WIFI_PS_NONE)
    );

    // Esperar hasta obtener IP
    xEventGroupWaitBits(
        s_wifi_event_group,
        WIFI_CONNECTED_BIT,
        pdFALSE,
        pdTRUE,
        portMAX_DELAY
    );

    ESP_LOGI(TAG,
             "WiFi conectado y listo.");
}