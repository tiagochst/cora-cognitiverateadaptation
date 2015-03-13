# Introduction #

We are going to explain how to insert CORA algorithm into your custom Openwrt firmware.

## Compiling CORA ##

First of all, we should replace some files. Go to:
```
cd (path_to_openwrt)/package/mac80211
```

Replace the Makefile for the one in the repository of this project.

Then, copy the file "910-multiple\_rc\_algorithms.patch" from this project into de following directory:
```
(path_to_openwrt)/package/mac80211/patches/
```

Go to the root dir (path\_to\_openwrt) and execute:
```
make /packages/mac80211/{clean,compile} V=99
```

This command will delete all extrated packages from the {{{build\_dir}
}}, and will extract the original files again. You will face an compile error claiming that `rc80211_cora.o` can't be build due to lack of source code. Now you have to copy all rc80211**_.c and rc80211_**.h from this project as shown:
```
cp rc80211_* ./openwrt/build_dir/linux-ar71xx/compat-wireless-2010-10-19/net/mac80211/
```

Try recompile the package:
```
make /packages/mac80211/compile V=99
```

This is probably going to work!

You can choose wich algorithm to use changing CONFIG\_MAC80211\_RC\_DEFAULT and CONFIG\_COMPAT\_MAC80211\_RC\_DEFAULT variables in ./openwrt/build\_dir/linux-ar71xx/compat-wireless-2010-10-19/config.mk file.

## Installing new packages into RSPro ##

Copy the packages files into your RSPro device:
```
cd (path_to_openwrt)/bin/ar71xx/packages/
scp kmod-ath_2.6.32.25+2010-10-19-1_ar71xx.ipk kmod-ath5k_2.6.32.25+2010-10-19-1_ar71xx.ipk kmod-cfg80211_2.6.32.25+2010-10-19-1_ar71xx.ipk kmod-mac80211_2.6.32.25+2010-10-19-1_ar71xx.ipk <<user>>@<<rspro_ip>>
```

Install them in the device using:
```
opkg install *.ipk
```

Reboot your router.
Be careful! If something went wrong, your router will be Unreachable!