#!/usr/bin/env python3
"""
Certificate Provisioning Script for ESP32
Generates NVS partition binary with TLS certificates
"""

import argparse
import sys
import os
import subprocess
import csv
from pathlib import Path

def main():
    parser = argparse.ArgumentParser(description='Generate NVS partition with TLS certificates')
    parser.add_argument('--ca', help='Path to CA certificate file (default: certs_to_provision/ca.crt)')
    parser.add_argument('--cert', help='Path to client certificate file (default: certs_to_provision/device*.crt)')
    parser.add_argument('--key', help='Path to client key file (default: certs_to_provision/device*.key)')
    parser.add_argument('--device-name', help='Device name (8 chars max, optional)', default='')
    parser.add_argument('--zipcode', help='Zipcode (8 chars max, optional)', default='')
    parser.add_argument('--user-name', help='User name (16 chars max, optional)', default='')
    parser.add_argument('--output', help='Output NVS binary file', default='nvs_certs.bin')
    parser.add_argument('--partition-size', help='NVS partition size in hex', default='0x6000')
    parser.add_argument('--port', help='Serial port for flashing (e.g., COM3, /dev/ttyUSB0)', default='COM18')
    parser.add_argument('--no-flash', action='store_true', help='Skip flashing to device')
    
    args = parser.parse_args()
    
    # Load defaults from configs.csv if it exists and values not provided via CLI
    configs_file = 'configs.csv'
    if os.path.exists(configs_file):
        try:
            with open(configs_file, 'r', encoding='utf-8-sig') as f:  # utf-8-sig removes BOM
                reader = csv.DictReader(f)
                for row in reader:
                    # Strip all keys and values of whitespace
                    row = {k.strip(): v.strip() if v else '' for k, v in row.items()}
                    print(f"DEBUG: Cleaned row: {row}")
                    if 'name' in row and row['name']:
                        args.device_name = row['name']
                        print(f"DEBUG: Set device_name to '{args.device_name}'")
                    if 'zipcode' in row and row['zipcode'] and not args.zipcode:
                        args.zipcode = row['zipcode']
                    if 'user_name' in row and row['user_name'] and not args.user_name:
                        args.user_name = row['user_name']
                    break  # Only read first row
            print(f"Loaded configuration from {configs_file}")
            if args.device_name:
                print(f"  Device name: {args.device_name}")
            if args.zipcode:
                print(f"  Zipcode: {args.zipcode}")
            if args.user_name:
                print(f"  User name: {args.user_name}")
        except Exception as e:
            print(f"WARNING: Could not read {configs_file}: {e}")
    
    # Auto-detect files in certs_to_provision if not provided
    certs_dir = 'certs_to_provision'
    if not os.path.exists(certs_dir):
        print(f"ERROR: Directory '{certs_dir}' not found")
        return 1
    
    if not args.ca:
        args.ca = os.path.join(certs_dir, 'ca.crt')
    
    if not args.cert:
        cert_files = [f for f in os.listdir(certs_dir) if f.startswith('dev') and f[3:5].isdigit() and f.endswith('.crt')]
        if len(cert_files) == 1:
            args.cert = os.path.join(certs_dir, cert_files[0])
        elif len(cert_files) > 1:
            print(f"ERROR: Multiple dev##.crt files found. Use --cert to specify which one.")
            return 1
        else:
            print(f"ERROR: No dev##.crt file found in {certs_dir}. Use --cert to specify.")
            return 1
    
    if not args.key:
        key_files = [f for f in os.listdir(certs_dir) if f.startswith('dev') and f[3:5].isdigit() and f.endswith('.key')]
        if len(key_files) == 1:
            args.key = os.path.join(certs_dir, key_files[0])
        elif len(key_files) > 1:
            print(f"ERROR: Multiple dev##.key files found. Use --key to specify which one.")
            return 1
        else:
            print(f"ERROR: No dev##.key file found in {certs_dir}. Use --key to specify.")
            return 1
    
    # Check if files exist
    if not os.path.exists(args.ca):
        print(f"ERROR: CA certificate file not found: {args.ca}")
        return 1
    
    if not os.path.exists(args.cert):
        print(f"ERROR: Certificate file not found: {args.cert}")
        return 1
    
    if not os.path.exists(args.key):
        print(f"ERROR: Key file not found: {args.key}")
        return 1
    
    # Read certificate files
    with open(args.ca, 'rb') as f:
        ca_data = f.read()
    
    with open(args.cert, 'rb') as f:
        cert_data = f.read()
    
    with open(args.key, 'rb') as f:
        key_data = f.read()
    
    print(f"CA Certificate: {args.ca} ({len(ca_data)} bytes)")
    print(f"Client Certificate: {args.cert} ({len(cert_data)} bytes)")
    print(f"Client Key: {args.key} ({len(key_data)} bytes)")
    
    # Create CSV file for NVS partition generator
    csv_file = 'nvs_certs.csv'
    
    # Debug: Show what will be written
    print(f"\nNVS Configuration to write:")
    print(f"  device_name: '{args.device_name}' (len={len(args.device_name)})")
    print(f"  zipcode: '{args.zipcode}' (len={len(args.zipcode)})")
    print(f"  user_name: '{args.user_name}' (len={len(args.user_name)})")
    
    # Convert binary files to hex strings for CSV
    # Note: ESP-IDF nvs_partition_gen.py handles 'file' encoding type
    
    with open(csv_file, 'w') as f:
        f.write('key,type,encoding,value\n')
        f.write('device_cfg,namespace,,\n')
        
        # Use 'file' encoding to directly embed binary files
        # The path should be absolute
        ca_path = os.path.abspath(args.ca)
        cert_path = os.path.abspath(args.cert)
        key_path = os.path.abspath(args.key)
        
        f.write(f'ca_cert,file,binary,{ca_path}\n')
        f.write(f'client_cert,file,binary,{cert_path}\n')
        f.write(f'client_key,file,binary,{key_path}\n')
        
        if args.device_name:
            f.write(f'name,data,string,{args.device_name}\n')
        
        if args.zipcode:
            f.write(f'zipcode,data,string,{args.zipcode}\n')
        
        if args.user_name:
            f.write(f'user_name,data,string,{args.user_name}\n')
    
    print(f"\nCreated NVS CSV: {csv_file}")
    
    # Try to find nvs_partition_gen.py
    idf_path = os.environ.get('IDF_PATH')
    
    # If IDF_PATH not set, try common default location
    if not idf_path:
        username = os.environ.get('USERNAME', 'User')
        default_paths = [
            f"C:\\Users\\{username}\\esp\\v5.5.1\\esp-idf",
            f"C:\\Users\\{username}\\esp\\esp-idf",
            "C:\\esp-idf",
        ]
        for path in default_paths:
            if os.path.exists(path):
                idf_path = path
                print(f"Auto-detected IDF_PATH: {idf_path}")
                break
    
    if not idf_path:
        print("\nERROR: IDF_PATH environment variable not set and could not auto-detect")
        print("Please set IDF_PATH environment variable or ensure ESP-IDF is installed at:")
        print("  C:\\Users\\<username>\\esp\\v5.5.1\\esp-idf")
        print("  C:\\Users\\<username>\\esp\\esp-idf")
        print("  C:\\esp-idf")
        return 1
    
    nvs_gen_tool = os.path.join(idf_path, 'components', 'nvs_flash', 
                                 'nvs_partition_generator', 'nvs_partition_gen.py')
    
    if not os.path.exists(nvs_gen_tool):
        print(f"\nERROR: nvs_partition_gen.py not found at: {nvs_gen_tool}")
        return 1
    
    # Generate NVS partition binary
    print(f"\nGenerating NVS partition binary...")
    cmd = [
        sys.executable,
        nvs_gen_tool,
        'generate',
        csv_file,
        args.output,
        args.partition_size
    ]
    
    print(f"Running: {' '.join(cmd)}")
    result = subprocess.run(cmd)
    
    if result.returncode != 0:
        print(f"\nERROR: Failed to generate NVS partition")
        return 1
    
    print(f"\n✓ Successfully generated: {args.output}")
    
    # Flash to device if not skipped
    if not args.no_flash:
        print(f"\nFlashing NVS partition to device on {args.port}...")
        flash_cmd = [
            sys.executable,
            "-m", "esptool",
            "-p", args.port,
            "write_flash",
            "0x9000",
            args.output
        ]
        
        print(f"Running: {' '.join(flash_cmd)}")
        result = subprocess.run(flash_cmd)
        
        if result.returncode == 0:
            print(f"\n✓ Successfully flashed {args.output} to device!")
            print(f"\nYou can now reset your device. The certificates will be")
            print(f"loaded from NVS on the next boot.")
        else:
            print(f"\nWARNING: esptool flash command failed.")
            print(f"To flash manually, run:")
            print(f"  esptool.py -p {args.port} write_flash 0x9000 {args.output}")
            return 1
    else:
        print(f"\nTo flash to your device:")
        print(f"  esptool.py -p {args.port} write_flash 0x9000 {args.output}")
    
    return 0

if __name__ == '__main__':
    sys.exit(main())
