#!/bin/bash
ifname="enxf8e43b6426e9"  
service networking restart
iptables -A FORWARD --in-interface ${ifname}  -j ACCEPT
iptables --table nat -A POSTROUTING --out-interface ens33 -j MASQUERADE
iptables -I DOCKER-USER -o ${ifname} -i ens33 -j ACCEPT
sysctl -w net.ipv4.ip_forward=1
