# Introduction #

How to use debugfs to see CORA's perfomance.


# See performance #
Inside the /sys/kernel/debug/ieee80211/phy0/stations/ directory there will be subdirectories, where the subdirectory corresponds to each node that we are associated with. The name of the subdirectory is the mac address of the associated node.

There you can use the following command
```
while true; do cat rc_stats ; sleep 1; clear; done
```

# Getting all results (pendrive needed) #
For this, you should use a pendrive.
Start with a USB pendrive that is formatted ext3.

After it is recognised by Openwrt (use the command dmesg to see it)
You can mount it and umount using for example:
```
mount -t ext3 /dev/sda2 /media/pendrive
umount /dev/sda2
```


To put rc\_status into a file, to get each rate changes for each client connected to the router use the following script:
```
	    let count=0 
	    while true
	    do
		let count=count+1
	echo $count
		if [[ $count -eq (Inteiro que depende do tempo usado como parâmetro no cora, para t=100ms pode-ser usar 10000) ]]; then
   		    let count=0
		    cat /sys/kernel/debug/ieee80211/phy0/netdev\:wlan0/stations/(Mac Adress)/rc_stats >> /media/pendrive/cora 
		fi
    	done
```


## References ##

http://wireless.kernel.org/en/developers/Documentation/mac80211/RateControl/minstrel

http://wiki.openwrt.org/doc/howto/extroot