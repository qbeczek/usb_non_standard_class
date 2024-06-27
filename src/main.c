/*
 * USB non standard class
 *
 * @author: Jakub Szymanski
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <zephyr/device.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/init.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/ring_buffer.h>
#include <zephyr/usb/usb_device.h>
#include <zephyr/usb/usbd.h>

#include "bme280.h"
#include "led.h"

#define BULK_OUT_EP_ADDR 0x01
#define BULK_IN_EP_ADDR 0x81

#define BULK_OUT_EP_IDX 0
#define BULK_IN_EP_IDX 1

#define USB_READ_FLAG 0x5b
#define USB_WRITE_FLAG 0x5c
#define I2C_FIND_DEVICE 0x5d
#define I2C_READ_BYTE 0x5e

LOG_MODULE_REGISTER(main);

static uint8_t usb_static_buffer[64] = {0};
// BUILD_ASSERT(sizeof(usb_static_buffer) == CONFIG_USB_REQUEST_BUFFER_SIZE);
uint8_t who_am_i;
const struct device *i2c_dev = DEVICE_DT_GET(DT_NODELABEL(i2c1));

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

static uint8_t is_i2c_ready() {
    if (i2c_dev == NULL) {
        /* No such node, or the node does not have status "okay". */
        LOG_INF("Error: no i2c device founding.\n");
        return NULL;
    }

    if (!device_is_ready(i2c_dev)) {
        LOG_INF(
            "Error: Device \"%s\" is not ready; "
            "check the driver initialization logs for errors.\n",
            i2c_dev->name);
        return NULL;
    }
    LOG_INF("Device is ready\n");
    return 1;
}

static void bulk_ep_out_cb(uint8_t ep,
                           enum usb_dc_ep_cb_status_code ep_status) {
    uint32_t bytes_to_read;

    usb_read(ep, NULL, 0, &bytes_to_read);
    LOG_INF("ep 0x%x, bytes to read %d ", ep, bytes_to_read);
    usb_read(ep, usb_static_buffer, bytes_to_read, NULL);

    LOG_INF("Received value: %d ", usb_static_buffer[0]);

    // int result =
    //     usb_write(0x81, usb_static_buffer, sizeof(usb_static_buffer), NULL);

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

    if (usb_reqtype_is_to_device(setup) && setup->bRequest == USB_READ_FLAG) {
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

    if ((usb_reqtype_is_to_host(setup)) &&
        (setup->bRequest == USB_WRITE_FLAG)) {
        LOG_INF("Device-to-Host, wValue %d, data %p", setup->wValue,
                (void *)*data);
        LOG_INF("Sending message: %s", usb_static_buffer);
        *data = usb_static_buffer;
        *len = MIN(sizeof(usb_static_buffer), setup->wLength);
        return 0;
    }

    if ((usb_reqtype_is_to_device(setup)) &&
        (setup->bRequest == I2C_FIND_DEVICE)) {
        LOG_INF("Device-to-Host, wValue %d, data %p", setup->wValue,
                (void *)*data);
        // i2c_dev = DEVICE_DT_GET(DT_NODELABEL(i2c1));

        if (!is_i2c_ready()) {
            LOG_ERR("Error: no i2c device found.\n");
            return NULL;
        }

        LOG_INF("Found device \"%s\", ready tu use\n", i2c_dev->name);

        return 0;
    }
    // tu trzeba dodać jakieś czytelne parsowanie tego co przychodzi z linuxa
    if ((usb_reqtype_is_to_device(setup)) &&
        (setup->bRequest == I2C_READ_BYTE)) {
        // i2c_dev = DEVICE_DT_GET(DT_NODELABEL(i2c1));

        // if (!is_i2c_ready()) {
        //     LOG_ERR("Error: no i2c device found.\n");
        //     return NULL;
        // }

        // int ret = i2c_reg_read_byte(i2c_dev, BMP280_I2C_ADDRESS,
        // BMP280_REG_CHIPID,
        //                             &who_am_i);
        // if (ret != 0) {
        //     printk("Failed to read chip ID: %d\n", ret);
        //     return 0;
        // }

        const struct device *i2c_dev = DEVICE_DT_GET(DT_NODELABEL(i2c1));

        read_temperature_from_bmp280(i2c_dev);
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

    const struct device *i2c_dev = DEVICE_DT_GET(DT_NODELABEL(i2c1));

    if (!i2c_dev) {
        printk("Cannot get I2C device\n");
        return 0;
    }

    printk("I2C device found\n");

    // Sprawdź identyfikator chipu
    uint8_t chip_id;
    ret = i2c_reg_read_byte(i2c_dev, BMP280_I2C_ADDRESS, BMP280_REG_CHIPID,
                            &chip_id);
    if (ret != 0) {
        printk("Failed to read chip ID: %d\n", ret);
        return 0;
    }

    if (chip_id != BMP280_CHIPID) {
        printk("Invalid chip ID: 0x%x\n", chip_id);
        return 0;
    }

    read_temperature_from_bmp280(i2c_dev);

    printk("End\n");
    return 0;
}