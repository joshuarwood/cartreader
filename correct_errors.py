
import os
import argparse

from crc32 import crc32

parser = argparse.ArgumentParser(__file__, description="Attempts error correction using 3 files.")
parser.add_argument("input", nargs=3, help="List of input files.")
parser.add_argument("--output", default='corrected.z64', help="Corrected file.")

args = parser.parse_args()

print("\nOpening Files ...")
data = []
size = os.path.getsize(args.input[0])
for path in args.input:

    print(f" {path}")
    f = open(path, 'rb')
    data.append(f.read(size))
    f.close()

print("\nAttempting ROM corrections ...")
fout = open(args.output, 'wb')
sector_size = 512
for i in range(0, size, sector_size):
    data0 = data[0][i:i + sector_size]
    data1 = data[1][i:i + sector_size]
    data2 = data[2][i:i + sector_size]

    good = (data0 == data1) and (data0 == data2)

    if not good:
        # attempt correction based on majority vote
        if data0 == data1 or data0 == data2:
            # data0 matches one other data and is likely good, use it
            pass
        elif data1 == data2:
            # data0 disagrees with 1 & 2, correct it
            data0 = data1
        else:
            fout.close()
            os.remove(args.output)
            raise ValueError(f"Could not correct sector at {i}.")
        print(f" Corrected sector at {i}")

    fout.write(data0)

fout.close()

print(f"\nCalculating CRC32 ...")
f = open(args.output, "rb")
data = f.read(os.path.getsize(args.output))
f.close()

crc = hex(crc32(data)).upper()[2:]
print(f" {crc} {args.output}\n")

