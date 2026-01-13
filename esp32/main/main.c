#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_http_server.h"
#include "driver/uart.h"
#include "driver/gpio.h"

static const char *TAG = "lift_control";

// WiFi credentials - CHANGE THESE!
#define WIFI_SSID "YOUR_WIFI_SSID"
#define WIFI_PASSWORD "YOUR_WIFI_PASSWORD"

// UART configuration
#define LIFT_UART_NUM UART_NUM_1
#define REMOTE_UART_NUM UART_NUM_2
#define LIFT_RX_PIN GPIO_NUM_16
#define LIFT_TX_PIN GPIO_NUM_17
#define REMOTE_RX_PIN GPIO_NUM_9
#define REMOTE_TX_PIN GPIO_NUM_10
#define UART_BUF_SIZE 1024
#define SERIAL_BAUD 9600

// Button command opcodes
static const uint8_t CMD_UP_PRESS[] = {0x55, 0xAA, 0xE3, 0xE3, 0xE3};
static const uint8_t CMD_UP_RELEASE[] = {0x55, 0xAA, 0xE1, 0xE1, 0xE1};
static const uint8_t CMD_DOWN_PRESS[] = {0x55, 0xAA, 0xE2, 0xE2, 0xE2};
static const uint8_t CMD_DOWN_RELEASE[] = {0x55, 0xAA, 0xE3, 0xE3, 0xE3};
static const uint8_t CMD_POS1[] = {0x55, 0xAA, 0xD1, 0xD1, 0xD1};
static const uint8_t CMD_POS2[] = {0x55, 0xAA, 0xD2, 0xD2, 0xD2};
static const uint8_t CMD_POS3[] = {0x55, 0xAA, 0xD3, 0xD3, 0xD3};
static const uint8_t CMD_POS4[] = {0x55, 0xAA, 0xD7, 0xD7, 0xD7};

static httpd_handle_t server = NULL;

const char* html_page = 
"<!DOCTYPE html>"
"<html>"
"<head>"
"<title>TV Lift Control</title>"
"<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">"
"<style>"
"body { font-family: Arial, sans-serif; max-width: 600px; margin: 50px auto; padding: 20px; background-color: #f5f5f5; }"
"h1 { color: #333; text-align: center; }"
".controls { display: flex; flex-direction: column; gap: 15px; margin-top: 30px; }"
".button-group { display: flex; gap: 10px; justify-content: center; }"
"button { padding: 15px 30px; font-size: 16px; border: none; border-radius: 5px; cursor: pointer; background-color: #4CAF50; color: white; transition: background-color 0.3s; }"
"button:hover { background-color: #45a049; }"
"button:active { background-color: #3d8b40; }"
"button.up { background-color: #2196F3; }"
"button.up:hover { background-color: #0b7dda; }"
"button.down { background-color: #f44336; }"
"button.down:hover { background-color: #da190b; }"
".memory-positions { display: grid; grid-template-columns: repeat(2, 1fr); gap: 10px; margin-top: 20px; }"
".status { margin-top: 30px; padding: 15px; background-color: #e3f2fd; border-radius: 5px; text-align: center; }"
"</style>"
"</head>"
"<body>"
"<h1>TV Lift Control</h1>"
"<div class=\"controls\">"
"<div class=\"button-group\">"
"<button class=\"up\" onmousedown=\"sendCommand('up', 'press')\" onmouseup=\"sendCommand('up', 'release')\" ontouchstart=\"sendCommand('up', 'press')\" ontouchend=\"sendCommand('up', 'release')\">Up ↑</button>"
"<button class=\"down\" onmousedown=\"sendCommand('down', 'press')\" onmouseup=\"sendCommand('down', 'release')\" ontouchstart=\"sendCommand('down', 'press')\" ontouchend=\"sendCommand('down', 'release')\">Down ↓</button>"
"</div>"
"<div class=\"memory-positions\">"
"<button onclick=\"sendCommand('pos1', 'press')\">Position 1</button>"
"<button onclick=\"sendCommand('pos2', 'press')\">Position 2</button>"
"<button onclick=\"sendCommand('pos3', 'press')\">Position 3</button>"
"<button onclick=\"sendCommand('pos4', 'press')\">Position 4</button>"
"</div>"
"</div>"
"<div class=\"status\" id=\"status\">Ready</div>"
"<script>"
"function sendCommand(cmd, action) {"
"const url = action ? `/cmd?action=${cmd}&type=${action}` : `/cmd?action=${cmd}`;"
"fetch(url)"
".then(response => response.json())"
".then(data => { document.getElementById('status').textContent = data.status || 'Command sent'; })"
".catch(error => { document.getElementById('status').textContent = 'Error: ' + error; });"
"event.preventDefault();"
"return false;"
"}"
"</script>"
"</body>"
"</html>";

static void send_command(const uint8_t* cmd, size_t len) {
    uart_write_bytes(LIFT_UART_NUM, (const char*)cmd, len);
}

static esp_err_t root_handler(httpd_req_t *req) {
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, html_page, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

static esp_err_t cmd_handler(httpd_req_t *req) {
    char buf[100];
    char action[20] = {0};
    char type[20] = {0};
    
    if (httpd_req_get_url_query_str(req, buf, sizeof(buf)) == ESP_OK) {
        httpd_query_key_value(buf, "action", action, sizeof(action));
        httpd_query_key_value(buf, "type", type, sizeof(type));
    }
    
    const char* status_msg = "OK";
    
    if (strcmp(action, "up") == 0) {
        if (strcmp(type, "press") == 0) {
            send_command(CMD_UP_PRESS, 5);
            status_msg = "Up pressed";
        } else if (strcmp(type, "release") == 0) {
            send_command(CMD_UP_RELEASE, 5);
            status_msg = "Up released";
        }
    } else if (strcmp(action, "down") == 0) {
        if (strcmp(type, "press") == 0) {
            send_command(CMD_DOWN_PRESS, 5);
            status_msg = "Down pressed";
        } else if (strcmp(type, "release") == 0) {
            send_command(CMD_DOWN_RELEASE, 5);
            status_msg = "Down released";
        }
    } else if (strcmp(action, "pos1") == 0) {
        send_command(CMD_POS1, 5);
        status_msg = "Position 1";
    } else if (strcmp(action, "pos2") == 0) {
        send_command(CMD_POS2, 5);
        status_msg = "Position 2";
    } else if (strcmp(action, "pos3") == 0) {
        send_command(CMD_POS3, 5);
        status_msg = "Position 3";
    } else if (strcmp(action, "pos4") == 0) {
        send_command(CMD_POS4, 5);
        status_msg = "Position 4";
    } else {
        httpd_resp_set_status(req, "400 Bad Request");
        httpd_resp_send(req, "{\"error\":\"Invalid command\"}", HTTPD_RESP_USE_STRLEN);
        return ESP_OK;
    }
    
    char json_response[100];
    snprintf(json_response, sizeof(json_response), "{\"status\":\"%s\"}", status_msg);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, json_response, HTTPD_RESP_USE_STRLEN);
    
    return ESP_OK;
}

static void httpd_register_uri_handlers(void) {
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.max_uri_handlers = 10;
    
    if (httpd_start(&server, &config) == ESP_OK) {
        httpd_uri_t root = {
            .uri       = "/",
            .method    = HTTP_GET,
            .handler   = root_handler,
        };
        httpd_register_uri_handler(server, &root);
        
        httpd_uri_t cmd = {
            .uri       = "/cmd",
            .method    = HTTP_GET,
            .handler   = cmd_handler,
        };
        httpd_register_uri_handler(server, &cmd);
        
        ESP_LOGI(TAG, "HTTP server started");
    } else {
        ESP_LOGE(TAG, "Error starting HTTP server!");
    }
}

static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                                int32_t event_id, void* event_data) {
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        ESP_LOGI(TAG, "WiFi disconnected, attempting reconnect");
        esp_wifi_connect();
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "WiFi connected! IP address: " IPSTR, IP2STR(&event->ip_info.ip));
        httpd_register_uri_handlers();
    }
}

static void wifi_init_sta(void) {
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL));

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASSWORD,
            .threshold.authmode = WIFI_AUTH_WPA2_PSK,
        },
    };
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "WiFi init finished. Connecting to %s...", WIFI_SSID);
}

static void proxy_task(void *pvParameters) {
    uint8_t* data = (uint8_t*)malloc(UART_BUF_SIZE);
    
    while (1) {
        // Forward lift → remote (status frames for display)
        int len = uart_read_bytes(LIFT_UART_NUM, data, UART_BUF_SIZE, 20 / portTICK_PERIOD_MS);
        if (len > 0) {
            uart_write_bytes(REMOTE_UART_NUM, (const char*)data, len);  // Forward to remote
            // Log to console (first few bytes)
            for (int i = 0; i < len && i < 16; i++) {
                printf("%02x ", data[i]);
            }
            if (len > 0) printf("\n");
        }
        
        // Forward remote → lift (button commands from physical remote)
        len = uart_read_bytes(REMOTE_UART_NUM, data, UART_BUF_SIZE, 20 / portTICK_PERIOD_MS);
        if (len > 0) {
            uart_write_bytes(LIFT_UART_NUM, (const char*)data, len);  // Forward to lift
            // Log to console
            for (int i = 0; i < len && i < 16; i++) {
                printf("%02x ", data[i]);
            }
            if (len > 0) printf("\n");
        }
        
        vTaskDelay(10 / portTICK_PERIOD_MS);
    }
    
    free(data);
    vTaskDelete(NULL);
}

void app_main(void) {
    ESP_LOGI(TAG, "Starting TV Lift Control");
    
    // Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    
    // Configure UART for lift controller
    uart_config_t lift_uart_config = {
        .baud_rate = SERIAL_BAUD,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
    };
    ESP_ERROR_CHECK(uart_driver_install(LIFT_UART_NUM, UART_BUF_SIZE * 2, 0, 0, NULL, 0));
    ESP_ERROR_CHECK(uart_param_config(LIFT_UART_NUM, &lift_uart_config));
    ESP_ERROR_CHECK(uart_set_pin(LIFT_UART_NUM, LIFT_TX_PIN, LIFT_RX_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
    
    // Configure UART for remote
    uart_config_t remote_uart_config = {
        .baud_rate = SERIAL_BAUD,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
    };
    ESP_ERROR_CHECK(uart_driver_install(REMOTE_UART_NUM, UART_BUF_SIZE * 2, 0, 0, NULL, 0));
    ESP_ERROR_CHECK(uart_param_config(REMOTE_UART_NUM, &remote_uart_config));
    ESP_ERROR_CHECK(uart_set_pin(REMOTE_UART_NUM, REMOTE_TX_PIN, REMOTE_RX_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
    
    ESP_LOGI(TAG, "UART initialized");
    
    // Initialize WiFi
    wifi_init_sta();
    
    // Start proxy task
    xTaskCreate(proxy_task, "proxy_task", 4096, NULL, 5, NULL);
    
    ESP_LOGI(TAG, "TV Lift Control initialized");
}
