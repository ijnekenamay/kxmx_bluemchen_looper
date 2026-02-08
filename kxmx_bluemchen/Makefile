# Project Name
TARGET = looper

# Sources
# 1. 録音バッファを使う場合、最適化を上げないと処理が追いつかないため OPT を -O3 に変更
DEBUG = 0
OPT = -O3

# 2. ソースファイル名を新しいファイル名（例: looper.cpp）に合わせて変更
CPP_SOURCES = src/kxmx_bluemchen.cpp examples/looper.cpp

USE_FATFS = 1

# 3. 【重要】SDRAMに巨大な配列を配置するためのリンカーフラグを追加
# これがないと DSY_SDRAM_BSS を使った巨大バッファでエラーが出る場合があります
LDFLAGS += -u _printf_float

# Library Locations
LIBDAISY_DIR = libDaisy
DAISYSP_DIR = DaisySP

# Core location, and generic Makefile.
SYSTEM_FILES_DIR = $(LIBDAISY_DIR)/core
include $(SYSTEM_FILES_DIR)/Makefile