
import argparse

from rich.progress import track
from arduino import Arduino

# https://gbdev.io/pandocs/The_Cartridge_Header.html
publishers = {"00": "None", "01": "Nintendo", "08": "Capcom", "13": "EA"}
cart_types = {
 0x00: "ROM ONLY",
 0x01: "MBC1", 0x02: "MBC1+RAM", 0x03: "MBC1+RAM+BATTERY",
 0x05: "MBC2", 0x06: "MBC2+BATTERY",
 0x08: "ROM+RAM", 0x09: "ROM+RAM+BATTERY",
 0x0B: "MMM01", 0x0C: "MMM01+RAM", 0x0D: "MMM01+RAM+BATTERY",
 0x0F: "MBC3+TIMER+BATTERY", 0x10: "MBC3+TIMER+RAM+BATTERY", 0x11: "MBC3", 0x12: "MBC3+RAM", 0x13: "MBC3+RAM+BATTERY",
 0x19: "MBC5", 0x1A: "MBC5+RAM", 0x1B: "MBC5+RAM+BATTERY", 0x1C: "MBC5+RUMBLE", 0x1D: "MBC5+RUMBLE+RAM", 0x1E: "MBC5+RUMBLE+RAM+BATTERY",
 0x20: "MBC6",
 0x22: "MBC7+SENSOR+RUMBLE+RAM+BATTERY",
 0xFC: "POCKET CAMERA",
 0xFD: "BANDAI TAMA5",
 0xFE: "HuC3",
 0xFF: "HuC1+RAM+BATTERY",
}
# note: banks = 2**(rom_size + 1)
rom_sizes = {
 0x00: "32 KiB, 2 banks (no banking)",
 0x01: "64 KiB, 4 banks",
 0x02: "128 KiB, 8 banks",
 0x03: "256 KiB, 16 banks",
 0x04: "512 KiB, 32 banks",
 0x05: "1 MiB, 64 banks",
 0x06: "2 MiB, 128 banks",
 0x07: "4 MiB, 256 banks",
 0x08: "8 MiB, 512 banks",
}
ram_sizes = {
 0x00: "0, No RAM",
 0x01: "-, Unused",
 0x02: "8 KiB, 1 bank",
 0x03: "32 KiB, 4 banks",
 0x04: "128 KiB, 16 banks",
 0x05: "64 KiB, 8 banks",
}
destinations = {
  0x00: "Japan",
  0x01: "Overseas only",
}

def hex_str(b):
    return (len(b) * " %02X") % tuple(b)

def header_checksum(header):
    checksum = 0;
    for i in range(52, 77):
        checksum = (checksum - header[i] - 1) & 0xFF;
    return checksum

# command line arguments
parser = argparse.ArgumentParser('transfer_GB.py', 'Program to read Game Boy ROMs over USB.')
parser.add_argument('--port', default='/dev/ttyACM0', help='Port address. Typically /dev/ttyACM0 on linux, COM# on Windows, /dev/tty.usbmodem# on Mac OS.')
parser.add_argument('--header', action='store_true', help='Only read ROM header information.')
args = parser.parse_args()

# open connection to Arduino MEGA 2560
a = Arduino(port=args.port, baud=500000, timeout=5)

# read header information
a.write("#HEADER$")
header = a.read(80, check_word=True, check_method='single')

print("\nEntry Point:", hex_str(header[:4])[1:])

print("\nNintentdo Logo:")
print(hex_str(header[ 4:20]))
print(hex_str(header[20:36]))
print(hex_str(header[36:52]))

print("\nTitle:", header[52:68])

licensee = header[68:70].decode('ascii')
print(f"\nLicense Code: {licensee} {publishers[licensee]}")

print(f"\nSGB Flag:         {header[70]:02X}")
print(f"Cartridge Type:   {header[71]:02X} ({cart_types[header[71]]})")
print(f"ROM size:         {header[72]:02X} ({rom_sizes[header[72]]})")
print(f"RAM size:         {header[73]:02X} ({ram_sizes[header[73]]})")
print(f"Destination:      {header[74]:02X} ({destinations[header[74]]})")
print(f"Old License Code: {header[75]:02X}")
print(f"Mask ROM Version: {header[76]:02X}")
print(f"Header Checksum:  {header[77]:02X} (should be {header_checksum(header):02X})")
print(f"Global Checksum:  {hex_str(header[78:80])[1:]}")

# get sector size used for dumping ROM
a.write("#SCSIZE$")
sector_size = int.from_bytes(a.read(4), "big")
print(f"\nSector Size: {sector_size}")

fout = open("test.gb", 'wb')
banks = 2**(header[72] + 1)
for bank in track(range(banks), "  - downloading"):

    if bank == 0:
        start = 0x0000
        end = 0x3FFF
    else:
        start = 0x4000
        end = 0x7FFF

    # activate the correct memory bank
    # Note: not needed for bank 0, which is always active
    if bank > 0:
        bank_command = b'#BK' + bank.to_bytes(4, 'big') + b'$'
        a.write(bank_command, binary=True)

    # loop through sectors of data bytes
    for addr in range(start, end, sector_size):
        sector_command = b'#SC' + addr.to_bytes(4, 'big') + b'$'
        a.write(sector_command, binary=True)
        for retry in range(3):
            try:
                data = a.read(sector_size, check_word=True, check_method='single')
                # break if read executes cleanly
                if len(data) == sector_size:
                    fout.write(data)
                    break
            except:
                pass
 
            # try bad read again with double validation
            print(f"  - retrying sector at {sector}")

            if retry == 2:
                raise ValueError(f"Failed to read bank {bank}, sector {sector}.")

fout.close()
