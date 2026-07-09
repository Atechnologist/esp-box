#!/bin/bash
# Generate modular patch files for ESP32-S3-BOX Industrial Hub
# This script creates the patches/ directory structure with all components
# Suggested by githubai
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(dirname "$SCRIPT_DIR")"
PATCHES_DIR="$REPO_ROOT/patches"

echo "📦 Generating ESP32-S3-BOX patches..."
echo "Target directory: $PATCHES_DIR"

# Create directory structure
mkdir -p "$PATCHES_DIR"/{core,hardware,services,ai}/{,subdir}

# ================================================================
# CORE PATCHES
# ================================================================

# 1. espnow_manager
mkdir -p "$PATCHES_DIR/core/espnow_manager"
cat > "$PATCHES_DIR/core/espnow_manager/espnow_relay.h" << 'EOF'
#pragma once

void espnow_relay_init(void);
void espnow_relay_broadcast(const char *msg);
EOF

cat > "$PATCHES_DIR/core/espnow_manager/espnow_relay.c" << 'EOF'
#include "espnow_relay.h"
#include "relay_state.h"
#include "node_registry.h"
#include "web_server.h"
#include "esp_now.h"
#include "esp_wifi.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

static const char *TAG = "ESPNOW";
static const uint8_t BCAST[6] = {0xff,0xff,0xff,0xff,0xff,0xff};
static bool s_ready = false;

static void mac_str(const uint8_t *m, char *out, size_t len)
{
    snprintf(out, len, "%02X:%02X:%02X:%02X:%02X:%02X",
             m[0],m[1],m[2],m[3],m[4],m[5]);
}

static void recv_cb(const esp_now_recv_info_t *info,
                    const uint8_t *data, int len)
{
    char msg[128] = {0};
    int n = len < (int)sizeof(msg)-1 ? len : (int)sizeof(msg)-1;
    memcpy(msg, data, n);
    
    char mac[24];
    mac_str(info->src_addr, mac, sizeof(mac));
    ESP_LOGI(TAG, "RX [%s] %s", mac, msg);
}

static void send_cb(const uint8_t *mac, esp_now_send_status_t status)
{
    if (status != ESP_NOW_SEND_SUCCESS)
        ESP_LOGW(TAG, "TX FAIL %02X:%02X:%02X:%02X:%02X:%02X",
                 mac[0],mac[1],mac[2],mac[3],mac[4],mac[5]);
}

static void espnow_init_task(void *arg)
{
    uint8_t primary = 0;
    wifi_second_chan_t second;
    while (primary == 0) {
        vTaskDelay(pdMS_TO_TICKS(500));
        esp_wifi_get_channel(&primary, &second);
    }
    
    if (esp_now_init() != ESP_OK) {
        ESP_LOGE(TAG, "esp_now_init failed");
        vTaskDelete(NULL);
        return;
    }
    
    esp_now_register_recv_cb(recv_cb);
    esp_now_register_send_cb(send_cb);
    
    esp_now_peer_info_t peer = {0};
    memcpy(peer.peer_addr, BCAST, 6);
    peer.channel = 0;
    peer.encrypt = false;
    esp_now_add_peer(&peer);
    
    s_ready = true;
    ESP_LOGI(TAG, "ESP-NOW ready");
    vTaskDelete(NULL);
}

void espnow_relay_init(void)
{
    xTaskCreate(espnow_init_task, "espnow_init", 4096, NULL, 5, NULL);
}

void espnow_relay_broadcast(const char *msg)
{
    if (!s_ready || !msg) return;
    esp_now_send(BCAST, (const uint8_t *)msg, strlen(msg) + 1);
}
EOF

# 2. command_dispatcher
mkdir -p "$PATCHES_DIR/core/command_dispatcher"
cat > "$PATCHES_DIR/core/command_dispatcher/command_dispatcher.h" << 'EOF'
#pragma once

typedef void (*command_cb_t)(const char *cmd, const char *arg);

void command_dispatcher_init(void);
void command_dispatcher_register(const char *cmd, command_cb_t cb);
void command_dispatcher_execute(const char *msg);
EOF

cat > "$PATCHES_DIR/core/command_dispatcher/command_dispatcher.c" << 'EOF'
#include "command_dispatcher.h"
#include "esp_log.h"
#include <string.h>
#include <stdlib.h>

static const char *TAG = "CMD_DISP";
#define MAX_HANDLERS 16

typedef struct {
    char cmd[32];
    command_cb_t cb;
} handler_t;

static handler_t handlers[MAX_HANDLERS];
static int handler_count = 0;

void command_dispatcher_init(void)
{
    handler_count = 0;
    ESP_LOGI(TAG, "Command dispatcher initialized");
}

void command_dispatcher_register(const char *cmd, command_cb_t cb)
{
    if (handler_count >= MAX_HANDLERS) {
        ESP_LOGW(TAG, "Handler limit reached");
        return;
    }
    strncpy(handlers[handler_count].cmd, cmd, sizeof(handlers[0].cmd)-1);
    handlers[handler_count].cb = cb;
    handler_count++;
    ESP_LOGI(TAG, "Registered: %s", cmd);
}

void command_dispatcher_execute(const char *msg)
{
    if (!msg) return;
    
    char cmd_copy[128];
    strncpy(cmd_copy, msg, sizeof(cmd_copy)-1);
    
    char *colon = strchr(cmd_copy, ':');
    if (!colon) return;
    
    *colon = '\0';
    char *cmd = cmd_copy;
    char *arg = colon + 1;
    
    for (int i = 0; i < handler_count; i++) {
        if (strcmp(handlers[i].cmd, cmd) == 0) {
            handlers[i].cb(cmd, arg);
            return;
        }
    }
    
    ESP_LOGD(TAG, "Unknown command: %s", cmd);
}
EOF

# 3. node_registry
mkdir -p "$PATCHES_DIR/core/node_registry"
cat > "$PATCHES_DIR/core/node_registry/node_registry.h" << 'EOF'
#pragma once

void node_registry_init(void);
void node_registry_add(const char *node);
const char **node_registry_get_all(int *count);
int node_registry_count(void);
EOF

cat > "$PATCHES_DIR/core/node_registry/node_registry.c" << 'EOF'
#include "node_registry.h"
#include "esp_log.h"
#include <string.h>

#define MAX_NODES 20
static char nodes[MAX_NODES][32];
static const char *ptrs[MAX_NODES];
static int n_count = 0;

void node_registry_init(void)
{
    memset(nodes, 0, sizeof(nodes));
    n_count = 0;
}

void node_registry_add(const char *node)
{
    if (!node || node[0] == '\0') return;
    for (int i = 0; i < n_count; i++)
        if (strcmp(nodes[i], node) == 0) return;
    if (n_count >= MAX_NODES) return;
    strncpy(nodes[n_count], node, 31);
    ptrs[n_count] = nodes[n_count];
    n_count++;
    ESP_LOGI("NODE_REG", "Added: %s", node);
}

const char **node_registry_get_all(int *c)
{
    if (c) *c = n_count;
    return ptrs;
}

int node_registry_count(void) { return n_count; }
EOF

# 4. logger
mkdir -p "$PATCHES_DIR/core/logger"
cat > "$PATCHES_DIR/core/logger/logger.h" << 'EOF'
#pragma once

void logger_init(void);
void logger_log(const char *msg);
const char *logger_get_buffer(void);
EOF

cat > "$PATCHES_DIR/core/logger/logger.c" << 'EOF'
#include "logger.h"
#include <string.h>
#include <stdio.h>

#define LOG_SIZE 4096
static char logbuf[LOG_SIZE] = "-- log start --\n";

void logger_init(void)
{
    logbuf[0] = '\0';
    strcat(logbuf, "-- log start --\n");
}

void logger_log(const char *msg)
{
    if (!msg) return;
    size_t cur = strlen(logbuf);
    size_t mlen = strlen(msg);
    
    if (cur + mlen + 2 >= LOG_SIZE) {
        size_t drop = mlen + 2;
        if (drop >= cur) drop = cur;
        memmove(logbuf, logbuf + drop, cur - drop + 1);
        cur = strlen(logbuf);
    }
    strncat(logbuf, msg, LOG_SIZE - cur - 2);
    strcat(logbuf, "\n");
}

const char *logger_get_buffer(void)
{
    return logbuf;
}
EOF

# ================================================================
# HARDWARE PATCHES
# ================================================================

mkdir -p "$PATCHES_DIR/hardware/relay_manager"
cat > "$PATCHES_DIR/hardware/relay_manager/relay_state.h" << 'EOF'
#pragma once
#include <stdbool.h>

typedef enum {
    SRC_VOICE  = 0,
    SRC_WEB    = 1,
    SRC_ESPNOW = 2,
    SRC_BOXUI  = 3,
} relay_src_t;

void relay_state_init(void);
void relay_state_set_light(bool on, relay_src_t src);
void relay_state_set_relay(bool on, relay_src_t src);
void relay_state_toggle_relay(relay_src_t src);
bool relay_state_get_light(void);
bool relay_state_get_relay(void);
EOF

cat > "$PATCHES_DIR/hardware/relay_manager/relay_state.c" << 'EOF'
#include "relay_state.h"
#include "esp_log.h"
#include <stdio.h>

static const char *TAG = "RELAY_STATE";
static bool s_light = false;
static bool s_relay = false;

void relay_state_init(void)
{
    s_light = false;
    s_relay = false;
    ESP_LOGI(TAG, "State manager ready");
}

void relay_state_set_light(bool on, relay_src_t src)
{
    s_light = on;
    const char *who[] = {"VOICE", "WEB", "ESPNOW", "BOXUI"};
    ESP_LOGI(TAG, "[%s] LIGHT %s", who[src], on ? "ON" : "OFF");
}

void relay_state_set_relay(bool on, relay_src_t src)
{
    s_relay = on;
    const char *who[] = {"VOICE", "WEB", "ESPNOW", "BOXUI"};
    ESP_LOGI(TAG, "[%s] RELAY %s", who[src], on ? "ON" : "OFF");
}

void relay_state_toggle_relay(relay_src_t src)
{
    relay_state_set_relay(!s_relay, src);
}

bool relay_state_get_light(void) { return s_light; }
bool relay_state_get_relay(void) { return s_relay; }
EOF

# Create placeholder files for other hardware components
for component in audio_manager light_manager touch_manager; do
    mkdir -p "$PATCHES_DIR/hardware/$component"
    touch "$PATCHES_DIR/hardware/$component/README.md"
    echo "# $component" > "$PATCHES_DIR/hardware/$component/README.md"
done

# ================================================================
# SERVICES PATCHES
# ================================================================

for service in webui websocket ota; do
    mkdir -p "$PATCHES_DIR/services/$service"
    touch "$PATCHES_DIR/services/$service/README.md"
    echo "# $service" > "$PATCHES_DIR/services/$service/README.md"
done

# ================================================================
# AI PATCHES
# ================================================================

for ai in offline online; do
    mkdir -p "$PATCHES_DIR/ai/$ai"
    touch "$PATCHES_DIR/ai/$ai/README.md"
    echo "# $ai" > "$PATCHES_DIR/ai/$ai/README.md"
done

echo ""
echo "✅ Patch structure created successfully!"
echo ""
echo "📁 Directory structure:"
tree -L 3 "$PATCHES_DIR" 2>/dev/null || find "$PATCHES_DIR" -type d | head -20
echo ""
echo "✨ You can now use patches in your workflow with cp -r patches/core/* examples/"
