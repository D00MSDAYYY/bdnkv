#!/usr/bin/env python3
"""Scan every local IPv4 address and optionally try SSH public-key login."""

import argparse
from concurrent.futures import ThreadPoolExecutor, as_completed
import ipaddress
import math
from pathlib import Path
import re
import shutil
import socket
import subprocess
import sys
import tempfile
import time

from connect_raspberry import local_network, wait_for_network


def probe(address, timeout):
    try:
        connection = socket.create_connection((address, 22), timeout=timeout)
    except ConnectionRefusedError:
        return address, False, "порт 22 закрыт"
    except TimeoutError:
        return address, False, "нет ответа на порту 22"
    except OSError as error:
        return address, False, str(error)

    with connection:
        deadline = time.monotonic() + timeout
        data = b""
        try:
            while len(data) < 8192 and time.monotonic() < deadline:
                connection.settimeout(max(0.01, deadline - time.monotonic()))
                chunk = connection.recv(1024)
                if not chunk:
                    break
                data += chunk
                for line in data.split(b"\n")[:-1]:
                    if line.startswith(b"SSH-"):
                        # Do not print terminal control characters from a network peer.
                        banner = "".join(chr(value) for value in line if 32 <= value < 127)
                        return address, True, banner
        except OSError:
            pass
    return address, True, "порт 22 открыт, SSH-баннер не получен"


def try_login(ssh, address, user, known_hosts, identity):
    args = [
        ssh, "-F", "/dev/null",
        "-o", "BatchMode=yes",
        "-o", "PreferredAuthentications=publickey",
        "-o", "StrictHostKeyChecking=accept-new",
        "-o", f"UserKnownHostsFile={known_hosts}",
        "-o", "ConnectTimeout=3",
        "-o", "ConnectionAttempts=1",
        "-o", "LogLevel=ERROR",
    ]
    if identity:
        args += ["-o", "IdentitiesOnly=yes", "-i", identity]
    args += ["-n", "-T", "-l", user, address, "true"]
    try:
        result = subprocess.run(args, capture_output=True, text=True, errors="replace", timeout=8)
    except subprocess.TimeoutExpired:
        return False, "тайм-аут SSH-входа"
    if result.returncode == 0:
        return True, f"вход {user}@{address} по ключу успешен"
    detail = next((line for line in result.stderr.splitlines() if line.strip()), "SSH-вход не выполнен")
    detail = "".join(char for char in detail if char.isprintable())
    return False, detail


def main():
    parser = argparse.ArgumentParser(description="Проверить порт 22 каждого адреса локальной сети; с --user попробовать SSH-вход по ключу.")
    parser.add_argument("--user", help="Логин для одной попытки входа по ключу на каждое устройство с открытым портом 22")
    parser.add_argument("--identity", help="Путь к приватному SSH-ключу (используется вместе с --user)")
    parser.add_argument("--subnet", type=ipaddress.IPv4Network, help="Подсеть для проверки, например 10.15.12.0/24")
    parser.add_argument("--wait", type=int, default=120, help="Ожидание отключения VPN в секундах (по умолчанию 120)")
    parser.add_argument("--timeout", type=float, default=1, help="Тайм-аут соединения и чтения баннера в секундах (по умолчанию 1)")
    parser.add_argument("--workers", type=int, default=32, help="Число параллельных проверок порта (по умолчанию 32)")
    args = parser.parse_args()
    if sys.platform != "darwin":
        parser.error("Скрипт предназначен для macOS")
    if args.wait < 0 or not math.isfinite(args.timeout) or args.timeout <= 0 or not 1 <= args.workers <= 128:
        parser.error("Нужны --wait >= 0, конечный --timeout > 0 и --workers от 1 до 128")
    if args.user is not None and not re.fullmatch(r"[a-zA-Z0-9_][a-zA-Z0-9_.-]*", args.user):
        parser.error("Некорректный --user")
    if args.identity and not args.user:
        parser.error("--identity требует --user")
    identity = str(Path(args.identity).expanduser().resolve()) if args.identity else None
    if identity and not Path(identity).is_file():
        parser.error("Файл ключа не найден")
    ssh = shutil.which("ssh") if args.user else None
    if args.user and not ssh:
        parser.error("Команда ssh не найдена")

    interface = wait_for_network(args.wait)
    own_address, detected_network = local_network(interface)
    network = args.subnet or detected_network
    if network.num_addresses > 1024:
        parser.error("Подсеть слишком большая: задайте через --subnet сеть не больше 1024 адресов")
    addresses = [str(address) for address in network.hosts() if str(address) != own_address]
    print(f"Проверяю {len(addresses)} адресов в {network}; адрес Mac {own_address} пропускаю.", flush=True)
    available = []
    with ThreadPoolExecutor(max_workers=args.workers) as pool:
        futures = [pool.submit(probe, address, args.timeout) for address in addresses]
        for future in as_completed(futures):
            address, is_open, detail = future.result()
            print(f"{address:15}  {detail}", flush=True)
            if is_open:
                available.append(address)
    available.sort(key=ipaddress.IPv4Address)
    print(f"\nПроверено адресов: {len(addresses)}. Открыт порт 22: {len(available)}.")
    if not available:
        print("Отсутствие ответа не доказывает, что устройство выключено: порт может фильтроваться.")
        return 1
    if not args.user:
        print("Для попытки входа на каждое найденное устройство запустите с --user ЛОГИН.")
        return 0

    succeeded = []
    # Preserve known host-key checks without adding every scanned host to the user's file.
    with tempfile.TemporaryDirectory(prefix="scan-ssh-") as directory:
        known_hosts = Path(directory) / "known_hosts"
        existing = Path.home() / ".ssh" / "known_hosts"
        known_hosts.write_bytes(existing.read_bytes() if existing.is_file() else b"")
        for address in available:
            print(f"Пробую SSH-вход: {args.user}@{address}…", flush=True)
            success, detail = try_login(ssh, address, args.user, known_hosts, identity)
            print(f"  {detail}", flush=True)
            if success:
                succeeded.append(address)
    print(f"\nУспешный вход по ключу: {len(succeeded)} из {len(available)}.")
    for address in succeeded:
        print(f"  ssh -l {args.user} {address}")
    return 0 if succeeded else 1


if __name__ == "__main__":
    try:
        sys.exit(main())
    except (RuntimeError, OSError) as error:
        print(f"Ошибка: {error}", file=sys.stderr)
        sys.exit(1)
    except KeyboardInterrupt:
        print("\nПроверка отменена.", file=sys.stderr)
        sys.exit(130)
