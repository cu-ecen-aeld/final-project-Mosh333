# Define external path variable
BR2_EXTERNAL_FINAL_PROJECT_PATH := $(realpath $(dir $(lastword $(MAKEFILE_LIST))))

# Register the package
include $(BR2_EXTERNAL_FINAL_PROJECT_PATH)/mgba/mgba.mk
include $(BR2_EXTERNAL_FINAL_PROJECT_PATH)/mgba_menu/mgba_menu.mk