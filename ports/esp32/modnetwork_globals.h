{ MP_ROM_QSTR(MP_QSTR___init__), MP_ROM_PTR(&esp_network_initialize_obj) },

#if MICROPY_PY_NETWORK_WLAN
{ MP_ROM_QSTR(MP_QSTR_WLAN), MP_ROM_PTR(&esp_network_wlan_type) },
#endif

#if MICROPY_PY_NETWORK_LAN
{ MP_ROM_QSTR(MP_QSTR_LAN), MP_ROM_PTR(&esp_network_get_lan_obj) },
#endif
{ MP_ROM_QSTR(MP_QSTR_PPP), MP_ROM_PTR(&esp_network_ppp_make_new_obj) },
{ MP_ROM_QSTR(MP_QSTR_phy_mode), MP_ROM_PTR(&esp_network_phy_mode_obj) },

#if MICROPY_PY_NETWORK_WLAN
{ MP_ROM_QSTR(MP_QSTR_STA_IF), MP_ROM_INT(WIFI_IF_STA)},
{ MP_ROM_QSTR(MP_QSTR_AP_IF), MP_ROM_INT(WIFI_IF_AP)},

{ MP_ROM_QSTR(MP_QSTR_MODE_11B), MP_ROM_INT(WIFI_PROTOCOL_11B) },
{ MP_ROM_QSTR(MP_QSTR_MODE_11G), MP_ROM_INT(WIFI_PROTOCOL_11G) },
{ MP_ROM_QSTR(MP_QSTR_MODE_11N), MP_ROM_INT(WIFI_PROTOCOL_11N) },
{ MP_ROM_QSTR(MP_QSTR_MODE_LR), MP_ROM_INT(WIFI_PROTOCOL_LR) },

{ MP_ROM_QSTR(MP_QSTR_AUTH_OPEN), MP_ROM_INT(WIFI_AUTH_OPEN) },
{ MP_ROM_QSTR(MP_QSTR_AUTH_WEP), MP_ROM_INT(WIFI_AUTH_WEP) },
{ MP_ROM_QSTR(MP_QSTR_AUTH_WPA_PSK), MP_ROM_INT(WIFI_AUTH_WPA_PSK) },
{ MP_ROM_QSTR(MP_QSTR_AUTH_WPA2_PSK), MP_ROM_INT(WIFI_AUTH_WPA2_PSK) },
{ MP_ROM_QSTR(MP_QSTR_AUTH_WPA_WPA2_PSK), MP_ROM_INT(WIFI_AUTH_WPA_WPA2_PSK) },
{ MP_ROM_QSTR(MP_QSTR_AUTH_WPA2_ENTERPRISE), MP_ROM_INT(WIFI_AUTH_WPA2_ENTERPRISE) },
{ MP_ROM_QSTR(MP_QSTR_AUTH_WPA3_PSK), MP_ROM_INT(WIFI_AUTH_WPA3_PSK) },
{ MP_ROM_QSTR(MP_QSTR_AUTH_WPA2_WPA3_PSK), MP_ROM_INT(WIFI_AUTH_WPA2_WPA3_PSK) },
{ MP_ROM_QSTR(MP_QSTR_AUTH_WAPI_PSK), MP_ROM_INT(WIFI_AUTH_WAPI_PSK) },
{ MP_ROM_QSTR(MP_QSTR_AUTH_OWE), MP_ROM_INT(WIFI_AUTH_OWE) },
#if ESP_IDF_VERSION > ESP_IDF_VERSION_VAL(5, 1, 1)
{ MP_ROM_QSTR(MP_QSTR_AUTH_WPA3_ENT_192), MP_ROM_INT(WIFI_AUTH_WPA3_ENT_192) },
#endif
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 2, 0)
{ MP_ROM_QSTR(MP_QSTR_AUTH_WPA3_EXT_PSK), MP_ROM_INT(WIFI_AUTH_WPA3_EXT_PSK) },
#endif
{ MP_ROM_QSTR(MP_QSTR_AUTH_MAX), MP_ROM_INT(WIFI_AUTH_MAX) },
#endif

#if MICROPY_PY_NETWORK_LAN
{ MP_ROM_QSTR(MP_QSTR_PHY_LAN8710), MP_ROM_INT(PHY_LAN8710) },
{ MP_ROM_QSTR(MP_QSTR_PHY_LAN8720), MP_ROM_INT(PHY_LAN8720) },
{ MP_ROM_QSTR(MP_QSTR_PHY_IP101), MP_ROM_INT(PHY_IP101) },
{ MP_ROM_QSTR(MP_QSTR_PHY_RTL8201), MP_ROM_INT(PHY_RTL8201) },
{ MP_ROM_QSTR(MP_QSTR_PHY_DP83848), MP_ROM_INT(PHY_DP83848) },
{ MP_ROM_QSTR(MP_QSTR_PHY_KSZ8041), MP_ROM_INT(PHY_KSZ8041) },
{ MP_ROM_QSTR(MP_QSTR_PHY_KSZ8081), MP_ROM_INT(PHY_KSZ8081) },

#if CONFIG_ETH_SPI_ETHERNET_KSZ8851SNL
{ MP_ROM_QSTR(MP_QSTR_PHY_KSZ8851SNL), MP_ROM_INT(PHY_KSZ8851SNL) },
#endif
#if CONFIG_ETH_SPI_ETHERNET_DM9051
{ MP_ROM_QSTR(MP_QSTR_PHY_DM9051), MP_ROM_INT(PHY_DM9051) },
#endif
#if CONFIG_ETH_SPI_ETHERNET_W5500
{ MP_ROM_QSTR(MP_QSTR_PHY_W5500), MP_ROM_INT(PHY_W5500) },
#endif

{ MP_ROM_QSTR(MP_QSTR_ETH_INITIALIZED), MP_ROM_INT(ETH_INITIALIZED)},
{ MP_ROM_QSTR(MP_QSTR_ETH_STARTED), MP_ROM_INT(ETH_STARTED)},
{ MP_ROM_QSTR(MP_QSTR_ETH_STOPPED), MP_ROM_INT(ETH_STOPPED)},
{ MP_ROM_QSTR(MP_QSTR_ETH_CONNECTED), MP_ROM_INT(ETH_CONNECTED)},
{ MP_ROM_QSTR(MP_QSTR_ETH_DISCONNECTED), MP_ROM_INT(ETH_DISCONNECTED)},
{ MP_ROM_QSTR(MP_QSTR_ETH_GOT_IP), MP_ROM_INT(ETH_GOT_IP)},
#endif

{ MP_ROM_QSTR(MP_QSTR_STAT_IDLE), MP_ROM_INT(STAT_IDLE)},
{ MP_ROM_QSTR(MP_QSTR_STAT_CONNECTING), MP_ROM_INT(STAT_CONNECTING)},
{ MP_ROM_QSTR(MP_QSTR_STAT_GOT_IP), MP_ROM_INT(STAT_GOT_IP)},
// Errors from the ESP-IDF
{ MP_ROM_QSTR(MP_QSTR_STAT_NO_AP_FOUND), MP_ROM_INT(WIFI_REASON_NO_AP_FOUND)},
{ MP_ROM_QSTR(MP_QSTR_STAT_WRONG_PASSWORD), MP_ROM_INT(WIFI_REASON_AUTH_FAIL)},
{ MP_ROM_QSTR(MP_QSTR_STAT_BEACON_TIMEOUT), MP_ROM_INT(WIFI_REASON_BEACON_TIMEOUT)},
{ MP_ROM_QSTR(MP_QSTR_STAT_ASSOC_FAIL), MP_ROM_INT(WIFI_REASON_ASSOC_FAIL)},
{ MP_ROM_QSTR(MP_QSTR_STAT_HANDSHAKE_TIMEOUT), MP_ROM_INT(WIFI_REASON_HANDSHAKE_TIMEOUT)},
