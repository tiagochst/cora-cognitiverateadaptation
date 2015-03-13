# Introduction #

How to configure an Basic Openwrt custom firmware to the following device:

  1. Ubitiqui Routerstation pro 680MHZ/4 LAN GIGA/128 MB/3 SLOTS+USB
  1. Mini-PCI Senao 2,4/5,8GHZ NL-5354MP PLUS ARIES2
  1. Power supply 18V 2000MA 90-240V 18W1820
  1. Cable Pig tail UFL for SMA

# Config #
In OpenWRT's root to access Openwrt configuration do:
```
Make menuconfig
```

Select the following options:
```
Tagert System: Atheros AR71xx/AR7240/AR913x
Target Profile: (Ubiquiti RouterStation Pro)  
```

To select the appropriate wireless Driver:
```
Kernel modules ---> Wireless Drivers --->
```

There you should select:
```
kmod-ath5k (Atheros 5xxx wireless card)
kmod-madwifi
```

`Kmod-ath5k` enables `kmod-mac80211`, `kmod-ath` and `kmod-cfg80211`.

`Kmod-ath` has two options that you should enable:
```
[*] Force Atheros drivers to respect the user's regdomain settings   
[*] Atheros wireless debugging                                        
```

`Kmod-mac80211` has one options that you also should enable:
```
[*] Export mac80211 internals in DebugFS
```

`kmod-madwifi` has one option that you should enable:
```
[*] Combine driver and net80211 into a single module
```

For wireless drivers we're done.

Next we can select the web interface luci. Return to main menu and choose:
```
LuCI  -->  Collections  --->
```

Select luci package
```
<*> luci 
```

# Other packages #
After flashing te router, we also have installed the following packages:

  * vsc7385-ucode-ap83
  * vsc7385-ucode-pb44
  * vsc7395-ucode-ap83
  * vsc7395-ucode-pb44
  * wpad-mini
  * kmod-hostap
  * kmod-hostap-pci

# /etc/config/wireless #
As long as **wifi detect** was not working, we included manually the wireless configuration in /etc/config.

Be careful with this configuration: **option macaddr** is different to each device.

```
	config wifi-device  radio0
	        option type     mac80211
	        option channel  11
	        option macaddr  00:02:6f:24:42:fe
	        option hwmode   11g
	
	        # REMOVE THIS LINE TO ENABLE WIFI:
	        option disabled 1
	
	config wifi-iface
	        option device   radio0
	        option network  lan
	        option mode     ap
	        option ssid     OpenWrt
	        option encryption none
```