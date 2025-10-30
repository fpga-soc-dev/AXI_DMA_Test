SUMMARY = "User space mappable DMA Buffer"
AUTHOR = "Ichiro Kawazome"
LICENSE = "BSD-2-Clause"
LIC_FILES_CHKSUM = "file://LICENSE;md5=bebf0492502927bef0741aa04d1f35f5"

inherit module

SRCREV = "e969832d944b6e97c4ee61b0f4c867592c3dc1f9"
PV = "5.3.0"
SRC_URI = "git://github.com/ikwzm/udmabuf;protocol=https;branch=master"

S = "${WORKDIR}/git"

EXTRA_OEMAKE += "CONFIG_U_DMA_BUF=m"
