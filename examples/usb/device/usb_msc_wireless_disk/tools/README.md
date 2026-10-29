# ESP HTTP Server File Upload Speed Test Tool

This Python script is used to test the file upload speed between your computer and an ESP device's HTTP server. The script creates random test files of specified size (default 10MB), then continuously uploads multiple files until reaching the specified total data amount (default 1GB for 100 uploads), and records the upload speed for each file.

## Prerequisites

- Python 3.6 or higher
- `requests` library (install with `pip install requests`)
- `matplotlib` library for chart generation (install with `pip install matplotlib`)
- A running ESP HTTP server (ESP device with WiFi enabled)

## Usage

1. Ensure your computer is connected to the ESP device's WiFi network (refer to your device settings for default SSID)
2. Run the following command to test upload speed:

```bash
python upload_speed_test.py
```

### Command Line Arguments

The script supports the following command line arguments:

- `--url`: Target URL for file uploads (default: http://192.168.1.165)
- `--file-size`: Size of each test file in MB (default: 10)
- `--total-size`: Total upload data amount in MB (default: 1000, i.e., 1GB for 100 uploads)
- `--keep-files`: Keep generated test files after testing (not kept by default)
- `--test-files`: Number of test files to generate (default: 2)
- `--verify`: Generate verifiable files and add MD5 info to filenames (disabled by default)
- `--verify-file`: Verify integrity of specified file (providing this parameter will only perform verification)
- `--log-file`: Save upload records and statistics to specified log file (automatically generates multiple format log files)
- `--max-retries`: Maximum retry attempts on upload failure (default: 5)
- `--retry-delay`: Retry interval in seconds (default: 10)
- `--timeout`: Upload request timeout in seconds (default: 120)
- `--chunk-size`: Chunk size for chunked upload in KB (default: 48, optimized for 64KB TCP window)
- `--console-log`: Save console output to specified log file (auto-generated filename based on current time by default)
- `--no-show-plot`: Do not display plot window when analyzing logs

### Examples

Test upload to custom URL:
```bash
python upload_speed_test.py --url http://192.168.1.100
```

Change file size and total upload amount:
```bash
python upload_speed_test.py --file-size 5 --total-size 500
```

Keep test files after upload testing:
```bash
python upload_speed_test.py --keep-files
```

Use verifiable files for testing and record logs:
```bash
python upload_speed_test.py --verify --log-file upload_test.csv
```

Run 100 uploads (default):
```bash
python upload_speed_test.py
```

Use fixed number of test files with verification:
```bash
python upload_speed_test.py --test-files 2 --verify
```

Verify downloaded file integrity:
```bash
python upload_speed_test.py --verify-file /path/to/downloaded/file.dat
```

## Log File Organization

The script automatically creates logs in a `logs` directory within the same directory as the script. Log files are automatically named with timestamps like `upload_test_20250826_105011.log`.

## Output Results

The script displays upload progress and speed for each file, and finally generates test statistics including:

- Number of successful uploads
- Total test time
- Average upload speed (MB/s)
- Maximum upload speed (MB/s)
- Minimum upload speed (MB/s)
- Speed median (MB/s)
- Speed standard deviation (MB/s)
- Total uploaded data (MB)

## Data Analysis and Chart Generation

### Automatic Log Analysis

The package includes an analysis script `analyze_upload_log_en.py` that automatically processes upload logs and generates charts:

```bash
# Analyze the most recent log file automatically
python analyze_upload_log_en.py

# Analyze a specific log file
python analyze_upload_log_en.py logs/upload_test_20250826_105011.log

# Generate charts without displaying them
python analyze_upload_log_en.py --no-show-plot
```
