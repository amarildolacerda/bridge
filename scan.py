#!/usr/bin/env python3
import socket
import argparse
from concurrent.futures import ThreadPoolExecutor, as_completed

def get_local_ip():
    try:
        sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        sock.connect(("8.8.8.8", 80))
        ip = sock.getsockname()[0]
        sock.close()
        return ip
    except:
        return None

def check_port(ip, port=8123, timeout=1):
    try:
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        sock.settimeout(timeout)
        result = sock.connect_ex((ip, port))
        sock.close()
        return ip if result == 0 else None
    except:
        return None

def main():
    parser = argparse.ArgumentParser(description="Scan for open port on 192.168.1.1-253")
    parser.add_argument("-p", "--port", type=int, default=8123, help="Port to scan (default: 8123)")
    args = parser.parse_args()
    
    base_ip = "192.168.1."
    ips = [f"{base_ip}{i}" for i in range(1, 253)]
    
    local_ip = get_local_ip()
    print(f"Scanning {len(ips)} IPs for port {args.port}...")
    if local_ip:
        print(f"Local IP: {local_ip}")
    found = []
    
    with ThreadPoolExecutor(max_workers=50) as executor:
        futures = {executor.submit(check_port, ip, args.port): ip for ip in ips}
        for future in as_completed(futures):
            result = future.result()
            if result:
                marker = " <eu>" if result == local_ip else ""
                print(f"---------------------->Found: {result}:{args.port}{marker}")
                found.append(result)
            else:
                #marker = " <eu>" if result == local_ip else ""
                print(f"Not found: {ips[ips.index(futures[future])]}")
           
    if not found:
        print(f"No devices found with port {args.port} open")
    else:
        print(f"\nTotal found: {len(found)}")

if __name__ == "__main__":
    main()