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

#define MAC2STR(a) (a)[0], (a)[1], (a)[2], (a)[3], (a)[4], (a)[5]
#define MACSTR "%02x:%02x:%02x:%02x:%02x:%02x"


#include "index.h"

#define WIFI_SSID           "den-esp"
#define WIFI_PASS           "den12345"
#define WIFI_CHANNEL        7
#define MAX_STA_CONN        4
#define LED                 22
#define UART_STM            UART_NUM_1
#define UART_RX_PIN         5
#define UART_TX_PIN         5
#define UART_BUFFER_SIZE    128

static const char *TAG = "esp-server";

uint8_t ledIsOn = 0;

uint8_t uart_tx[UART_BUFFER_SIZE];
uint8_t uart_rx[UART_BUFFER_SIZE];

void uart_init(void);

// #if CONFIG_EXAMPLE_BASIC_AUTH
// #endif

/* An HTTP GET handler */
static esp_err_t index_get_handler(httpd_req_t *req) {
    int i;
    // int sz;
    // int rep = 0;
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
    // httpd_resp_send(req, "<!DOCTYPE html><html><head><title>ESP32 Server</title></head><body><center>It works</center></body></html>", 106);
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

static esp_err_t query_handler(httpd_req_t *req) {
    int rxBytes = 0;
    char server_string[2] = {0x39, 0}; // 9 - error
    int timeout = 20;

    uart_tx[0] = 0xAA;
    uart_tx[1] = 0x55;
    uart_tx[2] = 0x00;
    uart_tx[3] = 0x55;

    if(ledIsOn) {
        uart_tx[2] = 0x00;
        ledIsOn = 0;
        server_string[0] = 0x37;
    } else {
        uart_tx[2] = 0x01;
        ledIsOn = 1;
        server_string[0] = 0x38;
    }

    uart_write_bytes(UART_STM, uart_tx, 4);
    uart_flush_input(UART_STM);

    // while (rxBytes < 1 && timeout > 0) {
    //     rxBytes = uart_read_bytes(UART_STM, uart_rx, 2, 200 / portTICK_RATE_MS);
    //     timeout--;
    // }
    // if(rxBytes > 0) {
    //     if(uart_rx[0] == 0xAA) {
    //         if(uart_rx[1] == 0x31) {
    //             gpio_set_level(LED, 1);
    //             server_string[0] = 0x31;
    //         } else {
    //             gpio_set_level(LED, 0);
    //             server_string[0] = 0x30;
    //         }
    //     }
    //     // if(uart_rx[0] == 0xAA && uart_rx[1] == 0x55) {
    //     //     // if(uart_rx[0] == 0xAA) {

    //     //     // }
    //     //     server_string[0] = (char)uart_rx[2];
    //     // }
    // }
    httpd_resp_send(req, server_string, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

static const httpd_uri_t uri_query = {
    .uri       = "/query",
    .method    = HTTP_GET,
    .handler   = query_handler,
    .user_ctx  = NULL
};

// static esp_err_t echo_post_handler(httpd_req_t *req)
// {
//     char buf[100];
//     int ret, remaining = req->content_len;
//     while (remaining > 0) {
//         /* Read the data for the request */
//         if ((ret = httpd_req_recv(req, buf,
//                         MIN(remaining, sizeof(buf)))) <= 0) {
//             if (ret == HTTPD_SOCK_ERR_TIMEOUT) {
//                 /* Retry receiving if timeout occurred */
//                 continue;
//             }
//             return ESP_FAIL;
//         }
//         /* Send back the same data */
//         httpd_resp_send_chunk(req, buf, ret);
//         remaining -= ret;
//         /* Log data received */
//         ESP_LOGI(TAG, "=========== RECEIVED DATA ==========");
//         ESP_LOGI(TAG, "%.*s", ret, buf);
//         ESP_LOGI(TAG, "====================================");
//     }
//     // End response
//     httpd_resp_send_chunk(req, NULL, 0);
//     return ESP_OK;
// }
// static const httpd_uri_t echo = {
//     .uri       = "/echo",
//     .method    = HTTP_POST,
//     .handler   = echo_post_handler,
//     .user_ctx  = NULL
// };

/* This handler allows the custom error handling functionality to be
 * tested from client side. For that, when a PUT request 0 is sent to
 * URI /ctrl, the /hello and /echo URIs are unregistered and following
 * custom error handler http_404_error_handler() is registered.
 * Afterwards, when /hello or /echo is requested, this custom error
 * handler is invoked which, after sending an error message to client,
 * either closes the underlying socket (when requested URI is /echo)
 * or keeps it open (when requested URI is /hello). This allows the
 * client to infer if the custom error handler is functioning as expected
 * by observing the socket state.
 */
esp_err_t http_404_error_handler(httpd_req_t *req, httpd_err_code_t err) {
    if (strcmp("/hello", req->uri) == 0) {
        httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "/hello URI is not available");
        /* Return ESP_OK to keep underlying socket open */
        return ESP_OK;
    } else if (strcmp("/echo", req->uri) == 0) {
        httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "/echo URI is not available");
        /* Return ESP_FAIL to close underlying socket */
        return ESP_FAIL;
    }
    /* For any other URI send 404 and close socket */
    httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "Some 404 error message");
    return ESP_FAIL;
}

/* An HTTP PUT handler. This demonstrates realtime
 * registration and deregistration of URI handlers
 */
// static esp_err_t ctrl_put_handler(httpd_req_t *req) {
//     char buf;
//     int ret;
//     if ((ret = httpd_req_recv(req, &buf, 1)) <= 0) {
//         if (ret == HTTPD_SOCK_ERR_TIMEOUT) {
//             httpd_resp_send_408(req);
//         }
//         return ESP_FAIL;
//     }
//     if (buf == '0') {
//         /* URI handlers can be unregistered using the uri string */
//         ESP_LOGI(TAG, "Unregistering /hello and /echo URIs");
//         httpd_unregister_uri(req->handle, "/hello");
//         httpd_unregister_uri(req->handle, "/echo");
//         /* Register the custom error handler */
//         httpd_register_err_handler(req->handle, HTTPD_404_NOT_FOUND, http_404_error_handler);
//     }
//     else {
//         ESP_LOGI(TAG, "Registering /hello and /echo URIs");
//         httpd_register_uri_handler(req->handle, &hello);
//         httpd_register_uri_handler(req->handle, &echo);
//         /* Unregister custom error handler */
//         httpd_register_err_handler(req->handle, HTTPD_404_NOT_FOUND, NULL);
//     }
//     /* Respond with empty body */
//     httpd_resp_send(req, NULL, 0);
//     return ESP_OK;
// }
// static const httpd_uri_t ctrl = {
//     .uri       = "/ctrl",
//     .method    = HTTP_PUT,
//     .handler   = ctrl_put_handler,
//     .user_ctx  = NULL
// };

static httpd_handle_t start_webserver(void) {
    httpd_handle_t server = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.lru_purge_enable = true;
    config.max_uri_handlers = 8; // it's default


    // Start the httpd server
    ESP_LOGI(TAG, "Starting server on port: '%d'", config.server_port);
    if (httpd_start(&server, &config) == ESP_OK) {
        // Set URI handlers
        ESP_LOGI(TAG, "Registering URI handlers");
        httpd_register_uri_handler(server, &uri_index);
        httpd_register_uri_handler(server, &uri_query);
        return server;
    }

    ESP_LOGI(TAG, "Error starting server!");
    return NULL;
}

static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                                    int32_t event_id, void* event_data)
{
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

void app_main(void)
{
    ESP_ERROR_CHECK(nvs_flash_init());
    wifi_init_softap();

    /* Start the server for the first time */
    (void)start_webserver();
}
