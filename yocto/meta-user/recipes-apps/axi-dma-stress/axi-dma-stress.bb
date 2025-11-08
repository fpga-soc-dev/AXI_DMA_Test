LICENSE = "CLOSED"

SRC_URI = "\
    file://CMakeLists.txt \
    file://main.cpp \
    file://Utilities/FileHandle.cpp \
    file://Utilities/FileHandle.hpp \
    file://Utilities/Types.hpp      \  
    file://Utilities/FileMapping.cpp \
    file://Utilities/FileMapping.hpp \
    file://Utilities/Utils.hpp \
    file://Utilities/EPoll.cpp \
    file://Utilities/EPoll.hpp \   
    file://DmaBuffer/UDmaBuffer.cpp \
    file://DmaBuffer/UDmaBuffer.hpp \
    file://DmaBuffer/UDmaBufferAllocate.cpp \
    file://DmaBuffer/UDmaBufferAllocate.hpp \
    file://DmaBuffer/UDmaBufferIoctl.hpp \
    file://AxiDataGen/AxiDataGen.cpp \
    file://AxiDataGen/AxiDataGen.hpp \   
    file://AxiTrafficGen/AxiTrafficGen.cpp \
    file://AxiTrafficGen/AxiTrafficGen.hpp \
    file://UIO/Uio.cpp \
    file://UIO/Uio.hpp \
    file://AxiDma/AxiDma.cpp \
    file://AxiDma/AxiDma.hpp \
    file://AxiDma/AxiDmaBd.cpp \
    file://AxiDma/AxiDmaBd.hpp \
    file://AxiDma/AxiDmaHw.hpp \
    file://AxiDma/AxiDmaBdRing.hpp \
    file://AxiDma/AxiDmaBdRing.cpp \
    "

S = "${WORKDIR}"

FILESEXTRAPATHS:prepend := "${THISDIR}/files:"
do_rm_work[noexec] = "1"

DEPENDS += "fmt spdlog"

inherit cmake pkgconfig

do_install() {

    install -d ${D}/home/root
    install -m 0755 ${B}/axi-dma-stress-test ${D}/home/root/

}


FILES:${PN} += "/home/root/axi-dma-stress-test"
