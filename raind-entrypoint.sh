#!/usr/bin/env bash
set -euo pipefail

TAP_DEV="${TAP_DEV:-tap0}"
TAP_ADDR="${TAP_ADDR:-10.0.2.1/24}"
TAP_CIDR="${TAP_CIDR:-10.0.2.0/24}"
WAN_DEV="${WAN_DEV:-eth0}"
HOST_HTTP_PORT="${HOST_HTTP_PORT:-8080}"
GUEST_HTTP_IP="${GUEST_HTTP_IP:-10.0.2.15}"
GUEST_HTTP_PORT="${GUEST_HTTP_PORT:-8080}"

# Requires container capabilities: NET_ADMIN and /dev/net/tun
if ! ip link show "$TAP_DEV" >/dev/null 2>&1; then
  ip tuntap add dev "$TAP_DEV" mode tap
fi

if ! ip -4 addr show dev "$TAP_DEV" | grep -q "${TAP_ADDR%/*}"; then
  ip addr add "$TAP_ADDR" dev "$TAP_DEV"
fi

ip link set "$TAP_DEV" up

# Enable IPv4 forwarding (best-effort in container namespace)
sysctl -w net.ipv4.ip_forward=1 >/dev/null 2>&1 || true

# NAT: tap0 subnet -> WAN device address
if ! iptables -C FORWARD -i "$TAP_DEV" -o "$WAN_DEV" -j ACCEPT >/dev/null 2>&1; then
  iptables -A FORWARD -i "$TAP_DEV" -o "$WAN_DEV" -j ACCEPT
fi
if ! iptables -C FORWARD -i "$WAN_DEV" -o "$TAP_DEV" -m conntrack --ctstate RELATED,ESTABLISHED -j ACCEPT >/dev/null 2>&1; then
  iptables -A FORWARD -i "$WAN_DEV" -o "$TAP_DEV" -m conntrack --ctstate RELATED,ESTABLISHED -j ACCEPT
fi
if ! iptables -t nat -C POSTROUTING -s "$TAP_CIDR" -o "$WAN_DEV" -j MASQUERADE >/dev/null 2>&1; then
  iptables -t nat -A POSTROUTING -s "$TAP_CIDR" -o "$WAN_DEV" -j MASQUERADE
fi

# DNAT: host:${HOST_HTTP_PORT} -> guest ${GUEST_HTTP_IP}:${GUEST_HTTP_PORT}
if ! iptables -t nat -C PREROUTING -i "$WAN_DEV" -p tcp --dport "$HOST_HTTP_PORT" -j DNAT --to-destination "${GUEST_HTTP_IP}:${GUEST_HTTP_PORT}" >/dev/null 2>&1; then
  iptables -t nat -A PREROUTING -i "$WAN_DEV" -p tcp --dport "$HOST_HTTP_PORT" -j DNAT --to-destination "${GUEST_HTTP_IP}:${GUEST_HTTP_PORT}"
fi
if ! iptables -C FORWARD -i "$WAN_DEV" -o "$TAP_DEV" -p tcp -d "$GUEST_HTTP_IP" --dport "$GUEST_HTTP_PORT" -m conntrack --ctstate NEW,RELATED,ESTABLISHED -j ACCEPT >/dev/null 2>&1; then
  iptables -A FORWARD -i "$WAN_DEV" -o "$TAP_DEV" -p tcp -d "$GUEST_HTTP_IP" --dport "$GUEST_HTTP_PORT" -m conntrack --ctstate NEW,RELATED,ESTABLISHED -j ACCEPT
fi
if ! iptables -C FORWARD -i "$TAP_DEV" -o "$WAN_DEV" -p tcp -s "$GUEST_HTTP_IP" --sport "$GUEST_HTTP_PORT" -m conntrack --ctstate RELATED,ESTABLISHED -j ACCEPT >/dev/null 2>&1; then
  iptables -A FORWARD -i "$TAP_DEV" -o "$WAN_DEV" -p tcp -s "$GUEST_HTTP_IP" --sport "$GUEST_HTTP_PORT" -m conntrack --ctstate RELATED,ESTABLISHED -j ACCEPT
fi

echo "== PRESS ENTER TO BOOT KERNEL =="
read -r

exec make start
