FILESEXTRAPATHS:prepend := "${THISDIR}/files:"
SRC_URI:append = " file://system-user.dtsi"
SYSTEM_USER_DTSI ?= "system-user.dtsi"
 
do_configure:append() {
        cp ${WORKDIR}/${SYSTEM_USER_DTSI} ${B}/device-tree
        sed -i '/#include "zc702.dtsi"/d' ${B}/device-tree/system-top.dts
        echo "/include/ \"${SYSTEM_USER_DTSI}\"" >> ${B}/device-tree/system-top.dts
}
