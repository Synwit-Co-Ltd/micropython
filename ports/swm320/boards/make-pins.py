"""Generates the pins file for the SWM320"""
from __future__ import print_function

import re
import sys
import argparse


class Pin:
    """Holds the information associated with a pin."""
    def __init__(self, name, port, pbit, preg, IRQn):
        self.name = name
        self.port = port
        self.pbit = pbit
        self.preg = preg
        self.IRQn = IRQn

    def print(self):
        print('pin_obj_t pin_{:4s} = PIN({:4s}, {:5s}, {:5s}, {:s}, {:10s});\n'.format(self.name, self.name, self.port, self.pbit, self.preg, self.IRQn))

    def print_header(self, hdr_file):
        hdr_file.write('extern pin_obj_t pin_{:4s};\n'.format(self.name))


class Pins:
    def __init__(self):
        self.pins = []   # list of Pin

    def find_pin(self, port, pbit):
        for pin in self.pins:
            if pin.port == port and pin.pbit == pbit:
                return pin

    def find_pin_by_name(self, name):
        for pin in self.pins:
            if pin.name == name:
                return pin

    def parse_af_file(self, filename):
        with open(filename, 'r') as f:
            for line in f.readlines():
                match = re.match(r'#define PORT([ABCD])_PIN(\d+)_GPIO', line)
                if match:
                    name = 'P' + match.group(1) + match.group(2)
                    for pin in self.pins:
                        if pin.name == name:
                            break
                    else:
                        preg= 'PIN_BIT_BAND(%s, %2s)' %('GPIO'+match.group(1), match.group(2))
                        pin = Pin(name, 'GPIO'+match.group(1), 'PIN'+match.group(2), preg, 'GPIO%s_IRQn' %match.group(1))
                        self.pins.append(pin)

    def print(self):
        for pin in self.pins:
            pin.print()
        print('')
        print('static const mp_rom_map_elem_t pins_locals_dict_table[] = {')
        for pin in self.pins:
            print('    {{ MP_ROM_QSTR(MP_QSTR_{:5s}),  MP_ROM_PTR(&pin_{:5s}) }},'.format(pin.name, pin.name))
        print('};')
        print('')
        print('MP_DEFINE_CONST_DICT(pins_locals_dict, pins_locals_dict_table);')
        

    def print_header(self, hdr_filename):
        with open(hdr_filename, 'wt') as hdr_file:
            for pin in self.pins:
                pin.print_header(hdr_file)

    def print_qstr(self, qstr_filename):
        with open(qstr_filename, 'wt') as qstr_file:
            for pin in self.pins:
                print('Q({})'.format(pin.name), file=qstr_file)


def main():
    parser = argparse.ArgumentParser(
        prog="make-pins.py",
        usage="%(prog)s [options] [command]",
        description="Generate board specific pin file"
    )
    parser.add_argument(
        "-a", "--af",
        dest="af_filename",
        help="Specifies the alternate function file for the chip",
        default="../chip/SWM3200_port.h"
    )
    parser.add_argument(
        "-p", "--prefix",
        dest="prefix_filename",
        help="Specifies beginning portion of generated pins file",
        default="SWM320_prefix.c"
    )
    parser.add_argument(
        "-q", "--qstr",
        dest="qstr_filename",
        help="Specifies name of generated qstr header file",
        default="../build-SWM320Lite/pins_qstr.h"
    )
    parser.add_argument(
        "-r", "--hdr",
        dest="hdr_filename",
        help="Specifies name of generated pin header file",
        default="../build-SWM320Lite/pins.h"
    )
    args = parser.parse_args(sys.argv[1:])

    pins = Pins()

    print('// This file was automatically generated by make-pins.py')
    print('//')

    if args.af_filename:
        print('// --af {:s}'.format(args.af_filename))
        pins.parse_af_file(args.af_filename)

    if args.prefix_filename:
        print('// --prefix {:s}'.format(args.prefix_filename))
        print('')
        with open(args.prefix_filename, 'r') as prefix_file:
            print(prefix_file.read())

    pins.print()
    pins.print_qstr(args.qstr_filename)
    pins.print_header(args.hdr_filename)


if __name__ == "__main__":
    main()
