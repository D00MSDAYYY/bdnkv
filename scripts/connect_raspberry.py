#!/usr/bin/env python3
"""Wait for a direct macOS network route, find a Raspberry Pi, and open SSH."""

import argparse
from concurrent.futures import ThreadPoolExecutor
import ipaddress
import os
import re
import shutil
import socket
import subprocess
import sys
import time


def command(*args, timeout=5):
    try:
        result = subprocess.run(args, capture_output=True, text=True, timeout=timeout)
    except subprocess.TimeoutExpired:
        return ""
    return result.stdout if result.returncode == 0 else ""


def route_interface():
    # Looking up a public address also catches VPNs with two /1 routes.
    output = command("/sbin/route", "-n", "get", "1.1.1.1")
    match = re.search(r"^\s*interface:\s*(\S+)", output, re.MULTILINE)
    return match.group(1) if match else None


def wait_for_network(seconds):
    deadline = time.monotonic() + seconds
    print("Отключите VPN. Жду прямой сетевой маршрут…", flush=True)
    while True:
        interface = route_interface()
        if interface and not interface.startswith(("utun", "tun", "tap", "ppp", "ipsec")):
            return interface
        if time.monotonic() >= deadline:
            raise RuntimeError("VPN-маршрут ещё активен или сеть недоступна. Отключите VPN и повторите запуск.")
        time.sleep(min(2, max(0, deadline - time.monotonic())))


def local_network(interface):
    output = command("/sbin/ifconfig", interface)
    match = re.search(r"\binet (\d+\.\d+\.\d+\.\d+) netmask (0x[0-9a-fA-F]+)", output)
    if not match:
        raise RuntimeError(f"Не найден IPv4-адрес интерфейса {interface}.")
    address, mask = match.groups()
    netmask = str(ipaddress.IPv4Address(int(mask, 16)))
    return address, ipaddress.IPv4Network(f"{address}/{netmask}", strict=False)


def resolve_ipv4(host):
    try:
        return [str(ipaddress.IPv4Address(host))]
    except ipaddress.AddressValueError:
        output = command("/usr/bin/dscacheutil", "-q", "host", "-a", "name", host, timeout=5)
        return re.findall(r"^ip_address: (\d+\.\d+\.\d+\.\d+)$", output, re.MULTILINE)


def ssh_available(address):
    try:
        with socket.create_connection((address, 22), timeout=1):
            return True
    except OSError:
        return False


def choose_address(addresses):
    # An open SSH port alone does not identify Raspberry hardware.
    print("Найдены устройства с портом SSH. Выберите адрес своей Raspberry Pi:")
    for number, address in enumerate(addresses, start=1):
        print(f"  {number}. {address}")
    if not sys.stdin.isatty():
        raise RuntimeError("Для выбора нужен терминал. Или укажите адрес через --host.")
    while True:
        choice = input("Номер устройства (Enter — отмена): ").strip()
        if not choice:
            raise RuntimeError("Подключение отменено.")
        if choice.isdecimal() and 1 <= int(choice) <= len(addresses):
            return addresses[int(choice) - 1]
        print("Введите номер из списка.")


def find_raspberry(host, network, own_address):
    hostname = host or "bkzs.local"
    print(f"Проверяю {hostname}:22…", flush=True)
    for address in resolve_ipv4(hostname):
        if ssh_available(address):
            print(f"{hostname} доступен: {address}")
            return address
    if host:
        raise RuntimeError(f"{host}:22 недоступен. Проверьте адрес и включён ли SSH на Raspberry Pi.")

    if network.num_addresses > 1024:
        raise RuntimeError("Сеть слишком большая для автопоиска. Укажите --host или меньшую сеть через --subnet.")
    print(f"Имя не найдено или SSH недоступен. Ищу порт 22 в {network}…", flush=True)
    addresses = [str(address) for address in network.hosts() if str(address) != own_address]
    with ThreadPoolExecutor(max_workers=32) as pool:
        found = [address for address, available in zip(addresses, pool.map(ssh_available, addresses)) if available]
    if not found:
        raise RuntimeError("Raspberry Pi не найдена. Проверьте питание, общую сеть и включён ли SSH; можно указать --host IP.")
    return choose_address(found)


def main():
    parser = argparse.ArgumentParser(description="Подключение к Raspberry Pi по SSH после отключения VPN (macOS).")
    parser.add_argument("--user", help="Логин на Raspberry Pi; иначе скрипт спросит его")
    parser.add_argument("--host", help="IP или имя Raspberry Pi; по умолчанию bkzs.local и поиск в локальной сети")
    parser.add_argument("--subnet", type=ipaddress.IPv4Network, help="Подсеть для поиска, например 192.168.1.0/24")
    parser.add_argument("--wait", type=int, default=120, metavar="SECONDS", help="Сколько ждать отключения VPN (по умолчанию 120 с)")
    args = parser.parse_args()
    if args.wait < 0:
        parser.error("--wait должен быть неотрицательным")
    if sys.platform != "darwin":
        parser.error("Этот скрипт предназначен для macOS")
    ssh = shutil.which("ssh")
    if not ssh:
        raise RuntimeError("Не найдена команда ssh.")

    interface = wait_for_network(args.wait)
    own_address, network = local_network(interface)
    print(f"Сеть: {interface}, Mac {own_address}, подсеть {args.subnet or network}")
    address = find_raspberry(args.host, args.subnet or network, own_address)
    # Recheck in case the VPN reconnected during discovery.
    if route_interface() != interface:
        raise RuntimeError("Сетевой маршрут изменился. Повторите запуск после отключения VPN.")
    user = args.user
    if user is None:
        if not sys.stdin.isatty():
            raise RuntimeError("Укажите логин на Raspberry Pi через --user.")
        user = input("Логин на Raspberry Pi: ").strip()
    if not user or not re.fullmatch(r"[a-zA-Z0-9_][a-zA-Z0-9_.-]*", user):
        raise RuntimeError("Укажите корректный логин через --user.")
    print(f"Подключаюсь: {user}@{address}", flush=True)
    os.execv(ssh, [ssh, "-o", "ConnectTimeout=10", "-o", "ServerAliveInterval=15", "-l", user, address])


if __name__ == "__main__":
    try:
        main()
    except (RuntimeError, OSError, ValueError) as error:
        print(f"Ошибка: {error}", file=sys.stderr)
        sys.exit(1)
    except (KeyboardInterrupt, EOFError):
        print("\nПодключение отменено.", file=sys.stderr)
        sys.exit(130)
