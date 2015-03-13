# Introduction #

We are going to explain how to build an Openwrt custom firmware from source code to use the CORA code algorithm of this project.

## Obtaining OpenWRT ##
First of all, you need to make a checkout of OpenWRT code from svn repository. The CORA algorithm of this project was implemented and tested for the OpenWRT Backfire 10.03.1-rc4 version (SVN repository [revision 24045](https://code.google.com/p/cora-cognitiverateadaptation/source/detail?r=24045)).
```
svn checkout --revision=24045 svn://svn.openwrt.org/openwrt/branches/backfire/ ./openwrt
```

The, you have to update and install the packages for this same version ([revision 24045](https://code.google.com/p/cora-cognitiverateadaptation/source/detail?r=24045)). To do that:
```
cd ./openwrt
cp feeds.conf.default feeds.conf
```

Open the `feeds.conf` file for editing and add the "`@24045`" at the end of the line that checks out the packages feed. It should look like this:
```
src-svn packages svn://svn.openwrt.org/openwrt/packages@24045
```

Update and install the packages:
```
./scripts/feeds update -a && ./scripts/feeds install -a
```

## Configuring OpenWRT ##
To access openwrt configuration do:
```
make menuconfig
```

Now you can select the options that matchs your hardware. Be careful!! You should use an appropriate configuration for your device or your device can be unreachable.

If you are not familiar with this step, take a look at openwrt's site for help: http://wiki.openwrt.org/doc/start and http://www.openwrt.org

The CORA code in this project was tested in a Ubiquiti RouterStation Pro with an Atheros Chipset. You can check the configuration for this hardware at [rspro](rspro.md)

## Compiling OpenWRT ##
In order to compile the code just execute:
```
make V=99
```

An be patient...