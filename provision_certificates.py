#!/usr/bin/env python3
"""
Certificate Provisioning Script for ESP32
Generates NVS partition binary with TLS certificates
"""

import argparse
import sys
import os
import subprocess
from pathlib import Path

def main():
    parser = argparse.ArgumentParser(description='Generate NVS partition with TLS certificates')
    parser.add_argument('--cert', required=True, help='Path to client certificate file')
    parser.add_argument('--key', required=True, help='Path to client key file')
    parser.add_argument('--device-name', help='Device name (8 chars max, optional)', default='')
    parser.add_argument('--zipcode', help='Zipcode (8 chars max, optional)', default='')
    parser.add_argument('--user-name', help='User name (16 chars max, optional)', default='')
    parser.add_argument('--output', help='Output NVS binary file', default='nvs_certs.bin')
    parser.add_argument('--partition-size', help='NVS partition size in hex', default='0x6000')
    
    args = parser.parse_args()
    
    # Check if files exist
    if not os.path.exists(args.cert):
        print(f"ERROR: Certificate file not found: {args.cert}")
        return 1
    
    if not os.path.exists(args.key):
        print(f"ERROR: Key file not found: {args.key}")
        return 1
    
    # Read certificate and key
    with open(args.cert, 'rb') as f:
        cert_data = f.read()
    
    with open(args.key, 'rb') as f:
        key_data = f.read()
    
    print(f"Certificate: {args.cert} ({len(cert_data)} bytes)")
    print(f"Key: {args.key} ({len(key_data)} bytes)")
    
    # Create CSV file for NVS partition generator
    csv_file = 'nvs_certs.csv'
    
    # Convert binary files to hex strings for CSV
    # Note: ESP-IDF nvs_partition_gen.py handles 'file' encoding type
    
    with open(csv_file, 'w') as f:
        f.write('key,type,encoding,value\n')
        f.write('device_cfg,namespace,,\n')
        
        # Use 'file' encoding to directly embed binary files
        # The path should be relative to the CSV file
        cert_path = os.path.abspath(args.cert)
        key_path = os.path.abspath(args.key)
        
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
    if not idf_path:
        print("\nERROR: IDF_PATH environment variable not set")
        print("Please source the ESP-IDF environment:")
        print("  . $IDF_PATH/export.sh (Linux/Mac)")
        print("  or")
        print("  . $IDF_PATH/export.ps1 (Windows)")
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
    print(f"\nTo flash to your device:")
    print(f"  esptool.py -p COM3 write_flash 0x9000 {args.output}")
    print(f"\n  (Replace COM3 with your port and 0x9000 with your NVS partition offset)")
    print(f"\nTo find your NVS partition offset, check partitions.csv")
    
    return 0

if __name__ == '__main__':
    sys.exit(main())
