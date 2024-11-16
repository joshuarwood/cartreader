
import ssl
import time
import math
import certifi
import argparse

from arduino import Arduino
from rich.progress import track
from urllib.request import urlopen

MB = 1024 * 1024 # 1 megabyte

def header_info(header, verbose=False):
    """Parse binary header info"""
    if verbose:
        [print(i, hex(i), hex(b)) for i, b in enumerate(header)]

    ver = header[0x3F]
    name = header[0x20:0x20 + 20].decode('ascii').strip()
    checksum = (''.join(f'{b:02x}' for b in header[0x10:0x10 + 4])).upper()

    return ver, name, checksum

def cartridge_db():
    """ Read the database of N64 cartridges and store the corresponding
    file names, 32 bit CRC, header checksum, cartridge size in MB, and save file type.

    Returns:
        (dict)
    """
    db = {}

    url = "https://raw.githubusercontent.com/sanni/cartreader/master/sd/n64.txt"
    context = ssl.create_default_context(cafile=certifi.where())

    n = 0
    lines = [line.decode() for line in urlopen(url, context=context)]
    for i in range(int(len(lines) / 3)):
        file = lines[3 * i].strip()
        crc, checksum, size, save = lines[3 * i + 1].strip().split(",")
        db[file] = {'crc': crc, 'checksum': checksum, 'size': int(size), 'save': save}
        n += 1

    return db


# command line arguments
parser = argparse.ArgumentParser('reader_N64.py', 'Program to read N64 ROMs over USB.')
parser.add_argument('--header', action='store_true', help='Only read ROM header information.')
args = parser.parse_args()

# open connection to Arduino MEGA 2560
a = Arduino(baud=500000)

# get sector size used for dumping ROM
a.write("#SCSIZE$")
sector_size = int.from_bytes(a.read(4), "big")

# get ROM header
print("\nReading ROM header")
a.write("#HEADER$")
header = a.read(64)
print(f"  - received {len(header)} bytes")
print(f"  - payload  {bytes(header)}")

# parse header info
rom_ver, rom_name, rom_checksum = header_info(header)
print("\nROM Info")
print(f"  - name     '{rom_name}'")
print(f"  - version  '{rom_ver}'")
print(f"  - checksum '{rom_checksum}'")

# use header info to lookup database entry
print("\nDatabase Info")
db = cartridge_db()
file = None
size = None
for file, item in db.items():
    key = file.upper()
    if ", THE" in key:
        key = "THE " + key.replace(", THE", "")
    if rom_name in key and rom_checksum == item['checksum']:
        crc = item['crc']
        size = item['size']
        print(f"  - file:  {file}")
        print(f"  - size:  {size} MB")
        print(f"  - crc32: {crc} ")
        break

#file = 'test.txt'
#size = 8

if size is None:
    raise ValueError("Could not locate ROM in database.")

if args.header:
    # exit here when only reading header
    print("")
    exit(0)

# dump ROM to binary file
t0 = time.time()
n = size * MB
print(f"\nReading {n} byte ROM in {sector_size} byte chunks")
fout = open(file, 'wb')
for sector in track(range(0, n, sector_size), "  - downloading"):
    sector_command = b'#SC' + sector.to_bytes(4, 'big') + b'$'
    for retry in range(3):
        a.write(sector_command, binary=True)
        data = a.read(sector_size)
        #print(data)
        if len(data) == sector_size:
            fout.write(data)
            break
 
        print(f"  - retrying sector {sector}")
        if retry == 2:
            raise ValueError(f"Failed to read sector {sector} with {len(data)}/{sector_size} bytes.")
        
    #break
fout.close()
print("")

print(f"Wrote '{file}'  in %.2f sec" % (time.time() - t0))

