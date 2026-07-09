#include "espnow_manager.h"

bool espnow_manager_init(void) #test
{
    return true;
}

void espnow_manager_task(void)
{
}

bool espnow_manager_send(const uint8_t *mac,
                         const char *message)
{
    (void)mac;
    (void)message;
    return false;
}

bool espnow_manager_broadcast(const char *message)
{
    (void)message;
    return false;
}

void espnow_manager_set_callback(
    espnow_rx_callback_t cb)
{
    (void)cb;
}
