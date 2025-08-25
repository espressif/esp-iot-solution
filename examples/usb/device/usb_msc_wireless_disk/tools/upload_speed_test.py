#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Apache-2.0
# -*- coding: utf-8 -*-

import os
import time
import argparse
import requests
import hashlib
import struct
from datetime import datetime
import random
import string
import statistics
import sys
import io

# Check required packages
REQUIRED_PACKAGES = {
    'requests_toolbelt': 'Chunked upload support'
}

# Store imported optional modules
OPTIONAL_MODULES = {}

# Logger class that outputs to both console and log file
class Logger:
    def __init__(self, log_file=None):
        self.terminal = sys.stdout
        self.log_file = None

        # If no log file specified, auto-generate one based on current time
        if log_file is None:
            # Create log directory in the same directory as the script
            script_dir = os.path.dirname(os.path.abspath(__file__))
            log_dir = os.path.join(script_dir, 'logs')
            if not os.path.exists(log_dir):
                os.makedirs(log_dir)
            timestamp = datetime.now().strftime('%Y%m%d_%H%M%S')
            log_file = os.path.join(log_dir, f'upload_test_{timestamp}.log')

        try:
            self.log_file = open(log_file, 'a', encoding='utf-8')
            print(f'Log file created: {log_file}')
        except Exception as e:
            print(f'Unable to create log file: {e}')
            self.log_file = None

    def write(self, message):
        self.terminal.write(message)
        if self.log_file is not None:
            self.log_file.write(message)
            self.log_file.flush()  # Write to file immediately

    def flush(self):
        self.terminal.flush()
        if self.log_file is not None:
            self.log_file.flush()

    def close(self):
        if self.log_file is not None:
            self.log_file.close()
            self.log_file = None

def import_optional_module(module_name):
    '''
    Try to import optional module, return None if not available
    '''
    if module_name in OPTIONAL_MODULES:
        return OPTIONAL_MODULES[module_name]

    try:
        module = __import__(module_name)
        OPTIONAL_MODULES[module_name] = module
        return module
    except ImportError:
        OPTIONAL_MODULES[module_name] = None
        return None

def check_and_import_package(package_name, description=None):
    '''
    Check and import specified package, prompt for installation if not found
    '''
    module = import_optional_module(package_name)
    if module is None:
        desc_text = f' ({description})' if description else ''
        print(f'Warning: {package_name} module not found{desc_text}.')
        print(f'Some features may be limited. Please run the following command to install:')
        print(f'  pip install {package_name}')
        return False
    return True

def check_server_connectivity(url, timeout=5):
    '''
    Check server connectivity
    :param url: Server URL
    :param timeout: Timeout in seconds
    :return: (connection successful, response content/error message)
    '''
    base_url = url

    # Find the end position of domain/IP part in URL
    domain_end = base_url.find('/', base_url.find('//') + 2)
    if domain_end != -1:
        base_url = base_url[:domain_end]

    # Ensure URL ends with '/'
    if not base_url.endswith('/'):
        base_url += '/'

    try:
        print(f'Checking server connection: {base_url}')
        response = requests.get(base_url, timeout=timeout)
        if response.status_code == 200:
            print(f'Server connection normal, status code: {response.status_code}')
            return True, 'Server connection normal'
        else:
            print(f'Server returned non-200 status code: {response.status_code}')
            return False, f'HTTP error: {response.status_code}'
    except requests.exceptions.ConnectionError as e:
        print(f'Connection error: {e}')
        return False, f'Connection error: {e}'
    except requests.exceptions.Timeout as e:
        print(f'Connection timeout: {e}')
        return False, f'Connection timeout: {e}'
    except requests.exceptions.RequestException as e:
        print(f'Request error: {e}')
        return False, f'Request error: {e}'

def generate_verifiable_file(size_mb=5, filename=None, file_id=0):
    '''
    Generate verifiable test file of specified size, file format:
    - File header (16 bytes): 'ESP_TEST_FILE' + 4-byte file ID + 2-byte checksum
    - Data blocks (1MB each): block number (4 bytes) + predictable content (remaining bytes)
    - File footer (16 bytes): MD5 checksum

    :param size_mb: File size in MB
    :param filename: Filename, auto-generate if not specified
    :param file_id: File ID for generating different file content
    :return: Tuple (filename, md5 checksum)
    '''
    if filename is None:
        # Generate random filename
        random_str = ''.join(random.choices(string.ascii_letters + string.digits, k=8))
        timestamp = datetime.now().strftime('%Y%m%d_%H%M%S')
        filename = f'test_file_{timestamp}_{random_str}.dat'

    print(f'Generating {size_mb}MB verifiable test file: {filename}')

    # Calculate actual file size
    header_size = 16
    footer_size = 16
    block_size = 1024 * 1024  # 1MB block

    # Create file
    with open(filename, 'wb') as f:
        # Write file header - 'ESP_TEST_FILE' + file ID + checksum placeholder
        header = b'ESP_TEST_FILE'
        header += struct.pack('<I', file_id)  # 4-byte file ID
        header += struct.pack('<H', 0)  # 2-byte checksum placeholder, fill later
        f.write(header)

        # Create MD5 hash object to calculate checksum of entire file
        md5_hash = hashlib.md5()
        md5_hash.update(header)

        # Write data blocks
        for i in range(size_mb):
            # Create a 1MB data block
            # Each block starts with 4-byte block number for easy verification
            block_data = struct.pack('<I', i)

            # Remaining part of block uses predictable repeating pattern
            # Use block number and file ID to generate seed
            seed = (file_id * 10000) + i
            random.seed(seed)

            # Generate a few random numbers for each block, then repeat to fill 1MB
            pattern = bytearray()
            for _ in range(64):  # Generate 256-byte random pattern
                pattern.extend(struct.pack('<I', random.randint(0, 0xFFFFFFFF)))

            # Copy pattern until block is full (minus 4 bytes for block number)
            remaining_size = block_size - len(block_data)
            pattern_size = len(pattern)
            for j in range(0, remaining_size, pattern_size):
                if j + pattern_size <= remaining_size:
                    block_data += pattern
                else:
                    block_data += pattern[:remaining_size - j]

            # Write block and update MD5
            f.write(block_data)
            md5_hash.update(block_data)

        # Calculate MD5 checksum
        file_md5 = md5_hash.digest()

        # Write file footer - MD5 checksum
        f.write(file_md5)

        # Go back to header position and write checksum
        header_checksum = sum(header[:-2]) & 0xFFFF
        f.seek(len(header) - 2)
        f.write(struct.pack('<H', header_checksum))

    # Return filename and hexadecimal representation of MD5 checksum
    return filename, md5_hash.hexdigest()


def generate_random_file(size_mb=5, filename=None, file_id=0):
    '''
    Generate random data file of specified size, kept for backward compatibility
    Now internally calls generate_verifiable_file

    :param size_mb: File size in MB
    :param filename: Filename, auto-generate if not specified
    :param file_id: File ID for generating different file content
    :return: Filename
    '''
    filename, _ = generate_verifiable_file(size_mb, filename, file_id)
    return filename

def extract_md5_from_filename(filename):
    '''
    Extract MD5 checksum from filename
    Filename format: test_XXX_YYYYYYY_TIMESTAMP.dat or test_XXX_MD5_TIMESTAMP.dat

    :param filename: Filename
    :return: Extracted MD5 checksum, None if not found
    '''
    try:
        # Parse filename
        base_name = os.path.basename(filename)
        parts = base_name.split('_')

        # MD5 format is usually 8 or 32 hexadecimal characters
        for part in parts:
            if len(part) == 8 and all(c in '0123456789abcdefABCDEF' for c in part):
                return part

        return None
    except:
        return None


def verify_file(file_path, expected_md5=None):
    '''
    Verify file integrity
    :param file_path: Path to file to verify
    :param expected_md5: Expected MD5 checksum (optional)
    :return: (verification passed, actual MD5 value)
    '''
    if not os.path.exists(file_path):
        print(f'File does not exist: {file_path}')
        return False, None

    # If no expected MD5 provided, try to extract from filename
    if not expected_md5:
        expected_md5 = extract_md5_from_filename(file_path)
        if expected_md5:
            print(f'MD5 extracted from filename: {expected_md5}')

    try:
        with open(file_path, 'rb') as f:
            # Read file header
            header = f.read(16)
            if len(header) < 16 or header[:12] != b'ESP_TEST_FILE':
                print(f'File format incorrect, not an ESP test file: {file_path}')
                print('Trying regular MD5 verification...')

                # If not an ESP test file, use regular MD5 calculation
                f.seek(0)
                md5_hash = hashlib.md5()
                while True:
                    data = f.read(1024 * 1024)
                    if not data:
                        break
                    md5_hash.update(data)
                computed_md5 = md5_hash.hexdigest()
            else:
                # Parse file header
                file_id = struct.unpack('<I', header[12:16])[0]
                print(f'File ID: {file_id}')

                # Get file size
                file_size = os.path.getsize(file_path)
                if file_size < 32:  # Minimum 32 bytes for header + footer
                    print(f'File too small, not a valid test file: {file_path}')
                    return False, None

                # Calculate MD5 of data portion
                md5_hash = hashlib.md5()
                md5_hash.update(header)

                # Read and calculate hash of data blocks
                f.seek(16)  # Skip header
                remaining = file_size - 16 - 16  # Subtract header and footer
                while remaining > 0:
                    chunk_size = min(1024 * 1024, remaining)
                    block = f.read(chunk_size)
                    if not block:
                        break
                    md5_hash.update(block)
                    remaining -= len(block)

                # Computed MD5
                computed_md5 = md5_hash.hexdigest()

                # Read stored MD5 from file and compare
                f.seek(file_size - 16)
                stored_md5 = f.read(16)
                stored_md5_hex = ''.join(f'{b:02x}' for b in stored_md5)
                print(f'MD5 stored in file: {stored_md5_hex}')
                if computed_md5[:32] != stored_md5_hex:
                    print(f'Internal MD5 verification failed!')
                    print(f'Computed MD5: {computed_md5}')
                    print(f'Stored MD5: {stored_md5_hex}')

        # If expected MD5 provided, compare
        if expected_md5:
            # Only compare the provided MD5 length (might be full 32-bit or first 8-bit)
            match_len = len(expected_md5)
            if computed_md5[:match_len] == expected_md5:
                print(f'File verification successful: {file_path}')
                return True, computed_md5
            else:
                print(f'File verification failed: {file_path}')
                print(f'Expected MD5: {expected_md5}')
                print(f'Actual MD5: {computed_md5[:match_len]}')
                return False, computed_md5
        else:
            print(f'File MD5: {computed_md5}')

        return True, computed_md5

    except Exception as e:
        print(f'Error during verification: {e}')
        return False, None


def upload_file(url, file_path, test_index=0, file_md5=None, max_retries=3, retry_delay=5, timeout=60, chunk_size=None):
    '''
    Upload file and measure speed
    :param url: Upload target URL
    :param file_path: Local file path
    :param test_index: Test sequence number, used for file renaming
    :param file_md5: File MD5 checksum, used in filename
    :param max_retries: Maximum retry attempts
    :param retry_delay: Retry interval in seconds
    :param timeout: Request timeout in seconds
    :param chunk_size: If set, use chunked upload (in bytes, e.g., 1MB=1048576)
    :return: Tuple (file size MB, upload time seconds, speed MB/s, remote filename)
    '''
    file_size = os.path.getsize(file_path)
    file_size_mb = file_size / (1024 * 1024)

    print(f'Starting file upload: {file_path} (size: {file_size_mb:.2f}MB)')
    start_time = time.time()
    timestamp_ms = int(start_time * 1000)

    # Use filename with timestamp and MD5 for upload, avoid duplicate files on ESP side
    # If MD5 provided, include first 8 characters in filename for easy identification and verification
    md5_part = f'_{file_md5[:8]}' if file_md5 else ''
    upload_filename = f'test_{test_index:03d}{md5_part}_{timestamp_ms}.dat'

    # Ensure correct URL format - ESP server needs filename in URL
    # Normalize base URL
    base_url = url
    if base_url.endswith('/'):
        base_url = base_url[:-1]

    # Build correct upload URL - ESP server expects format '/upload/filename'
    upload_url = f'{base_url}/upload/{upload_filename}'

    print(f'Upload to URL: {upload_url}')

    # Set request headers to ensure correct HTTP request format
    headers = {
        'User-Agent': 'ESP-Upload-Test/1.0',
        'Accept': '*/*',
        'Connection': 'close',  # Add close connection header to avoid keep-alive issues
        'Content-Type': 'multipart/form-data'  # Explicitly set to multipart/form-data type
    }

    # To resolve 'Method '3' not allowed' issue, need to ensure correct request method format
    # ESP32 HTTP server expects standard HTTP POST requests, may be sensitive to certain header formats

    # Reasonable chunk size for ESP32 - based on TCP window size (CONFIG_LWIP_TCP_WND_DEFAULT)
    # ESP device TCP window configured to 64KB, can use larger chunk sizes
    safe_chunk_size = 48 * 1024  # 48KB, optimized value based on 64KB TCP window
    if chunk_size is not None and chunk_size > 0:
        # If user specified chunk size, use user-specified value
        safe_chunk_size = chunk_size * 1024
    attempts = 0
    while attempts < max_retries:
        try:
            with open(file_path, 'rb') as f:
                # Adjust chunked upload logic, optimized based on TCP window size (64KB)
                if file_size > 128 * 1024:  # Use chunked upload for files larger than 128KB
                    # Use the set safe chunk size
                    actual_chunk_size = safe_chunk_size

                    print(f'Using chunked upload, chunk size: {actual_chunk_size/1024:.2f}KB')

                    # Use Python's requests library directly for chunked upload
                    # Read file content into memory
                    file_content = f.read()

                    # Use custom chunked reader
                    class ChunkedReader:
                        def __init__(self, data, chunk_size):
                            self.data = data
                            self.chunk_size = chunk_size
                            self.position = 0
                            self.total_size = len(data)

                        def read(self, size=-1):
                            if self.position >= self.total_size:
                                return b''

                            chunk = self.data[self.position:self.position + self.chunk_size]
                            self.position += len(chunk)
                            return chunk

                    # Create file reader
                    reader = ChunkedReader(file_content, actual_chunk_size)

                    # Use chunked upload
                    files = {'file': (upload_filename, reader, 'application/octet-stream')}
                    response = requests.post(
                        upload_url,
                        files=files,
                        headers=headers,
                        timeout=timeout
                    )
                else:
                    # Use standard upload method for small files
                    files = {'file': (upload_filename, f, 'application/octet-stream')}
                    response = requests.post(
                        upload_url,
                        files=files,
                        headers=headers,
                        timeout=timeout
                    )

                # Check response status code
                if response.status_code == 405:
                    print(f'HTTP 405 error: Method not allowed. Server may not support POST requests or URL format is incorrect.')
                    print(f'Server response: {response.text}')
                    raise requests.exceptions.RequestException('HTTP 405 Method Not Allowed')

                response.raise_for_status()  # Confirm request success

            # Upload successful, break out of retry loop
            break

        except requests.exceptions.ConnectionError as e:
            attempts += 1
            if attempts < max_retries:
                print(f'Connection error: {e}')
                print(f'Attempting to reconnect... ({attempts}/{max_retries})')
                # After connection error, add longer wait time to give server enough time to recover
                time.sleep(retry_delay * 2)
            else:
                print(f'Upload failed: Connection error reached maximum retry attempts ({max_retries}): {e}')
                return file_size_mb, 0, 0, None

        except requests.exceptions.Timeout as e:
            attempts += 1
            if attempts < max_retries:
                print(f'Upload timeout: {e}')
                print(f'Attempting to re-upload... ({attempts}/{max_retries})')
                time.sleep(retry_delay)
            else:
                print(f'Upload failed: Timeout reached maximum retry attempts ({max_retries}): {e}')
                return file_size_mb, 0, 0, None

        except requests.exceptions.RequestException as e:
            attempts += 1
            if attempts < max_retries:
                print(f'Request error: {e}')
                print(f'Attempting to re-upload... ({attempts}/{max_retries})')
                time.sleep(retry_delay)
            else:
                print(f'Upload failed: Request error reached maximum retry attempts ({max_retries}): {e}')
                return file_size_mb, 0, 0, None

    end_time = time.time()
    upload_time = end_time - start_time
    speed = file_size_mb / upload_time if upload_time > 0 else 0

    if attempts > 0:
        print(f'Upload successful (after {attempts} retries): {file_size_mb:.2f}MB took {upload_time:.2f}s, speed: {speed:.2f}MB/s')
    else:
        print(f'Upload completed: {file_size_mb:.2f}MB took {upload_time:.2f}s, speed: {speed:.2f}MB/s')
    return file_size_mb, upload_time, speed, upload_filename

def main():
    parser = argparse.ArgumentParser(description='ESP HTTP Server File Upload Speed Test Tool')
    parser.add_argument('--url', type=str, default='http://192.168.4.1',
                        help='Server address (default: http://192.168.4.1)')
    parser.add_argument('--file-size', type=int, default=10,
                        help='Size of each test file in MB (default: 10)')
    parser.add_argument('--total-size', type=int, default=1000,
                        help='Total upload data size in MB (default: 1000, i.e., 1GB for 100 uploads)')
    parser.add_argument('--keep-files', action='store_true',
                        help='Keep generated test files after testing')
    parser.add_argument('--test-files', type=int, default=2,
                        help='Number of test files to generate (default: 2)')
    parser.add_argument('--verify', action='store_true',
                        help='Generate verifiable files and add MD5 info to filenames')
    parser.add_argument('--verify-file', type=str,
                        help='Verify specified file instead of running upload test')
    parser.add_argument('--log-file', type=str,
                        help='Save upload records to specified log file')
    parser.add_argument('--max-retries', type=int, default=5,
                        help='Maximum retry attempts on upload failure (default: 5)')
    parser.add_argument('--retry-delay', type=int, default=10,
                        help='Retry interval in seconds (default: 10)')
    parser.add_argument('--timeout', type=int, default=120,
                        help='Upload request timeout in seconds (default: 120)')
    parser.add_argument('--chunk-size', type=int, default=48,
                        help='Chunk size for chunked upload in KB (default: 48, optimized for 64KB TCP window), set to 0 to disable chunking')
    parser.add_argument('--console-log', type=str, default=None,
                        help='Save console output to specified log file, default uses auto-generated filename based on current time')
    parser.add_argument('--esp-friendly', action='store_true', default=True,
                        help='Use ESP device-friendly upload parameters (default: enabled)')

    args = parser.parse_args()

    # Set up logger to output to both console and log file
    logger = Logger(args.console_log)
    sys.stdout = logger  # Replace standard output

    # If verify-file parameter specified, run verification and exit
    if args.verify_file:
        verify_file(args.verify_file)
        return

    # Check server connectivity
    print('Checking server connectivity...')
    server_ok, server_msg = check_server_connectivity(args.url, timeout=5)
    if not server_ok:
        print(f'Warning: Server connectivity check failed: {server_msg}')
        print('Do you still want to continue testing? (y/n)')
        if input().lower() != 'y':
            print('Test cancelled')
            return

    # Calculate number of files to upload
    num_uploads = args.total_size // args.file_size
    num_test_files = min(args.test_files, num_uploads)  # Don't generate more files than needed

    print(f'=== ESP HTTP File Upload Speed Test ===')
    print(f'Target URL: {args.url}')
    print(f'File size: {args.file_size}MB')
    print(f'Total data: {args.total_size}MB ({args.total_size/1024:.1f}GB)')
    print(f'Upload count: {num_uploads}')
    print(f'Test files: {num_test_files}')
    print(f'File verification: {"Enabled" if args.verify else "Disabled"}')
    print(f'Error retry: Max {args.max_retries} times, interval {args.retry_delay} seconds')
    print(f'Request timeout: {args.timeout} seconds')

    # Display chunked upload settings
    if args.chunk_size is not None and args.chunk_size > 0:
        print(f'Chunked upload: Enabled, chunk size {args.chunk_size}KB ({args.chunk_size/1024:.2f}MB)')
        # Check dependency libraries
        if not check_and_import_package('requests_toolbelt', 'Chunked upload support'):
            print('Warning: requests_toolbelt library not installed, chunked upload feature will be unavailable')
    else:
        print(f'Chunked upload: Disabled')

    # Display log file information
    if hasattr(sys.stdout, 'log_file') and sys.stdout.log_file:
        log_path = sys.stdout.log_file.name
        print(f'Terminal output log: {log_path}')

    print('==============================')

    # Prepare result statistics data
    speeds = []
    upload_times = []
    total_start_time = time.time()
    test_files = []
    file_md5s = []
    uploaded_files = []

    # If log file specified, open and write header information
    log_file = None
    stats_log_file = None
    csv_log_file = None

    if args.log_file:
        try:
            # Process filename for generating multiple log files
            log_base = os.path.splitext(args.log_file)[0]
            log_ext = os.path.splitext(args.log_file)[1] or '.csv'

            # Detailed log file (contains all upload information)
            log_file = open(args.log_file, 'w')
            log_file.write('Index,Filename,File Size(MB),MD5 Checksum,Remote Filename,Upload Time(s),Speed(MB/s),Timestamp\n')

            # Statistics data file (for plotting, one upload record per line)
            stats_filename = f'{log_base}_stats{log_ext}'
            stats_log_file = open(stats_filename, 'w')
            stats_log_file.write('Index,Timestamp,Upload Time(s),Speed(MB/s),File Size(MB)\n')

            # Summary CSV (suitable for Excel/data analysis tools)
            csv_filename = f'{log_base}_summary{log_ext}'
            csv_log_file = open(csv_filename, 'w')
            csv_log_file.write('Metric,Value\n')

            print(f'Log files created:')
            print(f' - Detailed log: {args.log_file}')
            print(f' - Statistics data: {stats_filename}')
            print(f' - Summary report: {csv_filename}')

        except Exception as e:
            print(f'Unable to open log file: {e}')
            log_file = None
            stats_log_file = None
            csv_log_file = None

    try:
        # First generate all test files
        print('Generating test files...')
        for i in range(num_test_files):
            file_name = f'test_file_{i+1}.dat'

            # Generate file and get MD5 checksum
            if args.verify:
                filename, md5_sum = generate_verifiable_file(
                    size_mb=args.file_size,
                    filename=file_name,
                    file_id=i
                )
                test_files.append(filename)
                file_md5s.append(md5_sum)
                print(f'Generated file {i+1}/{num_test_files}: {filename} (MD5: {md5_sum})')
            else:
                test_files.append(generate_random_file(
                    size_mb=args.file_size,
                    filename=file_name,
                    file_id=i
                ))
                file_md5s.append(None)
                print(f'Generated file {i+1}/{num_test_files}: {test_files[i]}')

        # Then perform upload test
        print('\nStarting upload test...')
        for i in range(num_uploads):
            print(f'\nUpload {i+1}/{num_uploads}')

            # Cycle through test files
            file_index = i % num_test_files

            # Upload and record results
            # If chunk size specified, convert to bytes
            chunk_size = None
            if args.chunk_size is not None and args.chunk_size > 0:
                chunk_size = args.chunk_size * 1024  # Convert to bytes

            _, upload_time, speed, remote_filename = upload_file(
                args.url,
                test_files[file_index],
                i+1,
                file_md5s[file_index] if args.verify else None,
                args.max_retries,
                args.retry_delay,
                args.timeout,
                chunk_size
            )

            if speed > 0:  # Only record successful uploads
                speeds.append(speed)
                upload_times.append(upload_time)

                # Record upload file information
                if remote_filename:
                    uploaded_files.append({
                        'index': i+1,
                        'local_file': test_files[file_index],
                        'size_mb': args.file_size,
                        'md5': file_md5s[file_index],
                        'remote_file': remote_filename,
                        'upload_time': upload_time,
                        'speed': speed,
                        'timestamp': time.time()
                    })

                    # If log file exists, write record
                    current_timestamp = int(time.time())
                    if log_file:
                        # Detailed log
                        log_file.write(f'{i+1},{test_files[file_index]},{args.file_size},' +
                                      f'{file_md5s[file_index] or "N/A"},{remote_filename},' +
                                      f'{upload_time:.2f},{speed:.2f},{current_timestamp}\n')
                        log_file.flush()  # Ensure data written to disk

                        # Statistics data (for plotting)
                        if stats_log_file:
                            stats_log_file.write(f'{i+1},{current_timestamp},{upload_time:.4f},{speed:.4f},{args.file_size}\n')
                            stats_log_file.flush()  # Ensure data written to disk

    except KeyboardInterrupt:
        print('\nTest interrupted by user')

    finally:
        # Close log files
        if log_file:
            log_file.close()

            # Write summary statistics to CSV (for easy import to Excel and other tools)
            if csv_log_file and speeds:
                total_time = time.time() - total_start_time
                avg_speed = sum(speeds)/len(speeds)

                csv_log_file.write(f'Test Start Time,{datetime.fromtimestamp(total_start_time).strftime("%Y-%m-%d %H:%M:%S")}\n')
                csv_log_file.write(f'Test End Time,{datetime.fromtimestamp(time.time()).strftime("%Y-%m-%d %H:%M:%S")}\n')
                csv_log_file.write(f'Total Test Time(s),{total_time:.2f}\n')
                csv_log_file.write(f'Total Test Time(min),{total_time/60:.2f}\n')
                csv_log_file.write(f'File Size(MB),{args.file_size}\n')
                csv_log_file.write(f'Planned Upload Count,{num_uploads}\n')
                csv_log_file.write(f'Successful Upload Count,{len(speeds)}\n')
                csv_log_file.write(f'Average Upload Speed(MB/s),{avg_speed:.4f}\n')
                csv_log_file.write(f'Maximum Upload Speed(MB/s),{max(speeds):.4f}\n')
                csv_log_file.write(f'Minimum Upload Speed(MB/s),{min(speeds):.4f}\n')
                csv_log_file.write(f'Speed Median(MB/s),{statistics.median(speeds):.4f}\n')

                if len(speeds) > 1:
                    csv_log_file.write(f'Speed Standard Deviation(MB/s),{statistics.stdev(speeds):.4f}\n')

                # Write percentile data, useful for plotting and analysis
                csv_log_file.write(f'Speed 10th Percentile(MB/s),{sorted(speeds)[int(len(speeds)*0.1)]:.4f}\n')
                csv_log_file.write(f'Speed 25th Percentile(MB/s),{sorted(speeds)[int(len(speeds)*0.25)]:.4f}\n')
                csv_log_file.write(f'Speed 75th Percentile(MB/s),{sorted(speeds)[int(len(speeds)*0.75)]:.4f}\n')
                csv_log_file.write(f'Speed 90th Percentile(MB/s),{sorted(speeds)[int(len(speeds)*0.9)]:.4f}\n')
                csv_log_file.write(f'Total Upload Data(MB),{args.file_size * len(speeds)}\n')
                csv_log_file.close()

            if stats_log_file:
                stats_log_file.close()

            print(f'Upload records saved to: {args.log_file}')
            print(f'Statistical analysis data generated, can be used for plotting and further analysis')

        # Clean up test files
        if not args.keep_files:
            print('Cleaning up test files...')
            for test_file in test_files:
                if os.path.exists(test_file):
                    os.remove(test_file)
                    print(f'Deleted file: {test_file}')

        # Print statistical results
        total_time = time.time() - total_start_time

        print('\n=== Test Statistics ===')
        print(f'Successful uploads: {len(speeds)}/{num_uploads}')

        if speeds:
            print(f'Total test time: {total_time:.2f}s ({total_time/60:.2f}min)')
            print(f'Average upload speed: {sum(speeds)/len(speeds):.2f}MB/s')
            print(f'Maximum upload speed: {max(speeds):.2f}MB/s')
            print(f'Minimum upload speed: {min(speeds):.2f}MB/s')
            print(f'Speed median: {statistics.median(speeds):.2f}MB/s')
            if len(speeds) > 1:
                print(f'Speed standard deviation: {statistics.stdev(speeds):.2f}MB/s')
            print(f'Total uploaded data: {args.file_size * len(speeds)}MB')

        # If verification enabled and files kept, prompt user on how to verify
        if args.verify and args.keep_files:
            print('\n=== File Integrity Verification ===')
            print('To verify uploaded file integrity, use:')
            for i, file_info in enumerate(uploaded_files[:3]):  # Only show first 3 examples
                print(f'  python upload_speed_test.py --verify-file <downloaded_file_path> # MD5 should be: {file_info["md5"]}')
            if len(uploaded_files) > 3:
                print(f'  ... {len(uploaded_files) - 3} more file info available in log')

            if args.log_file:
                print(f'\nAll file MD5 information recorded in: {args.log_file}')
                print(f'Can use this log for batch verification')

if __name__ == '__main__':
    try:
        main()
    finally:
        # Restore standard output and close log file
        if hasattr(sys.stdout, 'close') and not isinstance(sys.stdout, io.TextIOWrapper):
            # If standard output was replaced by our Logger, close it
            sys.stdout.close()
            sys.stdout = sys.__stdout__  # Restore original standard output
