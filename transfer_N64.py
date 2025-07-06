
import ssl
import time
import math
import certifi
import argparse

from crc32 import crc32
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
parser = argparse.ArgumentParser('transfer_N64.py', 'Program to read N64 ROMs over USB.')
parser.add_argument('--port', default='/dev/ttyACM0', help='Port address. Typically /dev/ttyACM0 on linux, COM# on Windows, /dev/tty.usbmodem# on Mac OS.')
parser.add_argument('--header', action='store_true', help='Only read ROM header information.')
args = parser.parse_args()

# open connection to Arduino MEGA 2560
a = Arduino(port=args.port, baud=500000)

# get sector size used for dumping ROM
a.write("#SCSIZE$")
sector_size = int.from_bytes(a.read(4), "big")

# get ROM header
print("\nReading ROM header")
a.write("#HEADER$")
header = a.read(64, check_word=True)
print(f"  - received {len(header)} bytes")
print(f"  - payload  {bytes(header)}")

# parse header info
rom_ver, rom_name, rom_checksum = header_info(header)
print("\nROM Info")
print(f"  - name:     {rom_name}")
print(f"  - version:  {rom_ver}")
print(f"  - checksum: {rom_checksum}")

# use header info to lookup database entry
print("\nDatabase Info")
db = cartridge_db()
matches = []
for file, item in db.items():
    if rom_checksum == item['checksum']:
        matches.append(file)

if len(matches) == 0:
    raise ValueError("Could not locate ROM in database.")

if len(matches) > 1:
    print("Found multiple matches based on checksum.")
    print("Please select the correct match from the list below:")
    for i, file in enumerate(matches):
        print(f"{i}. {file}")
    i = int(input("\nEnter number: "))
    matches = [matches[i]]

file = matches[0]
size = db[file]['size']
checksum = db[file]['checksum']
crc = db[file]['crc']

print(f"  - file:     {file}")
print(f"  - size:     {size} MB")
print(f"  - checksum: {checksum}")
print(f"  - crc32:    {crc} ")

if args.header:
    # exit here when only reading header
    print("")
    a.sp.close()
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
        try:
            data = a.read(sector_size, check_word=True)
            # break if read executes cleanly
            if len(data) == sector_size:
                fout.write(data)
                break
        except:
            pass
 
        # try bad read again with double validation
        print(f"  - retrying sector at {sector}")

        if retry == 2:
            raise ValueError(f"Failed to read sector {sector}.")
        
    #break
fout.close()
print("")

print(f"Wrote '{file}'  in %.2f sec" % (time.time() - t0))

print("\nCRC32 status is ", end='')
fin = open(file, 'rb')
data = fin.read(n)
fin.close()

file_crc = ("%08x" % crc32(data)).upper()

status, sign = ("GOOD", "=") if file_crc == crc else ("BAD", "!")
print(status)
print(f"  {file_crc} (File) {sign}= {crc} (Database)\n")

# close serial port
a.sp.close()
