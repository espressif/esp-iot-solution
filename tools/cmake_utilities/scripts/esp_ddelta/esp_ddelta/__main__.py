# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Apache-2.0
import argparse
import sys

from esp_ddelta import decode_file, encode_file


def main(argv=None):
    parser = argparse.ArgumentParser(
        prog='esp_ddelta',
        description='Encode or decode an uncompressed ddelta patch (DDELTA40).',
    )
    sub = parser.add_subparsers(dest='command', required=True)

    encode = sub.add_parser('encode', help='old + new -> uncompressed patch')
    encode.add_argument('old', help='base firmware')
    encode.add_argument('new', help='target firmware')
    encode.add_argument('patch', help='output patch path')

    decode = sub.add_parser('decode', help='old + patch -> new firmware')
    decode.add_argument('old', help='base firmware')
    decode.add_argument('patch', help='uncompressed ddelta patch')
    decode.add_argument('new', help='output firmware path')

    args = parser.parse_args(argv)
    if args.command == 'encode':
        encode_file(args.old, args.new, args.patch)
    else:
        decode_file(args.old, args.patch, args.new)
    return 0


if __name__ == '__main__':
    try:
        sys.exit(main())
    except Exception as exc:
        print('error: {}'.format(exc), file=sys.stderr)
        sys.exit(1)
