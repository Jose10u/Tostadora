#ifndef WIFI_CONNECT_H
#define WIFI_CONNECT_H

// Inicializa WiFi en modo estacion (STA), se conecta a la red
// definida en wifi_credentials.h y bloquea hasta obtener IP.
// Al obtener IP, hace ademas un escaneo de redes de referencia
// (basado en tu ejemplo de "scan") y lo muestra en el log.
void wifi_connect_init_sta(void);

#endif // WIFI_CONNECT_H
