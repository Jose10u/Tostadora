#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include <stdbool.h>
#include <stddef.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Inicializa el subsistema WiFi en modo dual AP+STA.
 *        Crea el punto de acceso "Tostadora-IoT" (IP 192.168.4.1)
 *        e intenta conectarse a la red WiFi del hogar (jhonjairo).
 */
esp_err_t wifi_manager_init(void);

/**
 * @brief Retorna si la conexión STA (cliente al router) está establecida y tiene IP.
 */
bool wifi_manager_is_sta_connected(void);

/**
 * @brief Obtiene la cadena con la dirección IP de la interfaz STA.
 */
void wifi_manager_get_sta_ip(char *buf, size_t maxlen);

/**
 * @brief Obtiene la cadena con la dirección IP del SoftAP local (192.168.4.1).
 */
void wifi_manager_get_ap_ip(char *buf, size_t maxlen);

#ifdef __cplusplus
}
#endif

#endif // WIFI_MANAGER_H
