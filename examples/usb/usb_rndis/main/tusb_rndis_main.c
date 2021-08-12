/* 
 * The MIT License (MIT)
 *
 * Copyright (c) 2020 Peter Lawrence
 *
 * influenced by lrndis https://github.com/fetisov/lrndis
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 */

/*
this appears as either a RNDIS or CDC-ECM USB virtual network adapter; the OS picks its preference

RNDIS should be valid on Linux and Windows hosts, and CDC-ECM should be valid on Linux and macOS hosts

The MCU appears to the host as IP address 192.168.7.1, and provides a DHCP server, DNS server, and web server.
*/
/*
Some smartphones *may* work with this implementation as well, but likely have limited (broken) drivers,
and likely their manufacturer has not tested such functionality.  Some code workarounds could be tried:

The smartphone may only have an ECM driver, but refuse to automatically pick ECM (unlike the OSes above);
try modifying ./examples/devices/net_lwip_webserver/usb_descriptors.c so that CONFIG_ID_ECM is default.

The smartphone may be artificially picky about which Ethernet MAC address to recognize; if this happens, 
try changing the first byte of tud_network_mac_address[] below from 0x02 to 0x00 (clearing bit 1).
*/

// #include "bsp/board.h"
// #include "tusb.h"
#include "esp_err.h"
#include "tinyusb.h"
#include "tusb_cdc_acm.h"

// #include "dhserver.h"
// #include "dnserver.h"
// #include "lwip/init.h"
// #include "lwip/timeouts.h"
// #include "httpd.h"

#include "lwip/ip_addr.h"
#include "lwip/netif.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_private/wifi.h"

#include "nvs_flash.h"

#include "semphr.h"

#ifdef CONFIG_HEAP_TRACING
#include "esp_heap_trace.h"
#endif

#include "tusb.h"

#define ADD_BLE_HCI

#ifdef ADD_BLE_HCI
#include "esp_bt.h"
#include "bt_hci_common.h"
#endif
static char* TAG = "esp_rndis";
/* lwip context */
// static struct netif netif_data;

/* shared between tud_network_recv_cb() and service_traffic() */

/* this is used by this code, ./class/net/net_driver.c, and usb_descriptors.c */
/* ideally speaking, this should be generated from the hardware's unique ID (if available) */
/* it is suggested that the first byte is 0x02 to indicate a link-local address */
uint8_t tud_network_mac_address[6] = {0x02,0x02,0x84,0x6A,0x96,0x00};

#ifdef CONFIG_HEAP_TRACING
#define NUM_RECORDS 100
static heap_trace_record_t trace_record[NUM_RECORDS]; // This buffer must be in internal RAM
#endif
/* 
 * The MIT License (MIT)
 *
 * Copyright (c) 2019 Ha Thach (tinyusb.org)
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 */

#include "tusb.h"

/* A combination of interfaces must have a unique product id, since PC will save device driver after the first plug.
 * Same VID/PID with different interface e.g MSC (first), then CDC (later) will possibly cause system error on PC.
 *
 * Auto ProductID layout's Bitmap:
 *   [MSB]       NET | VENDOR | MIDI | HID | MSC | CDC          [LSB]
 */
#define _PID_MAP(itf, n)  ( (CFG_TUD_##itf) << (n) )
#define USB_PID           (0x4000 | _PID_MAP(CDC, 0) | _PID_MAP(MSC, 1) | _PID_MAP(HID, 2) | \
                           _PID_MAP(MIDI, 3) | _PID_MAP(VENDOR, 4) | _PID_MAP(NET, 5) )

// String Descriptor Index
enum
{
  STRID_LANGID = 0,
  STRID_MANUFACTURER,
  STRID_PRODUCT,
  STRID_SERIAL,
  STRID_INTERFACE,
  STRID_MAC,
  STRID_CDC,
};

enum
{
  ITF_NUM_CDC = 0,
  ITF_NUM_CDC_DATA,
  ITF_NUM_CDC1,
  ITF_NUM_CDC1_DATA,
  ITF_NUM_AMP,
  ITF_NUM_TOTAL
};

enum
{
  CONFIG_ID_RNDIS = 0,
  CONFIG_ID_ECM   = 1,
  CONFIG_ID_COUNT
};

//--------------------------------------------------------------------+
// Device Descriptors
//--------------------------------------------------------------------+
tusb_desc_device_t const desc_device =
{
    .bLength            = sizeof(tusb_desc_device_t),
    .bDescriptorType    = TUSB_DESC_DEVICE,
    .bcdUSB             = 0x0101,

    // Use Interface Association Descriptor (IAD) device class
    // .bDeviceClass       = TUSB_CLASS_MISC,
    // .bDeviceSubClass    = MISC_SUBCLASS_COMMON,
    // .bDeviceProtocol    = MISC_PROTOCOL_IAD,
    .bDeviceClass       = 0xe0,
    .bDeviceSubClass    = 0x01,
    .bDeviceProtocol    = 0x01,
    
    .bMaxPacketSize0    = CFG_TUD_ENDPOINT0_SIZE,

    .idVendor           = 0xCafe,
    .idProduct          = USB_PID,
    .bcdDevice          = 0x0101,

    .iManufacturer      = STRID_MANUFACTURER,
    .iProduct           = STRID_PRODUCT,
    .iSerialNumber      = STRID_SERIAL,

    .bNumConfigurations = 0x01// CONFIG_ID_COUNT // multiple configurations
};

// Invoked when received GET DEVICE DESCRIPTOR
// Application return pointer to descriptor
uint8_t const * tud_descriptor_device_cb(void)
{
  return (uint8_t const *) &desc_device;
}

//--------------------------------------------------------------------+
// Configuration Descriptor
//--------------------------------------------------------------------+
// #define MAIN_CONFIG_TOTAL_LEN    (TUD_CONFIG_DESC_LEN + TUD_RNDIS_DESC_LEN + TUD_CDC_DESC_LEN + TUD_BT_DESC_LEN)
#define MAIN_CONFIG_TOTAL_LEN    (TUD_CONFIG_DESC_LEN + TUD_BTH_DESC_LEN)
// #define MAIN_CONFIG_TOTAL_LEN    (TUD_CONFIG_DESC_LEN + TUD_CDC_DESC_LEN + TUD_CDC_DESC_LEN)
#define ALT_CONFIG_TOTAL_LEN     (TUD_CONFIG_DESC_LEN + TUD_CDC_ECM_DESC_LEN + TUD_CDC_DESC_LEN + TUD_BT_DESC_LEN)

#define EPNUM_NET_NOTIF   0x81
#define EPNUM_NET_OUT     0x02
#define EPNUM_NET_IN      0x82
#define EPNUM_CDC_NOTIF   0x83
#define EPNUM_CDC_OUT     0x04
#define EPNUM_CDC_NOTIF   0x83

#define EPNUM_BT_EVT      0x81
#define EPNUM_BT_BULK_OUT 0x02
// #define EPNUM_BT_ISOCH_OUT  0x05

bool s_wifi_is_connected = false;


// Config number, interface count, string index, total length, attribute, power in mA
#define TUD_CONFIG_DESCRIPTOR(config_num, _itfcount, _stridx, _total_len, _attribute, _power_ma) \
  9, TUSB_DESC_CONFIGURATION, U16_TO_U8S_LE(_total_len), _itfcount, config_num, _stridx, TU_BIT(7) | _attribute, (_power_ma)/2

//------------- CDC -------------//

// Length of template descriptor: 61 bytes
#define TUD_BT_DESC_LEN  (8+9+7+7+7+9+7+7)

// CDC Descriptor Template
// Interface number, string index, EP notification address and size, EP data address (out, in) and size.
#define TUD_BT_DESCRIPTOR(_itfnum, _stridx, _ep_int, _ep_notif_size, _ep_bulkout, _ep_bulkout_size, _ep_bulkin, _ep_bulkin_size, _ep_isochout, _ep_isochout_size, _ep_isochin, _ep_isochin_size) \
  /* Interface Associate */\
  8, TUSB_DESC_INTERFACE_ASSOCIATION, _itfnum, 2, TUSB_CLASS_WIRELESS_CONTROLLER, TUD_BT_APP_SUBCLASS, TUD_BT_PROTOCOL_PRIMARY_CONTROLLER, 0,\
  /* Primary events & ACL Interface */\
  9, TUSB_DESC_INTERFACE, _itfnum, 0, 2, TUSB_CLASS_WIRELESS_CONTROLLER, TUD_BT_APP_SUBCLASS, TUD_BT_PROTOCOL_PRIMARY_CONTROLLER, 0,\
  /* INT ENDPOINT */\
  7, TUSB_DESC_ENDPOINT, _ep_int, TUSB_XFER_INTERRUPT, U16_TO_U8S_LE(_ep_notif_size), 16,\
  /* BULK OUT ENDPOINT */\
  7, TUSB_DESC_ENDPOINT, _ep_bulkout, TUSB_XFER_INTERRUPT, U16_TO_U8S_LE(_ep_bulkout_size), 16,\
  /* BULK IN ENDPOINT */\
  7, TUSB_DESC_ENDPOINT, _ep_bulkin, TUSB_XFER_INTERRUPT, U16_TO_U8S_LE(_ep_bulkin_size), 16, \
  /* Primary Controller SCO Interface */\
  9, TUSB_DESC_INTERFACE, (_itfnum) + 1, 0, 2, TUSB_CLASS_WIRELESS_CONTROLLER, TUD_BT_APP_SUBCLASS, TUD_BT_PROTOCOL_PRIMARY_CONTROLLER, 0,\
  /* ISOCH OUT ENDPOINT */\
  7, TUSB_DESC_ENDPOINT, _ep_isochout, TUSB_XFER_INTERRUPT, U16_TO_U8S_LE(_ep_isochout_size), 16,\
  /* ISOCH IN ENDPOINT */\
  7, TUSB_DESC_ENDPOINT, _ep_isochin, TUSB_XFER_INTERRUPT, U16_TO_U8S_LE(_ep_isochin_size), 16, 


static uint8_t const rndis_configuration[] =
{
  // Config number (index+1), interface count, string index, total length, attribute, power in mA
  // TUD_CONFIG_DESCRIPTOR(CONFIG_ID_RNDIS+1, 1/* ITF_NUM_TOTAL */, 0, MAIN_CONFIG_TOTAL_LEN, 0, 100),

  // // Interface number, string index, EP notification address and size, EP data address (out, in) and size.
  // TUD_RNDIS_DESCRIPTOR(ITF_NUM_CDC, STRID_INTERFACE, EPNUM_NET_NOTIF, 8, EPNUM_NET_OUT, EPNUM_NET_IN, CFG_TUD_NET_ENDPOINT_SIZE),
  
  // // TUD_CDC_DESCRIPTOR(0, 4, EPNUM_NET_NOTIF, 8, EPNUM_NET_OUT, 0x80 | EPNUM_NET_OUT, 64),
  // TUD_CDC_DESCRIPTOR(2, 4, EPNUM_CDC_NOTIF, 8, EPNUM_CDC_OUT, 0x80 | EPNUM_CDC_OUT, 64),
  // CDC_DESCRIPTOR(CONFIG_ID_RNDIS+2, 4, EPNUM_CDC_NOTIF, 8, EPNUM_CDC_OUT, 0x80 | EPNUM_CDC_OUT, 64),
  TUD_CONFIG_DESCRIPTOR(CONFIG_ID_RNDIS+1, 2/* ITF_NUM_TOTAL */, 0, MAIN_CONFIG_TOTAL_LEN, 0, 100),
  // TUD_BT_DESCRIPTOR(0, 4, EPNUM_BT_INT, 64, EPNUM_BT_BULK_OUT, 64, 0x80 | EPNUM_BT_BULK_OUT, 64, 
  //   EPNUM_BT_ISOCH_OUT, 64, 0x80 | EPNUM_BT_ISOCH_OUT, 64)
  // TUD_BTH_PRI_ITF(0, 4, EPNUM_BT_INT, 64, 16, EPNUM_BT_BULK_OUT, 0x80 | EPNUM_BT_BULK_OUT, 64),
  // TUD_BTH_ISO_ITF(1, 0, 0x80 | EPNUM_BT_ISOCH_OUT, EPNUM_BT_ISOCH_OUT, 64)
  // Interface number, string index, attributes, event endpoint, event endpoint size, interval, data in, data out, data endpoint size, iso endpoint sizes
  TUD_BTH_DESCRIPTOR(0, 0, EPNUM_BT_EVT, 16, 1, (0x80 | EPNUM_BT_BULK_OUT), EPNUM_BT_BULK_OUT, 64, 0, 9, 17, 25, 33, 49)
};

static uint8_t const ecm_configuration[] =
{
  // Config number (index+1), interface count, string index, total length, attribute, power in mA
  TUD_CONFIG_DESCRIPTOR(CONFIG_ID_ECM+1, ITF_NUM_TOTAL, 0, ALT_CONFIG_TOTAL_LEN, 0, 100),

  // Interface number, description string index, MAC address string index, EP notification address and size, EP data address (out, in), and size, max segment size.
  TUD_CDC_ECM_DESCRIPTOR(ITF_NUM_CDC, STRID_INTERFACE, STRID_MAC, EPNUM_NET_NOTIF, 64, EPNUM_NET_OUT, EPNUM_NET_IN, CFG_TUD_NET_ENDPOINT_SIZE, CFG_TUD_NET_MTU),
  // CDC_DESCRIPTOR(CONFIG_ID_ECM+2, 4, EPNUM_CDC_NOTIF, 8, EPNUM_CDC_OUT, 0x80 | EPNUM_CDC_OUT, 64),
  TUD_CDC_DESCRIPTOR(2, 4, EPNUM_CDC_NOTIF, 64, EPNUM_CDC_OUT, 0x80 | EPNUM_CDC_OUT, 64),
};

// Configuration array: RNDIS and CDC-ECM
// - Windows only works with RNDIS
// - MacOS only works with CDC-ECM
// - Linux will work on both
static uint8_t const * const configuration_arr[CONFIG_ID_COUNT] =
{
  [CONFIG_ID_RNDIS] = rndis_configuration,
  [CONFIG_ID_ECM  ] = ecm_configuration,
};

// Invoked when received GET CONFIGURATION DESCRIPTOR
// Application return pointer to descriptor
// Descriptor contents must exist long enough for transfer to complete
uint8_t const * tud_descriptor_configuration_cb(uint8_t index)
{
  return (index < CONFIG_ID_COUNT) ? configuration_arr[index] : NULL;
}

//--------------------------------------------------------------------+
// String Descriptors
//--------------------------------------------------------------------+

// array of pointer to string descriptors
static char const* string_desc_arr [] =
{
  [STRID_LANGID]       = (const char[]) { 0x09, 0x04 }, // supported language is English (0x0409)
  [STRID_MANUFACTURER] = "TinyUSB",                     // Manufacturer
  [STRID_PRODUCT]      = "TinyUSB Device",              // Product
  [STRID_SERIAL]       = "123456",                      // Serial
  [STRID_INTERFACE]    = "TinyUSB Network Interface",    // Interface Description
  [STRID_MAC]    = "",    // Interface Description
  [STRID_CDC]    = "TinyUSB CDC"    // Interface Description

  // STRID_MAC index is handled separately
};

static uint16_t _desc_str[32];

// Invoked when received GET STRING DESCRIPTOR request
// Application return pointer to descriptor, whose contents must exist long enough for transfer to complete
uint16_t const* tud_descriptor_string_cb(uint8_t index, uint16_t langid)
{
  (void) langid;
  unsigned int chr_count = 0;

  if (STRID_LANGID == index)
  {
    memcpy(&_desc_str[1], string_desc_arr[STRID_LANGID], 2);
    chr_count = 1;
  }
  else if (STRID_MAC == index)
  {
    // Convert MAC address into UTF-16

    for (unsigned i=0; i<sizeof(tud_network_mac_address); i++)
    {
      _desc_str[1+chr_count++] = "0123456789ABCDEF"[(tud_network_mac_address[i] >> 4) & 0xf];
      _desc_str[1+chr_count++] = "0123456789ABCDEF"[(tud_network_mac_address[i] >> 0) & 0xf];
    }
  }
  else
  {
    // Note: the 0xEE index string is a Microsoft OS 1.0 Descriptors.
    // https://docs.microsoft.com/en-us/windows-hardware/drivers/usbcon/microsoft-defined-usb-descriptors

    if ( !(index < sizeof(string_desc_arr)/sizeof(string_desc_arr[0])) ) return NULL;

    const char* str = string_desc_arr[index];

    // Cap at max char
    chr_count = strlen(str);
    if ( chr_count > (TU_ARRAY_SIZE(_desc_str) - 1)) chr_count = TU_ARRAY_SIZE(_desc_str) - 1;

    // Convert ASCII string into UTF-16
    for (unsigned int i=0; i<chr_count; i++)
    {
      _desc_str[1+i] = str[i];
    }
  }

  // first byte is length (including header), second byte is string type
  _desc_str[0] = (TUSB_DESC_STRING << 8 ) | (2*chr_count + 2);

  return _desc_str;
}

bool tud_network_recv_cb(const uint8_t *src, uint16_t size)
{
  if (s_wifi_is_connected) {
    esp_wifi_internal_tx(ESP_IF_WIFI_STA, src, size);
  }
  tud_network_recv_renew();
  return true;
}

uint16_t tud_network_xmit_cb(uint8_t *dst, void *ref, uint16_t arg)
{
  uint16_t len = arg;

  /* traverse the "pbuf chain"; see ./lwip/src/core/pbuf.c for more info */
  memcpy(dst, ref, len);
  return len;
}

void tud_network_init_cb(void)
{
}

SemaphoreHandle_t semp;
bool tud_network_wait_xmit(uint32_t ms)
{
  if (xSemaphoreTake(semp, ms/portTICK_PERIOD_MS) == pdTRUE) {
    xSemaphoreGive(semp);
    return true;
  }

  return false;
}

void tud_network_set_xmit_status(bool enable)
{
    if (enable == true) {
      xSemaphoreGive(semp);
    } else {
      xSemaphoreTake(semp, 0);
    }
}

#define DELAY_TICK    10
static esp_err_t pkt_wifi2usb(void *buffer, uint16_t len, void *eb)
{
    uint32_t loop = DELAY_TICK;
    if (!tud_ready()) {
      esp_wifi_internal_free_rx_buffer(eb);
      return ERR_USE;
    }
    
    // loop = DELAY_TICK;
    // while(loop-- && !tud_network_can_xmit()) {
    //   vTaskDelay(1);
    // }
    if (tud_network_wait_xmit(100)) {
      /* if the network driver can accept another packet, we make it happen */
      if (tud_network_can_xmit()) {
          tud_network_xmit(buffer, len);
      }
    }

    esp_wifi_internal_free_rx_buffer(eb);
    return ESP_OK;
}

static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data)
{
    switch (event_id) {
        case WIFI_EVENT_STA_START:
          esp_wifi_get_mac(ESP_IF_WIFI_STA, tud_network_mac_address);
          esp_wifi_connect();
          break;
        case WIFI_EVENT_STA_CONNECTED:
            ESP_LOGI(TAG, "Wi-Fi STA connected");
            esp_wifi_internal_reg_rxcb(ESP_IF_WIFI_STA, pkt_wifi2usb);
            // s_wifi_is_started = true;
            s_wifi_is_connected = true;

            // esp_wifi_internal_reg_rxcb(ESP_IF_WIFI_STA, pkt_wifi2usb);
            break;

        case WIFI_EVENT_STA_DISCONNECTED:
            ESP_LOGI(TAG, "Wi-Fi STA disconnected");
            s_wifi_is_connected = false;
            // esp_wifi_internal_reg_rxcb(ESP_IF_WIFI_STA, NULL);
            if (tud_ready()) {
              esp_wifi_connect();
            }
            break;

        default:
            break;
    }
}

/* Initialize Wi-Fi as sta and set scan method */
static void wifi_init(void)
{
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_t *sta_netif = esp_netif_create_default_wifi_sta();
    assert(sta_netif);
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, wifi_event_handler, NULL);
    wifi_config_t wifi_config = {
        .sta = {
            .ssid = "buyaolianwo",
            .password = "adminxcg"
        },
    };
    // esp_netif_dhcpc_stop(sta_netif);
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA) );
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config) );
    ESP_ERROR_CHECK(esp_wifi_start() );
}

#if 0
// echo to either Serial0 or Serial1
// with Serial0 as all lower case, Serial1 as all upper case
static void echo_serial_port(uint8_t itf, uint8_t buf[], uint32_t count)
{
  for(uint32_t i=0; i<count; i++)
  {
    if (itf == 0)
    {
      // echo back 1st port as lower case
      if (isupper(buf[i])) buf[i] += 'a' - 'A';
    }
    else
    {
      // echo back 2nd port as upper case
      if (islower(buf[i])) buf[i] -= 'a' - 'A';
    }

    tud_cdc_n_write_char(itf, buf[i]);
  }
  tud_cdc_n_write_flush(itf);
}

//--------------------------------------------------------------------+
// USB CDC
//--------------------------------------------------------------------+
static void cdc_task(void)
{
  uint8_t itf;

  for (itf = 0; itf < CFG_TUD_CDC; itf++)
  {
    // connected() check for DTR bit
    // Most but not all terminal client set this when making connection
    // if ( tud_cdc_n_connected(itf) )
    {
      if ( tud_cdc_n_available(itf) )
      {
        uint8_t buf[64];

        uint32_t count = tud_cdc_n_read(itf, buf, sizeof(buf));

        // echo back to both serial ports
        echo_serial_port(0, buf, count);
        echo_serial_port(1, buf, count);
      }
    }
  }
}
#endif
#ifdef ADD_BLE_HCI
static const char *tag = "BLE_USB";
static uint8_t hci_cmd_buf[128];
static uint8_t hci_evt_buf[128];
static uint8_t hci_acl_buf_host[128]; // recv data from host
static uint8_t hci_acl_long_buf_host[1024]; 
static uint8_t hci_acl_buf_contr[128]; 
//static uint16_t gl_evt_len = 0;
SemaphoreHandle_t xSemaphore = NULL;

void ble_controller_init(void) {

    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    esp_err_t ret;
    ret = esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT);
    if (ret) {
        ESP_LOGI(tag, "Bluetooth controller release classic bt memory failed: %s", esp_err_to_name(ret));
        return;
    }

    if ((ret = esp_bt_controller_init(&bt_cfg)) != ESP_OK) {
        ESP_LOGI(tag, "Bluetooth controller initialize failed: %s", esp_err_to_name(ret));
        return;
    }

    if ((ret = esp_bt_controller_enable(ESP_BT_MODE_BLE)) != ESP_OK) {
        ESP_LOGI(tag, "Bluetooth controller enable failed: %s", esp_err_to_name(ret));
        return;
    }
}

/*
 * @brief: BT controller callback function, used to notify the upper layer that
 *         controller is ready to receive command
 */
static void controller_rcv_pkt_ready(void)
{
    //printf("controller rcv pkt ready\n");
}
/*
 * @brief: BT controller callback function, to transfer data packet to upper
 *         controller is ready to receive command
 */
static int host_rcv_pkt(uint8_t *data, uint16_t len)
{
    memset(hci_evt_buf, 0x0 , 128);
    memset(hci_acl_buf_contr, 0x0 , 128);
    uint16_t act_len = len - 1;
    //printf("host_rcv_pkt: data[0] = %02x\n", data[0]);

    if(data[0] == 0x04) { // event data from controller
      memcpy(hci_evt_buf, data +1 , act_len);
      printf("evt_data from controller, evt_data_length: %d :\n", act_len);
      for (uint16_t i = 0; i < act_len; i++) {
        printf("%02x ", hci_evt_buf[i]);
      }
      printf("\n");
      xSemaphoreGive(xSemaphore);
      tud_bt_event_send(hci_evt_buf, act_len);
    } else if(data[0] == 0x02) { // acl data from controller
      memcpy(hci_acl_buf_contr, data +1 , act_len);
      printf("acl_data from controller, acl_data_length: %d :\n", act_len);
      for (uint16_t i = 0; i < act_len; i++) {
        printf("%02x ", hci_acl_buf_contr[i]);
      }
      printf("\n");
      //xSemaphoreGive(xSemaphore);
      tud_bt_acl_data_send(hci_acl_buf_contr, act_len);
    }
    return 0;
}

static esp_vhci_host_callback_t vhci_host_cb = {
    controller_rcv_pkt_ready,
    host_rcv_pkt
};

#endif
void app_main(void)
{
  // Initialize NVS
  esp_err_t ret = nvs_flash_init();
  if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
      ESP_ERROR_CHECK(nvs_flash_erase());
      ret = nvs_flash_init();
  }
  ESP_ERROR_CHECK( ret );
#ifdef ADD_BLE_HCI
  // init ble controller
  ble_controller_init();
  esp_vhci_host_register_callback(&vhci_host_cb);
  vSemaphoreCreateBinary(xSemaphore);
#endif
  vSemaphoreCreateBinary(semp);
  /* initialize TinyUSB */
  // board_init();
  ESP_LOGI(TAG, "USB initialization");
  tinyusb_config_t tusb_cfg = {0}; // the configuration using default values
  ESP_ERROR_CHECK(tinyusb_driver_install(&tusb_cfg));
  
  tusb_init();
  // ble_hci_trans_cfg_hs(ble_hs_hci_rx_evt, NULL, ble_hs_rx_data, NULL);
  wifi_init();
#ifdef CONFIG_HEAP_TRACING
    heap_trace_init_standalone(trace_record, NUM_RECORDS);
    heap_trace_start(HEAP_TRACE_LEAKS);
#endif

  while (1)
  {
    tud_task();
    // cdc_task();
  }
}

// Invoked when HCI command was received over USB from Bluetooth host.
// Detailed format is described in Bluetooth core specification Vol 2,
// Part E, 5.4.1.
// Length of the command is from 3 bytes (2 bytes for OpCode,
// 1 byte for parameter total length) to 258.
void tud_bt_hci_cmd_cb(void *hci_cmd, size_t cmd_len)
{
  //printf(">>> %s %d line\r\n", __func__, __LINE__);
  memset(hci_cmd_buf, 0x0 , 128);
  hci_cmd_buf[0] = 0x01;
  memcpy(hci_cmd_buf+1, hci_cmd, cmd_len);

  printf("cmd_data from host, length: %d\n",cmd_len +1);
  for(int i = 0 ; i< cmd_len + 1; i++) {
      printf("%02x ",hci_cmd_buf[i]);
  }
  printf("\n");

  esp_vhci_host_send_packet(hci_cmd_buf, cmd_len +1);

  xSemaphoreTake(xSemaphore, portMAX_DELAY);

}

// Invoked when ACL data was received over USB from Bluetooth host.
// Detailed format is described in Bluetooth core specification Vol 2,
// Part E, 5.4.2.
// Length is from 4 bytes, (12 bits for Handle, 4 bits for flags
// and 16 bits for data total length) to endpoint size.
static bool prepare_write = false;
static uint16_t write_offset = 0;
static uint16_t acl_data_length = 0;
void tud_bt_acl_data_received_cb(void *acl_data, uint16_t data_len)
{ // if acl_data is long data
  if(!prepare_write) {
     // first get acl_data_length
      acl_data_length = *(((uint16_t * )acl_data) + 1);
      if(acl_data_length > data_len) {
        prepare_write = true;
        memset(hci_acl_buf_host, 0x0, 128);
        hci_acl_buf_host[0] = 0x02;
        memcpy(hci_acl_buf_host + 1, acl_data, data_len);
        write_offset = data_len + 1;
      } else {
        memset(hci_acl_buf_host, 0x0 , 128);
        hci_acl_buf_host[0] = 0x02;
        memcpy(hci_acl_buf_host + 1, acl_data, data_len);
        printf("short acl_data from host, will send to controller, length: %d\n",data_len + 1);
        for(int i = 0 ; i< data_len + 1; i++) {
          printf("%02x ",hci_acl_buf_host[i]);
        }
        printf("\n");
        esp_vhci_host_send_packet(hci_acl_buf_host, data_len + 1);
      }
  } else {
      memcpy(hci_acl_buf_host + write_offset, acl_data, data_len);
      write_offset += data_len;
      if(acl_data_length > write_offset) {
        printf" Remaining bytes: %d\n", acl_data_length - write_offset);
        return;
      }
      printf("long acl_data from host, will send to controller, length: %d\n",data_len + 1);
      for(int i = 0 ; i< write_offset; i++) {
        printf("%02x ",hci_acl_buf_host[i]);
      }
      printf("\n");
      prepare_write = false;
      esp_vhci_host_send_packet(hci_acl_buf_host, data_len + write_offset);
  }

}

// Called when event sent with tud_bt_event_send() was delivered to BT stack.
// Controller can release/reuse buffer with Event packet at this point.
void tud_bt_event_sent_cb(uint16_t sent_bytes)
{
  //printf(">>> %s %d line\r\n", __func__, __LINE__);
}

// Called when ACL data that was sent with tud_bt_acl_data_send()
// was delivered to BT stack.
// Controller can release/reuse buffer with ACL packet at this point.
void tud_bt_acl_data_sent_cb(uint16_t sent_bytes)
{
  //printf(">>> %s %d line\r\n", __func__, __LINE__);
  //printf("controller --> host:????????????????????????????????????\n");
  //tud_bt_acl_data_send()
}
