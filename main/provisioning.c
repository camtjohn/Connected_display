#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_netif_ip_addr.h"

// lwIP for simple DNS captive portal
#include "lwip/udp.h"
#include "lwip/pbuf.h"
#include "lwip/ip_addr.h"
#include "lwip/inet.h"

#include "provisioning.h"
#include "device_config.h"

static const char *TAG = "PROVISIONING";

#define SOFTAP_SSID "Connected_Display"
#define SOFTAP_PASS "setup1234"  // Required for WPA2
#define SOFTAP_CHANNEL 6
#define SOFTAP_MAX_CONNECTIONS 4

// Event group for provisioning completion
static EventGroupHandle_t provisioning_event_group = NULL;
#define PROVISIONING_COMPLETE_BIT BIT0

// HTTP Server handle
static httpd_handle_t provisioning_server = NULL;

// Flag to track if provisioning is active
static bool provisioning_active = false;

// DNS server (captive portal) state
static struct udp_pcb *dns_pcb = NULL;
static ip4_addr_t ap_ip_addr;  // 192.168.4.1 by default in ESP SoftAP

static void dns_server_stop(void);
static esp_err_t dns_server_start(void);

/**
 * @brief HTML form for WiFi provisioning with network scanning
 */
static const char provisioning_html[] = "<!DOCTYPE html>"
"<html>"
"<head>"
"<meta charset='UTF-8'>"
"<meta name='viewport' content='width=device-width,initial-scale=1'>"
"<title>Device Setup</title>"
"<style>"
"body{font-family:Arial,sans-serif;margin:0;padding:20px;background:#f0f0f0}"
".container{max-width:400px;margin:50px auto;background:#fff;padding:30px;border-radius:8px;box-shadow:0 2px 10px rgba(0,0,0,0.1)}"
"h1{color:#333;text-align:center;margin-top:0}"
".form-group{margin-bottom:20px}"
"label{display:block;margin-bottom:5px;color:#555;font-weight:bold}"
"input[type='text'],input[type='password'],select,datalist{width:100%;padding:10px;border:1px solid #ddd;border-radius:4px;box-sizing:border-box;font-size:14px}"
"input[type='submit'],button{width:100%;padding:10px;background:#007bff;color:#fff;border:none;border-radius:4px;font-size:16px;cursor:pointer;margin-top:10px}"
"button{background:#6c757d}"
"input[type='submit']:hover{background:#0056b3}"
"button:hover{background:#5a6268}"
".loading{display:none;text-align:center;color:#666;font-size:12px;margin:10px 0}"
".spinner{border:2px solid #f3f3f3;border-top:2px solid #007bff;border-radius:50%;width:20px;height:20px;animation:spin 1s linear infinite;margin:0 auto}"
"@keyframes spin{0%{transform:rotate(0deg)}100%{transform:rotate(360deg)}}"
".info{text-align:center;color:#666;font-size:12px;margin-top:20px}"
".toggle{font-size:12px;color:#007bff;cursor:pointer;text-decoration:underline;text-align:center;margin-top:10px}"
"</style>"
"</head>"
"<body>"
"<div class='container'>"
"<h1>Device Setup</h1>"
"<p style='text-align:center;color:#666'>Enter your WiFi network credentials</p>"
"<form method='POST' action='/configure'>"
"<div class='form-group'>"
"<label>WiFi Network Name (SSID):</label>"
"<input type='text' id='ssid' name='ssid' required placeholder='Select or type SSID' list='ssid_list'>"
"<datalist id='ssid_list'></datalist>"
"<select id='ssid_select' style='display:none;margin-top:8px' onchange=\"document.getElementById('ssid').value=this.value\"></select>"
"<div class='loading' id='scan-loading'><div class='spinner'></div><p>Scanning networks...</p></div>"
"</div>"
"<div class='form-group'>"
"<button type='button' id='scan-btn' onclick='scanNetworks()'>Scan Networks</button>"
"</div>"
"<div class='form-group'>"
"<label for='password'>WiFi Password:</label>"
"<input type='password' id='password' name='password' required placeholder='Your WiFi Password'>"
"</div>"
"<div class='form-group'>"
"<label for='user_name'>Name the device:</label>"
"<input type='text' id='user_name' name='user_name' required placeholder='Device name'>"
"</div>"
"<input type='submit' value='Connect'>"
"</form>"
"<div class='info'><p>Your device will restart and connect to the WiFi network.</p></div>"
"</div>"
"<script>"
"function scanNetworks(){var btn=document.getElementById('scan-btn');var loading=document.getElementById('scan-loading');var sel=document.getElementById('ssid_select');btn.disabled=true;loading.style.display='block';fetch('/scan_networks').then(r=>r.json()).then(networks=>{var datalist=document.getElementById('ssid_list');datalist.innerHTML='';sel.innerHTML='';if(networks.length>0){var ph=document.createElement('option');ph.value='';ph.text='Select a network';sel.appendChild(ph);sel.style.display='block';}else{sel.style.display='none';}networks.forEach(net=>{var opt=document.createElement('option');opt.value=net.ssid;datalist.appendChild(opt);var opt2=document.createElement('option');opt2.value=net.ssid;opt2.text=net.ssid;sel.appendChild(opt2)});loading.style.display='none';btn.disabled=false}).catch(e=>{console.error('Scan failed:',e);loading.style.display='none';btn.disabled=false})}"
"window.onload=function(){scanNetworks()};"
"</script>"
"</body>"
"</html>";

/**
 * @brief Success page HTML
 */
static const char success_html[] = "<!DOCTYPE html>"
"<html>"
"<head>"
"<meta charset='UTF-8'>"
"<meta name='viewport' content='width=device-width,initial-scale=1'>"
"<title>Setup Complete</title>"
"<style>"
"body{font-family:Arial,sans-serif;margin:0;padding:20px;background:#f0f0f0}"
".container{max-width:400px;margin:50px auto;background:#fff;padding:30px;border-radius:8px;box-shadow:0 2px 10px rgba(0,0,0,0.1)}"
"h1{color:#28a745;text-align:center}"
".checkmark{text-align:center;font-size:48px;color:#28a745}"
".info{text-align:center;color:#666;margin-top:20px}"
"</style>"
"</head>"
"<body>"
"<div class='container'>"
"<div class='checkmark'>✓</div>"
"<h1>Setup Complete!</h1>"
"<div class='info'>"
"<p>Your device has been configured and will now restart.</p>"
"<p>It should connect to your WiFi network shortly.</p>"
"</div>"
"</div>"
"</body>"
"</html>";

/**
 * @brief Handle GET request for provisioning page
 */
static esp_err_t provisioning_get_handler(httpd_req_t *req) {
    ESP_LOGI(TAG, "Serving provisioning page");
    
    httpd_resp_set_type(req, "text/html; charset=utf-8");
    httpd_resp_send(req, provisioning_html, strlen(provisioning_html));
    
    return ESP_OK;
}

/**
 * @brief Handle POST request for WiFi configuration
 */
static esp_err_t provisioning_post_handler(httpd_req_t *req) {
    char buf[512];
    int total_len = req->content_len;
    int received = 0;
    
    if (total_len >= sizeof(buf)) {
        ESP_LOGE(TAG, "Content length too large: %d", total_len);
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Content too large");
        return ESP_FAIL;
    }
    
    received = httpd_req_recv(req, buf, total_len);
    if (received != total_len) {
        ESP_LOGE(TAG, "Failed to receive complete data: %d != %d", received, total_len);
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Incomplete data");
        return ESP_FAIL;
    }
    
    buf[total_len] = '\0';
    ESP_LOGI(TAG, "Received data: %s", buf);
    
    // Parse form data (simple URL-encoded parser)
    char ssid[WIFI_SSID_MAX_LEN] = {0};
    char password[WIFI_PASS_MAX_LEN] = {0};
    char user_name[64] = {0};
    
    // Find ssid parameter
    char *ssid_start = strstr(buf, "ssid=");
    if (ssid_start) {
        ssid_start += 5;  // Skip "ssid="
        char *ssid_end = strchr(ssid_start, '&');
        if (!ssid_end) ssid_end = strchr(ssid_start, '\0');
        
        int ssid_len = ssid_end - ssid_start;
        if (ssid_len > 0 && ssid_len < WIFI_SSID_MAX_LEN) {
            strncpy(ssid, ssid_start, ssid_len);
            ssid[ssid_len] = '\0';
            
            // URL decode simple case (spaces as +)
            for (int i = 0; ssid[i]; i++) {
                if (ssid[i] == '+') ssid[i] = ' ';
            }
        }
    }
    
    // Find password parameter
    char *pass_start = strstr(buf, "password=");
    if (pass_start) {
        pass_start += 9;  // Skip "password="
        char *pass_end = strchr(pass_start, '&');
        if (!pass_end) pass_end = strchr(pass_start, '\0');
        
        int pass_len = pass_end - pass_start;
        if (pass_len > 0 && pass_len < WIFI_PASS_MAX_LEN) {
            strncpy(password, pass_start, pass_len);
            password[pass_len] = '\0';
            
            // URL decode simple case (spaces as +)
            for (int i = 0; password[i]; i++) {
                if (password[i] == '+') password[i] = ' ';
            }
        }
    }
    
    // Find user_name parameter
    char *name_start = strstr(buf, "user_name=");
    if (name_start) {
        name_start += 10;  // Skip "user_name="
        char *name_end = strchr(name_start, '&');
        if (!name_end) name_end = strchr(name_start, '\0');
        
        int name_len = name_end - name_start;
        if (name_len > 0 && name_len < sizeof(user_name)) {
            strncpy(user_name, name_start, name_len);
            user_name[name_len] = '\0';
            
            // URL decode simple case (spaces as +)
            for (int i = 0; user_name[i]; i++) {
                if (user_name[i] == '+') user_name[i] = ' ';
            }
        }
    }
    
    // Validate credentials
    if (strlen(ssid) == 0) {
        ESP_LOGE(TAG, "SSID is empty");
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "SSID cannot be empty");
        return ESP_FAIL;
    }
    
    if (strlen(password) == 0) {
        ESP_LOGE(TAG, "Password is empty");
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Password cannot be empty");
        return ESP_FAIL;
    }
    
    if (strlen(user_name) == 0) {
        ESP_LOGE(TAG, "Device name is empty");
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Device name cannot be empty");
        return ESP_FAIL;
    }
    
    // Save credentials using device config
    ESP_LOGI(TAG, "Saving WiFi credentials - SSID: %s", ssid);
    Device_Config__Set_WiFi_SSID(ssid);
    Device_Config__Set_WiFi_Password(password);
    Device_Config__Set_UserName(user_name);
    ESP_LOGI(TAG, "Device name set to: %s", user_name);
    
    // Send success page
    httpd_resp_set_type(req, "text/html; charset=utf-8");
    httpd_resp_send(req, success_html, strlen(success_html));
    
    // Signal provisioning complete
    if (provisioning_event_group) {
        xEventGroupSetBits(provisioning_event_group, PROVISIONING_COMPLETE_BIT);
    }
    
    return ESP_OK;
}

/**
 * @brief Scan WiFi networks and return JSON list of SSIDs
 */
static esp_err_t scan_networks_handler(httpd_req_t *req) {
    // Perform WiFi scan
    wifi_scan_config_t scan_config = {
        .ssid = NULL,
        .bssid = NULL,
        .channel = 0,
        .show_hidden = true,
        .scan_type = WIFI_SCAN_TYPE_ACTIVE,
        .scan_time = {
            .active = { .min = 100, .max = 300 },
            .passive = 0
        }
    };
    
    if (esp_wifi_scan_start(&scan_config, true) != ESP_OK) {
        ESP_LOGE(TAG, "WiFi scan failed");
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Scan failed");
        return ESP_FAIL;
    }
    
    // Get scan results
    uint16_t ap_count = 0;
    esp_wifi_scan_get_ap_num(&ap_count);
    
    if (ap_count == 0) {
        httpd_resp_set_type(req, "application/json");
        httpd_resp_send(req, "[]", 2);
        return ESP_OK;
    }
    
    // Allocate array for APs
    wifi_ap_record_t *ap_list = malloc(ap_count * sizeof(wifi_ap_record_t));
    if (!ap_list) {
        ESP_LOGE(TAG, "Failed to allocate AP list");
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Memory error");
        return ESP_FAIL;
    }
    
    esp_wifi_scan_get_ap_records(&ap_count, ap_list);
    
    // Sort by RSSI (signal strength) - strongest first
    // Simple bubble sort for small lists
    for (int i = 0; i < ap_count - 1; i++) {
        for (int j = 0; j < ap_count - i - 1; j++) {
            if (ap_list[j].rssi < ap_list[j + 1].rssi) {
                // Swap
                wifi_ap_record_t temp = ap_list[j];
                ap_list[j] = ap_list[j + 1];
                ap_list[j + 1] = temp;
            }
        }
    }
    
    // Build JSON response
    char *json_buf = malloc(4096);
    if (!json_buf) {
        free(ap_list);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Memory error");
        return ESP_FAIL;
    }
    
    int len = snprintf(json_buf, 4096, "[");
    int added = 0;
    
    // Add top 20 networks with strongest signals
    for (int i = 0; i < ap_count && added < 20; i++) {
        // Get SSID length (SSID is not null-terminated, max 32 bytes)
        int ssid_len = 0;
        for (int k = 0; k < 32; k++) {
            if (ap_list[i].ssid[k] == 0) break;
            ssid_len++;
        }
        
        // Skip empty SSIDs
        if (ssid_len == 0) continue;
        
        if (added > 0) len += snprintf(json_buf + len, 4096 - len, ",");
        
        // Sanitize SSID for JSON (escape quotes)
        char safe_ssid[33];
        int safe_len = 0;
        for (int j = 0; j < ssid_len && safe_len < 32; j++) {
            if (ap_list[i].ssid[j] == '"') {
                safe_ssid[safe_len++] = '\\';
            }
            safe_ssid[safe_len++] = ap_list[i].ssid[j];
        }
        safe_ssid[safe_len] = '\0';
        
        len += snprintf(json_buf + len, 4096 - len, 
                       "{\"ssid\":\"%s\",\"rssi\":%d,\"channel\":%d}",
                       safe_ssid, ap_list[i].rssi, ap_list[i].primary);
        added++;
    }
    
    len += snprintf(json_buf + len, 4096 - len, "]");
    
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, json_buf, len);
    
    free(json_buf);
    free(ap_list);
    
    return ESP_OK;
}

// Well-known endpoints to trigger captive portal on various OSes
// Android: /generate_204 or /gen_204 (return 200 to force sign-in UI)
static esp_err_t android_generate_204_handler(httpd_req_t *req) {
    httpd_resp_set_status(req, "200 OK");
    httpd_resp_set_type(req, "text/plain");
    const char *body = "OK";
    httpd_resp_send(req, body, strlen(body));
    return ESP_OK;
}

// iOS/macOS: /hotspot-detect.html
static esp_err_t apple_hotspot_handler(httpd_req_t *req) {
    httpd_resp_set_status(req, "200 OK");
    httpd_resp_set_type(req, "text/html");
    const char *html = "<HTML><HEAD><TITLE>Success</TITLE></HEAD><BODY>Success</BODY></HTML>";
    httpd_resp_send(req, html, strlen(html));
    return ESP_OK;
}

// Windows: /ncsi.txt
static esp_err_t windows_ncsi_handler(httpd_req_t *req) {
    httpd_resp_set_status(req, "200 OK");
    httpd_resp_set_type(req, "text/plain");
    const char *txt = "Microsoft NCSI";
    httpd_resp_send(req, txt, strlen(txt));
    return ESP_OK;
}

/**
 * @brief Catch-all handler for captive portal (DNS redirection)
 * Any unknown request redirects to the provisioning page
 */
static esp_err_t captive_portal_handler(httpd_req_t *req) {
    // Redirect to main provisioning page
    httpd_resp_set_status(req, "302 Found");
    httpd_resp_set_hdr(req, "Location", "/");
    httpd_resp_send(req, NULL, 0);
    return ESP_OK;
}

/**
 * @brief Start HTTP server for provisioning
 */
static esp_err_t start_provisioning_server(void) {
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = 80;
    config.uri_match_fn = httpd_uri_match_wildcard;
    config.lru_purge_enable = true;
    config.max_uri_handlers = 14;
    
    ESP_LOGI(TAG, "Starting provisioning HTTP server on port %d", config.server_port);
    
    if (httpd_start(&provisioning_server, &config) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start HTTP server");
        return ESP_FAIL;
    }
    
    // Register URI handlers
    httpd_uri_t get_uri = {
        .uri = "/",
        .method = HTTP_GET,
        .handler = provisioning_get_handler,
        .user_ctx = NULL
    };
    httpd_register_uri_handler(provisioning_server, &get_uri);
    
    httpd_uri_t post_uri = {
        .uri = "/configure",
        .method = HTTP_POST,
        .handler = provisioning_post_handler,
        .user_ctx = NULL
    };
    httpd_register_uri_handler(provisioning_server, &post_uri);
    
    // WiFi scan endpoint
    httpd_uri_t scan_uri = {
        .uri = "/scan_networks",
        .method = HTTP_GET,
        .handler = scan_networks_handler,
        .user_ctx = NULL
    };
    httpd_register_uri_handler(provisioning_server, &scan_uri);
    
    // Catch-all for captive portal
    httpd_uri_t catchall_uri = {
        .uri = "/*",
        .method = HTTP_GET,
        .handler = captive_portal_handler,
        .user_ctx = NULL
    };
    httpd_register_uri_handler(provisioning_server, &catchall_uri);

    // Well-known captive portal endpoints
    httpd_uri_t gen204_uri = {
        .uri = "/generate_204",
        .method = HTTP_GET,
        .handler = android_generate_204_handler,
        .user_ctx = NULL
    };
    httpd_register_uri_handler(provisioning_server, &gen204_uri);

    httpd_uri_t gen204_uri2 = {
        .uri = "/gen_204",
        .method = HTTP_GET,
        .handler = android_generate_204_handler,
        .user_ctx = NULL
    };
    httpd_register_uri_handler(provisioning_server, &gen204_uri2);

    httpd_uri_t hotspot_uri = {
        .uri = "/hotspot-detect.html",
        .method = HTTP_GET,
        .handler = apple_hotspot_handler,
        .user_ctx = NULL
    };
    httpd_register_uri_handler(provisioning_server, &hotspot_uri);

    httpd_uri_t ncsi_uri = {
        .uri = "/ncsi.txt",
        .method = HTTP_GET,
        .handler = windows_ncsi_handler,
        .user_ctx = NULL
    };
    httpd_register_uri_handler(provisioning_server, &ncsi_uri);
    
    return ESP_OK;
}

/**
 * @brief Event handler for AP mode
 */
static void softap_event_handler(void* arg, esp_event_base_t event_base,
                                int32_t event_id, void* event_data) {
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_AP_STACONNECTED) {
        wifi_event_ap_staconnected_t* event = (wifi_event_ap_staconnected_t*) event_data;
        ESP_LOGI(TAG, "Client connected to SoftAP: MAC %02x:%02x:%02x:%02x:%02x:%02x",
                 event->mac[0], event->mac[1], event->mac[2],
                 event->mac[3], event->mac[4], event->mac[5]);
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_AP_STADISCONNECTED) {
        wifi_event_ap_stadisconnected_t* event = (wifi_event_ap_stadisconnected_t*) event_data;
        ESP_LOGI(TAG, "Client disconnected from SoftAP: MAC %02x:%02x:%02x:%02x:%02x:%02x",
                 event->mac[0], event->mac[1], event->mac[2],
                 event->mac[3], event->mac[4], event->mac[5]);
    }
}

/**
 * @brief Start WiFi in AP mode
 */
static esp_err_t start_softap(void) {
    ESP_LOGI(TAG, "Starting SoftAP: %s", SOFTAP_SSID);

    // Initialize TCP/IP stack (ignore if already initialized)
    esp_err_t err = esp_netif_init();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "esp_netif_init failed: %s", esp_err_to_name(err));
        return err;
    }

    // Create event loop if not exists
    err = esp_event_loop_create_default();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "esp_event_loop_create_default failed: %s", esp_err_to_name(err));
        return err;
    }

    // Create WiFi AP interface (AP only; STA already exists from main WiFi init)
    static bool ap_created = false;
    if (!ap_created) {
        if (esp_netif_create_default_wifi_ap() == NULL) {
            ESP_LOGE(TAG, "Failed to create default WiFi AP netif");
            return ESP_FAIL;
        }
        ap_created = true;
    }

    // Initialize WiFi (ignore if already initialized)
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    err = esp_wifi_init(&cfg);
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "esp_wifi_init failed: %s", esp_err_to_name(err));
        return err;
    }
    
    // Register event handler
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &softap_event_handler,
                                                        NULL,
                                                        NULL));
    
    // Configure AP
    wifi_config_t wifi_config = {
        .ap = {
            .ssid = SOFTAP_SSID,
            .ssid_len = strlen(SOFTAP_SSID),
            .password = SOFTAP_PASS,
            .channel = SOFTAP_CHANNEL,
            .max_connection = SOFTAP_MAX_CONNECTIONS,
            .authmode = WIFI_AUTH_WPA2_PSK,
        },
    };
    
    // Use AP+STA mode so we can scan nearby networks while AP is running
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
    
    ESP_LOGI(TAG, "SoftAP started. SSID: %s Password: %s", SOFTAP_SSID, SOFTAP_PASS);
    ESP_LOGI(TAG, "Connect to the network and open http://192.168.4.1 in your browser");
    
    return ESP_OK;
}

int Provisioning__Start(void) {
    if (provisioning_active) {
        ESP_LOGI(TAG, "Provisioning already active");
        return 0;
    }
    
    // Create event group for provisioning completion
    provisioning_event_group = xEventGroupCreate();
    if (!provisioning_event_group) {
        ESP_LOGE(TAG, "Failed to create event group");
        return -1;
    }

    // Mark active early to prevent re-entrancy
    provisioning_active = true;
    
    // Start SoftAP
    if (start_softap() != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start SoftAP");
        provisioning_active = false;
        vEventGroupDelete(provisioning_event_group);
        provisioning_event_group = NULL;
        return -1;
    }
    
    // Start DNS captive portal server (resolve all hostnames to AP IP)
    if (dns_server_start() != ESP_OK) {
        ESP_LOGW(TAG, "Failed to start DNS captive portal server");
    }

    // Start HTTP server
    if (start_provisioning_server() != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start provisioning server");
        // Cleanup started subsystems
        esp_wifi_stop();
        esp_wifi_deinit();
        dns_server_stop();
        vEventGroupDelete(provisioning_event_group);
        provisioning_event_group = NULL;
        provisioning_active = false;
        return -1;
    }
    
    // Wait for provisioning to complete
    ESP_LOGI(TAG, "Waiting for provisioning...");
    EventBits_t bits = xEventGroupWaitBits(provisioning_event_group,
                                          PROVISIONING_COMPLETE_BIT,
                                          pdFALSE,
                                          pdFALSE,
                                          portMAX_DELAY);
    
    if (bits & PROVISIONING_COMPLETE_BIT) {
        ESP_LOGI(TAG, "Provisioning completed successfully");
        vTaskDelay(pdMS_TO_TICKS(2000));  // Give time for form to close
        Provisioning__Stop();
        return 0;
    }
    
    // Safety cleanup if wait returns unexpectedly
    Provisioning__Stop();
    return -1;
}

void Provisioning__Stop(void) {
    if (!provisioning_active) {
        return;
    }
    
    // Stop HTTP server
    if (provisioning_server) {
        httpd_stop(provisioning_server);
        provisioning_server = NULL;
    }
    
    // Stop WiFi AP
    esp_wifi_stop();
    esp_wifi_deinit();
    
    // Stop DNS server
    dns_server_stop();

    // Clean up event group
    if (provisioning_event_group) {
        vEventGroupDelete(provisioning_event_group);
        provisioning_event_group = NULL;
    }
    
    provisioning_active = false;
    ESP_LOGI(TAG, "Provisioning stopped");
}

int Provisioning__Is_Active(void) {
    return provisioning_active ? 1 : 0;
}

// -----------------------
// Simple DNS captive portal
// -----------------------

#pragma pack(push, 1)
typedef struct {
    uint16_t id;
    uint16_t flags;
    uint16_t qdcount;
    uint16_t ancount;
    uint16_t nscount;
    uint16_t arcount;
} dns_header_t;
#pragma pack(pop)

// Minimal DNS response: answer A record pointing to AP IP
static void dns_recv(void *arg, struct udp_pcb *pcb, struct pbuf *p, const ip_addr_t *addr, u16_t port) {
    if (!p) return;
    if (p->len < sizeof(dns_header_t)) {
        pbuf_free(p);
        return;
    }

    dns_header_t hdr;
    pbuf_copy_partial(p, &hdr, sizeof(hdr), 0);
    uint16_t id = hdr.id;

    // Build response into a new pbuf
    // Copy entire query to response, then append one A answer
    struct pbuf *resp = pbuf_alloc(PBUF_TRANSPORT, p->tot_len + 16, PBUF_RAM);
    if (!resp) {
        pbuf_free(p);
        return;
    }
    pbuf_copy(resp, p);

    // Set header in response: standard response, no error, 1 answer
    dns_header_t rhdr;
    pbuf_copy_partial(resp, &rhdr, sizeof(rhdr), 0);
    rhdr.id = id;
    rhdr.flags = PP_HTONS(0x8180);  // QR=1, RD=1, RA=1, RCODE=0
    rhdr.ancount = PP_HTONS(1);
    pbuf_take(resp, &rhdr, sizeof(rhdr));

    // Append answer: name pointer to question (0xC00C)
    uint8_t ans[16];
    size_t off = 0;
    ans[off++] = 0xC0; ans[off++] = 0x0C;   // Name pointer
    ans[off++] = 0x00; ans[off++] = 0x01;   // Type A
    ans[off++] = 0x00; ans[off++] = 0x01;   // Class IN
    ans[off++] = 0x00; ans[off++] = 0x00; ans[off++] = 0x00; ans[off++] = 0x3C; // TTL 60s
    ans[off++] = 0x00; ans[off++] = 0x04;   // RDLENGTH 4
    // RDATA (AP IPv4)
    uint8_t *ipb = (uint8_t *)&ap_ip_addr.addr;
    ans[off++] = ipb[0]; ans[off++] = ipb[1]; ans[off++] = ipb[2]; ans[off++] = ipb[3];

    pbuf_take_at(resp, ans, off, p->tot_len);

    udp_sendto(pcb, resp, addr, port);
    pbuf_free(resp);
    pbuf_free(p);
}

static esp_err_t dns_server_start(void) {
    // Get AP IP (default 192.168.4.1)
    esp_netif_ip_info_t ip_info;
    esp_netif_t *ap = esp_netif_get_handle_from_ifkey("WIFI_AP_DEF");
    if (ap && esp_netif_get_ip_info(ap, &ip_info) == ESP_OK) {
        ap_ip_addr.addr = ip_info.ip.addr;
    } else {
        IP4_ADDR(&ap_ip_addr, 192, 168, 4, 1);
    }

    dns_pcb = udp_new_ip_type(IPADDR_TYPE_ANY);
    if (!dns_pcb) {
        ESP_LOGE(TAG, "Failed to create DNS pcb");
        return ESP_FAIL;
    }
    err_t err = udp_bind(dns_pcb, IP_ANY_TYPE, 53);
    if (err != ERR_OK) {
        ESP_LOGE(TAG, "DNS bind failed: %d", err);
        udp_remove(dns_pcb);
        dns_pcb = NULL;
        return ESP_FAIL;
    }
    udp_recv(dns_pcb, dns_recv, NULL);
    ESP_LOGI(TAG, "DNS captive portal server started on port 53, IP=%s", ip4addr_ntoa(&ap_ip_addr));
    return ESP_OK;
}

static void dns_server_stop(void) {
    if (dns_pcb) {
        udp_remove(dns_pcb);
        dns_pcb = NULL;
        ESP_LOGI(TAG, "DNS captive portal server stopped");
    }
}
