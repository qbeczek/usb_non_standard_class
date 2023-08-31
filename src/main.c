/*
 * USB non standard class
 *
 * @author: Jakub Szymanski
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <zephyr/device.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/init.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/ring_buffer.h>
#include <zephyr/usb/usb_device.h>
#include <zephyr/usb/usbd.h>

#include "led.h"

LOG_MODULE_REGISTER(main);

#define BULK_OUT_EP_ADDR 0x01
#define BULK_IN_EP_ADDR 0x81

#define BULK_OUT_EP_IDX 0
#define BULK_IN_EP_IDX 1

static uint8_t usb_static_buffer[64] = {0};
// BUILD_ASSERT(sizeof(usb_static_buffer) == CONFIG_USB_REQUEST_BUFFER_SIZE);

struct usb_config {
    struct usb_if_descriptor if0;
    struct usb_ep_descriptor if0_out_ep;
    struct usb_ep_descriptor if0_in_ep;
} __packed;

USBD_CLASS_DESCR_DEFINE(primary, 0)
struct usb_config my_usb_config = {
    /* Interface descriptor 0 */
    .if0 =
        {
            .bLength = sizeof(struct usb_if_descriptor),
            .bDescriptorType = USB_DESC_INTERFACE,
            .bInterfaceNumber = 0,
            .bAlternateSetting = 0,
            .bNumEndpoints = 2,
            .bInterfaceClass = USB_BCC_VENDOR,
            .bInterfaceSubClass = 0,
            .bInterfaceProtocol = 0,
            .iInterface = 0,
        },

    /* Data Endpoint OUT */
    .if0_out_ep =
        {
            .bLength = sizeof(struct usb_ep_descriptor),
            .bDescriptorType = USB_DESC_ENDPOINT,
            .bEndpointAddress = BULK_OUT_EP_ADDR,
            .bmAttributes = USB_DC_EP_BULK,
            .wMaxPacketSize = 64,
            .bInterval = 0x00,
        },

    /* Data Endpoint IN */
    .if0_in_ep =
        {
            .bLength = sizeof(struct usb_ep_descriptor),
            .bDescriptorType = USB_DESC_ENDPOINT,
            .bEndpointAddress = BULK_IN_EP_ADDR,
            .bmAttributes = USB_DC_EP_BULK,
            .wMaxPacketSize = 64,
            .bInterval = 0x00,
        },
};

static void bulk_ep_out_cb(uint8_t ep,
                           enum usb_dc_ep_cb_status_code ep_status) {
    uint32_t bytes_to_read;

    usb_read(ep, NULL, 0, &bytes_to_read);
    LOG_INF("ep 0x%x, bytes to read %d ", ep, bytes_to_read);
    usb_read(ep, usb_static_buffer, bytes_to_read, NULL);

    LOG_INF("Received value: %d ", usb_static_buffer[0]);

    int result =
        usb_write(ep, usb_static_buffer, sizeof(usb_static_buffer), NULL);

    set_led_state(usb_static_buffer[0]);
}

static void bulk_ep_in_cb(uint8_t ep, enum usb_dc_ep_cb_status_code ep_status) {
    uint8_t stalled;
    LOG_WRN("usb_dc_ep_is_stalled - return: %d, stalled: %d",
            usb_dc_ep_is_stalled(ep, &stalled), stalled);

    int result =
        usb_write(ep, usb_static_buffer, sizeof(usb_static_buffer), NULL);

    if (result == 0) {
        LOG_INF("ep 0x%x", ep);
        LOG_INF("Sending value: %d ", usb_static_buffer[0]);
    } else {
        printk("USB write failed with error code %d.\n", result);
    }
}

static struct usb_ep_cfg_data ep_cfg[] = {
    {
        .ep_cb = bulk_ep_out_cb,
        .ep_addr = BULK_OUT_EP_ADDR,
    },
    {
        .ep_cb = bulk_ep_in_cb,
        .ep_addr = BULK_IN_EP_ADDR,
    },
};

static void usb_status_cb(struct usb_cfg_data *cfg,
                          enum usb_dc_status_code status,
                          const uint8_t *param) {
    ARG_UNUSED(cfg);

    // LOG_INF("USB Status callback enter");

    switch (status) {
        case USB_DC_INTERFACE:
            bulk_ep_in_cb(ep_cfg[BULK_IN_EP_IDX].ep_addr, 0);
            LOG_INF("USB interface configured");
            break;

        case USB_DC_SET_HALT:
            LOG_INF("Set Feature ENDPOINT_HALT");
            break;

        case USB_DC_CLEAR_HALT:
            /*TODO: understand this and use this */
            LOG_INF("Clear Feature ENDPOINT_HALT");
            if (*param == ep_cfg[BULK_IN_EP_IDX].ep_addr) {
                bulk_ep_in_cb(ep_cfg[BULK_IN_EP_IDX].ep_addr, 0);
            }
            break;

        default:
            break;
    }
}

static int usb_vendor_handler(struct usb_setup_packet *setup, int32_t *len,
                              uint8_t **data) {
    LOG_INF("Class request: bRequest 0x%x bmRequestType 0x%x len %d",
            setup->bRequest, setup->bmRequestType, *len);

    if (setup->RequestType.recipient != USB_REQTYPE_RECIPIENT_DEVICE) {
        return -ENOTSUP;
    }

    if (usb_reqtype_is_to_device(setup) && setup->bRequest == 0x5b) {
        LOG_INF("Host-to-Device, data: %p wValue: %d", (void *)*data,
                setup->wValue);
        LOG_INF("Receive message: %s", *data);
        /*
         * Copy request data in usb_static_buffer buffer and reuse
         * it later in control device-to-host transfer.
         */
        memcpy(usb_static_buffer, *data,
               MIN(sizeof(usb_static_buffer), setup->wLength));
        set_led_state(setup->wValue);

        return 0;
    }

    if ((usb_reqtype_is_to_host(setup)) && (setup->bRequest == 0x5c)) {
        LOG_INF("Device-to-Host, wValue %d, data %p", setup->wValue,
                (void *)*data);
        LOG_INF("Sending message: %s", usb_static_buffer);
        *data = usb_static_buffer;
        *len = MIN(sizeof(usb_static_buffer), setup->wLength);
        return 0;
    }

    return -ENOTSUP;
}

/* usb.rst vendor handler end */
static void custom_interface_config(struct usb_desc_header *head,
                                    uint8_t bInterfaceNumber) {
    ARG_UNUSED(head);

    my_usb_config.if0.bInterfaceNumber = bInterfaceNumber;
}

/* usb.rst device config data start */
USBD_DEFINE_CFG_DATA(my_usb_config_data) = {
    .usb_device_description = NULL,
    .interface_config = custom_interface_config,
    .interface_descriptor = &my_usb_config.if0,
    .cb_usb_status = usb_status_cb,
    .interface =
        {
            .class_handler = NULL,
            .custom_handler = NULL,
            .vendor_handler = usb_vendor_handler,
        },
    .num_endpoints = ARRAY_SIZE(ep_cfg),
    .endpoint = ep_cfg,
};
/* usb.rst device config data end */

int main(void) {
    int ret;

    ret = usb_enable(NULL);
    if (ret != 0) {
        LOG_ERR("Failed to enable USB");
        return 0;
    }

    LOG_INF("entered main.");
    if (init_led() < 0) {
        LOG_WRN("Failed to enable LED");
        return 0;
    }

    return 0;
}