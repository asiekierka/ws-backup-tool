#!/usr/bin/python3
#
# Copyright (c) 2020 Adrian Siekierka
#
# Permission to use, copy, modify, and/or distribute this software for any
# purpose with or without fee is hereby granted.
#
# THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
# WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
# MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
# SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER
# RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF
# CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
# CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.

from collections import OrderedDict
from pathlib import Path
import argparse
import re
import sys

def main(args):
    if args.field_name is not None and len(args.input) > 1:
        raise Exception("Cannot use --field_name with more than one input file!")
    files = OrderedDict()
    for fn in args.input:
        file_key = args.field_name or ("_%s" % re.sub(r"[^a-zA-Z0-9]", "_", Path(fn).name))
        with open(fn, "rb") as f:
            files[file_key] = bytearray(f.read())
    # generate header file
    with open(args.outh, "w") as f:
        f.write("// autogenerated by bin2c.py\n")
        f.write("#include <stdint.h>\n")
        for field_name, data in files.items():
            f.write("\n")
            f.write("#define %s_size %d\n" % (field_name, len(data)))
            f.write("extern const uint8_t __far %s[%s_size];\n" % (field_name, field_name))
    # generate C file
    line_step = 16
    with open(args.outc, "w") as f:
        f.write("// autogenerated by bin2c.py\n")
        if args.bank is not None:
            f.write("#pragma bank %s\n" % args.bank)
        f.write("#include <stdint.h>\n")
        for field_name, data in files.items():
            f.write("\n")
            f.write("const uint8_t __far %s[] = {\n" % field_name)
            for i in range(0, len(data), line_step):
                data_part = data[i:(i + line_step)]
                data_part_str = ", ".join([str(x) for x in data_part])
                if (i + line_step) < len(data):
                    f.write("\t%s, // %d\n" % (data_part_str, i))
                else:
                    f.write("\t%s // %d\n" % (data_part_str, i))
            f.write("};\n")

if __name__ == '__main__':
    args_parser = argparse.ArgumentParser(
        description="Convert binary files to a .C/.H pair"
    )
    args_parser.add_argument("--field_name", required=False, type=str, help="Target field name (for one input file).")
    args_parser.add_argument("--bank", required=False, type=str, help="Bank (for GBDK)")
    args_parser.add_argument("outc", help="Output C file.")
    args_parser.add_argument("outh", help="Output header file.")
    args_parser.add_argument("input", nargs="+", help="Input binary file.")
    args = args_parser.parse_args()
    main(args)