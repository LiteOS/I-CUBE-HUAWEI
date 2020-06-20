################################################################################
# this is used for compile the macos socket
################################################################################

#configure the macos socket itself
ESP8266_SOCKET_IMP_SOURCE  = ${wildcard $(iot_link_root)/network/tcpip/fibocom_l716_cn_socket/*.c}
C_SOURCES += $(ESP8266_SOCKET_IMP_SOURCE)