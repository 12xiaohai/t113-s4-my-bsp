#
# AIC8800D80 USB Wi-Fi support for xiaohai_t113s4_nand.
#

define KernelPackage/net-aic8800
  SUBMENU:=$(WIRELESS_MENU)
  TITLE:=AIC8800 USB/SDIO wireless support
  DEPENDS:=+aic8800-firmware +@IPV6
  KCONFIG:=\
	CONFIG_WLAN=y \
	CONFIG_CFG80211=y \
	CONFIG_CFG80211_WEXT=y \
	CONFIG_RFKILL=y \
	CONFIG_SUNXI_RFKILL=y \
	CONFIG_AIC_WLAN_SUPPORT=y \
	CONFIG_AIC_INTF_USB=y \
	CONFIG_USB_MSG_EP=y \
	CONFIG_AIC8800_WLAN_SUPPORT=m
  FILES:=$(LINUX_DIR)/drivers/net/wireless/aic8800/aic8800_bsp/aic8800_bsp.ko
  FILES+=$(LINUX_DIR)/drivers/net/wireless/aic8800/aic8800_fdrv/aic8800_fdrv.ko
  AUTOLOAD:=$(call AutoProbe,aic8800_bsp aic8800_fdrv)
endef

define KernelPackage/net-aic8800/description
  Kernel modules for the AIC8800D/D80 USB Wi-Fi family.
endef

$(eval $(call KernelPackage,net-aic8800))
