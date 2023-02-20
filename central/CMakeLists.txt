# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(central_multilink)

target_sources(app PRIVATE
  src/main.c
  src/app_bt.c
)
target_include_directories(app PRIVATE src)

zephyr_library_include_directories(${ZEPHYR_BASE}/samples/bluetooth)
