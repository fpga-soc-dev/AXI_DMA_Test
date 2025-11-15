SUMMARY = "Image for pynq z1 board"

IMAGE_LINGUAS = " "

LICENSE = "MIT"

inherit core-image

IMAGE_INSTALL = "packagegroup-core-boot"
IMAGE_INSTALL:append = " ${CORE_IMAGE_EXTRA_INSTALL}"
IMAGE_INSTALL:append = " kernel-modules"
IMAGE_INSTALL:append = " openssh openssh-sshd openssh-sftp-server"
IMAGE_INSTALL:append = " devmem2"
IMAGE_INSTALL:append = " dma-proxy u-dma-buf axi-dma-stress"
IMAGE_INSTALL:append = " libgpiod libgpiod-dev libgpiod-tools"

inherit extrausers
EXTRA_USERS_PARAMS = "usermod -d /home/root root;"
