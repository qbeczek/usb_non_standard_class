# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.20.0)
set(BOARD blackpill_f401ce)
set(DTC_OVERLAY_FILE blackpill_f401ce.overlay)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})

project(usb_non_standard_class)

target_sources(app  PRIVATE src/main.c src/led.c src/bme280.c)
