#include <esp_wifi.h>
#include <esp_event.h>
#include <esp_log.h>
#include <esp_system.h>
#include <nvs_flash.h>
#include <sys/param.h>
#include "nvs_flash.h"
#include "esp_netif.h"
#include "esp_eth.h"
#include "driver/gpio.h"
#include "driver/uart.h"
#include <esp_http_server.h>
#include "lwip/dhcp.h"
#include "lwip/netif.h"

#define MAC2STR(a)  (a)[0], (a)[1], (a)[2], (a)[3], (a)[4], (a)[5]
#define MACSTR      "%02x:%02x:%02x:%02x:%02x:%02x"

#include "index.h"

#define WIFI_SSID           "den-esp"
#define WIFI_PASS           "den12345"
#define WIFI_CHANNEL        7
#define MAX_STA_CONN        4
#define LED                 22
#define UART_STM            UART_NUM_1
#define UART_TX_PIN         GPIO_NUM_4
#define UART_RX_PIN         GPIO_NUM_5
#define UART_BUFFER_SIZE    128

// #define STA_SSID            "Lanars_Community"
#define STA_SSID            "MikroTik"
#define STA_PASS            "youshouldbehere!"

static const char *TAG = "esp-server";

uint8_t ledIsOn = 0;

uint8_t uart_tx[UART_BUFFER_SIZE];
uint8_t uart_rx[UART_BUFFER_SIZE];

esp_netif_t *sta_netif = NULL; // WiFi Station network interface

void uart_init(void);

// #if CONFIG_EXAMPLE_BASIC_AUTH
// #endif

/* An HTTP GET handler */
static esp_err_t index_get_handler(httpd_req_t *req) {
    int i;
    int lvl = gpio_get_level(LED);
    char *buf = calloc(1, index_htm_len+1);
    const char *led_lvl = lvl ? "ON " : "OFF";
    const char *led_cls = lvl ? "on " : "off";
    for(i = 0; i < index_htm_len; i++) {
        buf[i] = index_htm[i];
        if(i > 3 && buf[i-2] == 'C' && buf[i-1] == 'C' && buf[i] == 'C') {
            buf[i-2] = led_cls[0];
            buf[i-1] = led_cls[1];
            buf[i] = led_cls[2];
        }
        if(i > 3 && buf[i-2] == 'T' && buf[i-1] == 'T' && buf[i] == 'T') {
            buf[i-2] = led_lvl[0];
            buf[i-1] = led_lvl[1];
            buf[i] = led_lvl[2];
        }
    }
    buf[i] = 0;
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    httpd_resp_set_hdr(req, "Content-Type", "text/html; charset=UTF-8");
    httpd_resp_send(req, buf, HTTPD_RESP_USE_STRLEN);
    free(buf);
    return ESP_OK;
}

static const httpd_uri_t uri_index = {
    .uri       = "/",
    .method    = HTTP_GET,
    .handler   = index_get_handler,
    .user_ctx  = NULL
};

static void send_stm_command(uint8_t cmd) {
    uart_tx[0] = 'c'; //0xAA;
    uart_tx[1] = 'm'; //0x55;
    uart_tx[2] = cmd;
    uart_tx[3] = 'd'; //0x55;

    uart_write_bytes(UART_STM, uart_tx, 4);
    uart_flush_input(UART_STM);
}

static int get_stm_answer() {
    int rxBytes = 0;
    int timeout = 20;

    while (rxBytes == 0 && timeout > 0) {
        rxBytes = uart_read_bytes(UART_STM, uart_rx, 127, 200 / portTICK_PERIOD_MS);
        timeout--;
    }
    ESP_LOGI(TAG, "Got %d bytes", rxBytes);
    if(rxBytes > 0) {
        if(uart_rx[0] == 0xAA && uart_rx[1] == 0x55 && uart_rx[rxBytes - 1] == 0x55) {
            if(rxBytes > 2) {
                return rxBytes - 3;
            }
        } else {
            return 0 - rxBytes;
        }
    }
    return 0;
}

static esp_err_t query_handler(httpd_req_t *req) {
    char server_string[16] = "error\0";
    char queryCode = 0;
    uint8_t cmd = 0;
    int i, sz, dx;

    ESP_LOGI(TAG, "Got query uri: [%s]", req->uri);

    queryCode = req->uri[7];

    switch(queryCode) {
    case '0': cmd = 0x30; break; // 0x30 = 0, Led OFF
    case '1': cmd = 0x31; break; // 0x31 = 1, Led ON
    case '2': cmd = 0x74; break; // 0x74 = t, Get Temp string
    case '3': cmd = 0x68; break; // 0x68 = h, Get Humidity string
    case '4': cmd = 0x70; break; // 0x70 = p, Get Pressure string
    }

    if(cmd) {
        ESP_LOGI(TAG, "Send command 0x%X", cmd);
        send_stm_command(cmd);
        sz = get_stm_answer();
        ESP_LOGI(TAG, "Answer sz = %i", sz);
        if(sz) {
            if(sz < 0) {
                sz = 0 - sz;
                dx = 0;
            } else {
                dx = 2;
            }
            for(i = 0; i < sz; i++) {
                if(i == 14) {
                    server_string[i] = '~';
                    break;
                }
                server_string[i] = uart_rx[dx + i];
            }
            if(i > 14) i = 14;
            server_string[i] = 0;
        }
    }

    httpd_resp_send(req, server_string, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

static const httpd_uri_t uri_query = {
    .uri       = "/query/*",
    .method    = HTTP_GET,
    .handler   = query_handler,
    .user_ctx  = NULL
};

static esp_err_t led_handler(httpd_req_t *req) {
    ESP_LOGI(TAG, "Got led uri: [%s]", req->uri);

    char queryCode = 0;
    char resp_str[2] = { 0, 0 };
    queryCode = req->uri[5]; //    /led/0
    ESP_LOGI(TAG, "Led code = %c", queryCode);
    if(queryCode == '1') {
        resp_str[0] = '1';
        gpio_set_level(LED, 1);
        ESP_LOGI(TAG, "Led ON");
    } else if(queryCode == '0') {
        resp_str[0] = '0';
        gpio_set_level(LED, 0);
        ESP_LOGI(TAG, "Led OFF");
    } else {
        resp_str[0] = '?';
    }

    httpd_resp_send(req, resp_str, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;

}

static const httpd_uri_t uri_led = {
    .uri       = "/led/*",
    .method    = HTTP_GET,
    .handler   = led_handler,
    .user_ctx  = NULL
};


esp_err_t http_404_error_handler(httpd_req_t *req, httpd_err_code_t err) {
    httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "Requested url is not found!");
    return ESP_OK;
}

static httpd_handle_t start_webserver(void) {
    httpd_handle_t server = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.lru_purge_enable = true;
    config.max_uri_handlers = 8; // it's default
    config.uri_match_fn = httpd_uri_match_wildcard; // wildcard uri


    // Start the httpd server
    ESP_LOGI(TAG, "Starting server on port: '%d'", config.server_port);
    if (httpd_start(&server, &config) == ESP_OK) {
        // Set URI handlers
        ESP_LOGI(TAG, "Registering URI handlers");
        httpd_register_uri_handler(server, &uri_index);
        httpd_register_uri_handler(server, &uri_query);
        httpd_register_uri_handler(server, &uri_led);

        httpd_register_err_handler(server, HTTPD_404_NOT_FOUND, http_404_error_handler);
        return server;
    }

    ESP_LOGI(TAG, "Error starting server!");
    return NULL;
}

static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                                    int32_t event_id, void* event_data) {
    if (event_id == WIFI_EVENT_AP_STACONNECTED) {
        wifi_event_ap_staconnected_t* event = (wifi_event_ap_staconnected_t*) event_data;
        ESP_LOGI(TAG, "station "MACSTR" join, AID=%d",
                 MAC2STR(event->mac), event->aid);
    } else if (event_id == WIFI_EVENT_AP_STADISCONNECTED) {
        wifi_event_ap_stadisconnected_t* event = (wifi_event_ap_stadisconnected_t*) event_data;
        ESP_LOGI(TAG, "station "MACSTR" leave, AID=%d",
                 MAC2STR(event->mac), event->aid);
    }
}

void wifi_init_softap(void) {
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_ap();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        NULL));

    wifi_config_t wifi_config = {
        .ap = {
            .ssid       = WIFI_SSID,
            .ssid_len   = strlen(WIFI_SSID),
            .channel    = WIFI_CHANNEL,
            .password   = WIFI_PASS,
            .max_connection = MAX_STA_CONN,
            .authmode   = WIFI_AUTH_WPA_WPA2_PSK
        },
    };
    if (strlen(WIFI_PASS) == 0) {
        wifi_config.ap.authmode = WIFI_AUTH_OPEN;
    }

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "wifi_init_softap finished. SSID:%s password:%s channel:%d",
             WIFI_SSID, WIFI_PASS, WIFI_CHANNEL);
}

static void wifi_sta_event_handler(void* arg, esp_event_base_t event_base,
                                int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_CONNECTED) {
        ESP_LOGI(TAG, "WiFi connected");
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        esp_wifi_connect();
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
    }
}

/* Initialize Wi-Fi as sta and set scan method */
static void wifi_init_sta(void)
{
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_sta_event_handler, NULL, NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_sta_event_handler, NULL, NULL));

    // Initialize default station as network interface instance (esp-netif)
    sta_netif = esp_netif_create_default_wifi_sta();
    assert(sta_netif);

    // Initialize and start WiFi
    wifi_config_t wifi_config = {
        .sta = {
            .ssid               = STA_SSID,
            .password           = STA_PASS,
            .scan_method        = WIFI_FAST_SCAN, // WIFI_ALL_CHANNEL_SCAN
            .sort_method        = WIFI_CONNECT_AP_BY_SIGNAL,
            .threshold.rssi     = -127,
            .threshold.authmode = WIFI_AUTH_WPA2_PSK,
        },
    };
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
}

void uart_init() {
    const uart_config_t uart_config = {
        .baud_rate  = 115200,
        .data_bits  = UART_DATA_8_BITS,
        .parity     = UART_PARITY_DISABLE,
        .stop_bits  = UART_STOP_BITS_1,
        .flow_ctrl  = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_APB
    };
    int rx_buf_size = UART_BUFFER_SIZE * 2;
    uart_driver_install(UART_STM, rx_buf_size, 0, 20, NULL, 0);
    uart_param_config(UART_STM, &uart_config);
    //                          TX = 4       RX = 5
    uart_set_pin(UART_STM, UART_TX_PIN, UART_RX_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
}

void app_main(void) {
    ESP_ERROR_CHECK(nvs_flash_init());
    // wifi_init_softap();
    wifi_init_sta();
    uart_init();
    // GPIO Init
    gpio_set_direction(LED, GPIO_MODE_OUTPUT);
    gpio_set_level(LED, 0);

    /* Start the server for the first time */
    (void)start_webserver();
}
