from __future__ import division

import argparse
import array
import binascii
import codecs
import logging
import math
import os
import re
import struct
import sys

from collections import namedtuple

logging.getLogger().setLevel(logging.INFO)

def parse_args(args):
    """
    Parse the args 
    """
    parser = argparse.ArgumentParser(description='Assemble file system parts into a single image')
    parser.add_argument('-o', '--output_file',
                        help='Path to the output file')
    parser.add_argument('fsparts', nargs='*', help='Sequence of file system parts in the order to be assembled')
    return parser.parse_args(args)

def create_xuv_file(fpath, output_filename, buffer=bytes()):
    # Convert file system buffer to xuv format and write as xuv file
    path = os.path.dirname(fpath)
    output_file = os.path.join(path, output_filename + '.xuv')
    print("Creating %s" % output_file)
    xuv_buffer=''
    b_vafs_buffer = bytearray(buffer)
    if((len(b_vafs_buffer) % 2) == 1):
        b_vafs_buffer.append(0)
        print("Odd number of bytes in binary file appended to {0}".format(len(b_vafs_buffer)))
    else:
        print("Number of bytes in binary file: {0}".format(len(b_vafs_buffer)))
    for i in range(0,len(b_vafs_buffer), 2):
        high_byte = b_vafs_buffer[i+1]
        low_byte = b_vafs_buffer[i]
        xuv_buffer += '@%06x    %02x%02x\n' % (i//2, high_byte, low_byte)
    with open(output_file, 'w') as f:
        f.write(xuv_buffer)

def run_script(arglist):
    pargs = parse_args(arglist)

    for i in range(0,len(pargs.fsparts)):
        print("Input files: " + pargs.fsparts[i])
    print("Output filename: " + pargs.output_file)
    if len(pargs.fsparts) > FileSystem.MAX_FILE_ENTRIES:
        sys.exit('ERROR: Only 1 file allowed in ra_fs')

    for fpath in pargs.fsparts:
        fn = os.path.basename(fpath)
        if len(fn) > FileSystem.MAX_FILE_NAME_LENGTH:
            sys.exit('ERROR: Filename exceeds max length {}:{}').format(FileSystem.MAX_FILE_NAME_LENGTH, fn)
        if not os.path.exists(fpath):
            sys.exit('ERROR: File does not exist: {}'.format(fpath))
        fsize = os.path.getsize(fpath)

        # Read in file to fbuffer
        with open(fpath, 'rb') as fh:
            fbuffer = fh.read()
        entry = FileSystem.FILE_ATTRIBS(filename=fn, length=fsize, buffer=fbuffer)
        create_xuv_file(fpath, pargs.output_file, entry.buffer)

class FileSystem(object):
  MAX_FILE_ENTRIES = 1
  MAX_FILE_NAME_LENGTH = 0x10 - 1 # leave space for \0 terminator
  FILE_ATTRIBS = namedtuple('FileAttribs', [
        'filename',
        'length',
        'buffer'
    ])

  def __init__(self):
    self.entries = []

