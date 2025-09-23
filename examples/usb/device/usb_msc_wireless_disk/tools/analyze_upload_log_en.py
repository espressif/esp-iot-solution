#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Apache-2.0
# -*- coding: utf-8 -*-

'''
Analyze ESP HTTP upload logs and generate charts
Usage: python analyze_upload_log_en.py [log_file]

This script will parse the upload log file, extract upload time and speed data
directly from the log file (without recalculation), and generate relevant charts.

If no log file is specified, it will automatically use the most recent .log file
from the 'logs' directory in the same directory as this script.
'''

import sys
import os
import re
import matplotlib.pyplot as plt
import numpy as np
from datetime import datetime

def parse_log_file(log_path):
    '''Parse log file and extract upload information'''
    upload_data = []

    print(f'Parsing log file: {log_path}')
    print('Using actual upload times and speeds from log file (no recalculation)')

    # Process upload log
    with open(log_path, 'r', encoding='utf-8') as f:
        lines = f.readlines()

    i = 0
    while i < len(lines):
        line = lines[i].strip()
        if line.startswith('Upload ') and '/' in line:
            try:
                # Parse upload number
                upload_part = line.split('/')[0].replace('Upload ', '')
                upload_num = int(upload_part)

                # Get file name and size
                i += 1
                if i >= len(lines):
                    break
                file_info = lines[i].strip()
                if not file_info.startswith('Starting file upload:'):
                    continue

                file_name = file_info.split('(')[0].replace('Starting file upload: ', '').strip()
                size_part = file_info.split('size: ')[1].replace('MB)', '').strip()
                file_size_mb = float(size_part)

                # Get URL
                i += 1
                if i >= len(lines):
                    break
                url_line = lines[i].strip()
                if not url_line.startswith('Upload to URL:'):
                    continue

                url = url_line.replace('Upload to URL: ', '')
                url_match = re.search(r'test_(\d+)_(\d+)\.dat', url)
                upload_seq = None
                timestamp = None
                if url_match:
                    upload_seq = int(url_match.group(1))
                    timestamp = int(url_match.group(2))

                # Skip chunk size line if present
                i += 1
                if i < len(lines) and lines[i].strip().startswith('Using chunked upload'):
                    i += 1

                # Get upload complete info
                if i >= len(lines):
                    break
                upload_complete = lines[i].strip()
                # Match upload time and speed
                match = re.search(r'took (\d+\.\d+)s, speed: (\d+\.\d+)MB/s', upload_complete)
                if match:
                    actual_time = float(match.group(1))
                    actual_speed = float(match.group(2))

                    # Collect data using actual values from log
                    upload_data.append({
                        'num': upload_num,
                        'file': file_name,
                        'size_mb': file_size_mb,
                        'url': url,
                        'timestamp': timestamp,
                        'upload_seq': upload_seq,
                        'actual_time': actual_time,
                        'actual_speed': actual_speed
                    })
                    print(f'Parsed upload {upload_num}: {file_name}, {actual_time}s, {actual_speed}MB/s')
            except Exception as e:
                print(f'Parse error at line {i+1}: {e}')
        i += 1

    return upload_data

def generate_charts(data, log_file_name=None, output_dir=None, show_plot=True):
    '''Generate various data charts'''
    if not data:
        print('No data to plot')
        return False

    # Extract data series
    nums = [item['num'] for item in data]
    actual_times = [item['actual_time'] for item in data]
    actual_speeds = [item['actual_speed'] for item in data]

    # Calculate statistics
    avg_actual_speed = np.mean(actual_speeds)
    max_actual_speed = np.max(actual_speeds)
    min_actual_speed = np.min(actual_speeds)

    # Create charts
    plt.figure(figsize=(15, 12))

    # 1. Actual Upload Speed
    plt.subplot(2, 2, 1)
    plt.plot(nums, actual_speeds, '-o', markersize=2, color='blue')
    plt.title('Upload Speed Over Time')
    plt.xlabel('Upload Number')
    plt.ylabel('Speed (MB/s)')
    plt.grid(True)

    # 2. Speed Distribution Histogram
    plt.subplot(2, 2, 2)
    plt.hist(actual_speeds, bins=20, alpha=0.7, color='green')
    plt.title('Upload Speed Distribution')
    plt.xlabel('Speed (MB/s)')
    plt.ylabel('Frequency')
    plt.grid(True)

    # 3. Upload Time
    plt.subplot(2, 2, 3)
    plt.plot(nums, actual_times, '-o', markersize=2, color='orange')
    plt.title('Upload Time')
    plt.xlabel('Upload Number')
    plt.ylabel('Time (seconds)')
    plt.grid(True)

    # 4. Upload Speed Box Plot
    plt.subplot(2, 2, 4)
    plt.boxplot(actual_speeds)
    plt.title('Upload Speed Box Plot')
    plt.ylabel('Speed (MB/s)')
    plt.grid(True)

    # Add overall title and basic information
    plt.suptitle(f'ESP HTTP File Upload Speed Analysis\n'
                f'Average Speed: {avg_actual_speed:.2f} MB/s\n'
                f'Min Speed: {min_actual_speed:.2f} MB/s, Max Speed: {max_actual_speed:.2f} MB/s\n'
                f'Total Uploads: {len(data)}',
                fontsize=14)

    # Adjust spacing between subplots
    plt.tight_layout(rect=[0, 0, 1, 0.95])

    # Save chart
    timestamp = datetime.now().strftime('%Y%m%d_%H%M%S')
    if output_dir:
        os.makedirs(output_dir, exist_ok=True)
        output_file = os.path.join(output_dir, f'upload_analysis_{timestamp}.png')
    else:
        # Save in the same directory as the script
        script_dir = os.path.dirname(os.path.abspath(__file__))

        # Try to get log file name as base name
        try:
            # If log file name is provided
            if log_file_name:
                base_name = os.path.splitext(os.path.basename(log_file_name))[0]
            else:
                # Try to get from command line arguments
                log_file_path = sys.argv[1] if len(sys.argv) > 1 and not sys.argv[1].startswith('--') else None
                if log_file_path:
                    base_name = os.path.splitext(os.path.basename(log_file_path))[0]
                # Try to get base filename from data URL
                elif data and data[0].get('url'):
                    url = data[0]['url']
                    match = re.search(r'upload_test_\d+_\d+', url)
                    if match:
                        base_name = match.group(0)
                    else:
                        base_name = 'upload'
                else:
                    base_name = 'upload'
        except:
            base_name = 'upload'

        output_file = os.path.join(script_dir, f'{base_name}_analysis_{timestamp}.png')

    plt.savefig(output_file, dpi=300)
    print(f'Chart saved to: {output_file}')

    # Display chart if requested
    if show_plot:
        plt.show()

    return True

def save_csv_data(data, log_file_name=None, output_dir=None):
    '''Save parsed data to CSV file'''
    if not data:
        print('No data to save')
        return False

    timestamp = datetime.now().strftime('%Y%m%d_%H%M%S')
    if output_dir:
        os.makedirs(output_dir, exist_ok=True)
        output_file = os.path.join(output_dir, f'upload_data_{timestamp}.csv')
    else:
        # Save in the same directory as the script
        script_dir = os.path.dirname(os.path.abspath(__file__))

        # Try to get log file name as base name
        try:
            # If log file name is provided
            if log_file_name:
                base_name = os.path.splitext(os.path.basename(log_file_name))[0]
            else:
                # Try to get from command line arguments
                log_file_path = sys.argv[1] if len(sys.argv) > 1 and not sys.argv[1].startswith('--') else None
                if log_file_path:
                    base_name = os.path.splitext(os.path.basename(log_file_path))[0]
                # Try to get base filename from data URL
                elif data and data[0].get('url'):
                    url = data[0]['url']
                    match = re.search(r'upload_test_\d+_\d+', url)
                    if match:
                        base_name = match.group(0)
                    else:
                        base_name = 'upload'
                else:
                    base_name = 'upload'
        except:
            base_name = 'upload'

        output_file = os.path.join(script_dir, f'{base_name}_data_{timestamp}.csv')

    with open(output_file, 'w', encoding='utf-8') as f:
        # Write headers
        headers = ['Number', 'File', 'Size(MB)', 'URL', 'Timestamp', 'UploadSeq', 'ActualTime(s)', 'ActualSpeed(MB/s)']
        f.write(','.join(headers) + '\n')

        # Write data
        for item in data:
            row = [
                str(item['num']),
                item['file'],
                str(item['size_mb']),
                item['url'],
                str(item.get('timestamp', '')),
                str(item.get('upload_seq', '')),
                str(item['actual_time']),
                str(item['actual_speed'])
            ]
            f.write(','.join(row) + '\n')

    print(f'Data saved to: {output_file}')
    return True

def main():
    import argparse
    import glob

    parser = argparse.ArgumentParser(description='Analyze ESP HTTP upload log files and generate charts')
    parser.add_argument('log_file', nargs='?', help='Path to the log file to analyze. If not provided, the most recent .log file in the logs directory (same directory as script) will be used.')
    parser.add_argument('--output-dir', help='Directory to save output files')
    parser.add_argument('--no-show-plot', action='store_true', help='Do not display the plot window, just save the image file')
    args = parser.parse_args()

    log_file = args.log_file

    # If no log file specified, automatically find the latest .log file in the ./logs directory
    if log_file is None:
        # Get the script directory and look for logs subdirectory
        script_dir = os.path.dirname(os.path.abspath(__file__))
        logs_dir = os.path.join(script_dir, 'logs')

        if os.path.exists(logs_dir):
            log_files = glob.glob(os.path.join(logs_dir, '*.log'))
        else:
            # Fallback: look in current directory
            log_files = glob.glob('*.log')

        if not log_files:
            print(f'Error: No .log files found in {logs_dir if os.path.exists(logs_dir) else "current directory"}')
            return

        # Sort by file modification time, take the most recent log file
        log_files.sort(key=lambda x: os.path.getmtime(x), reverse=True)
        log_file = log_files[0]
        print(f'Automatically selected the most recent log file: {log_file}')

    if not os.path.exists(log_file):
        print(f'Error: File {log_file} not found')
        return

    print(f'Analyzing file: {log_file}')

    data = parse_log_file(log_file)

    if data:
        print(f'Successfully parsed {len(data)} upload records')

        # Generate charts and save data
        generate_charts(data, log_file, args.output_dir, not args.no_show_plot)
        save_csv_data(data, log_file, args.output_dir)
    else:
        print('No upload records found')

if __name__ == '__main__':
    main()
