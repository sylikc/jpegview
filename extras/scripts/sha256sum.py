"""
quick and dirty to create sha256sum file of the files in a directory

https://www.quickprogrammingtips.com/python/how-to-calculate-sha256-hash-of-a-file-in-python.html
"""

# Python program to find SHA256 hash string of a file
import hashlib
from pathlib import Path
import sys

glob_pattern = "*" if len(sys.argv) == 1 else sys.argv[1]

for filename in Path.cwd().glob(glob_pattern):
    if not filename.is_file():
        continue

    sha256_hash = hashlib.sha256()
    with open(filename,"rb") as f:
        # Read and update hash string value in blocks of 4K
        for byte_block in iter(lambda: f.read(4096),b""):
            sha256_hash.update(byte_block)
        print(f"{sha256_hash.hexdigest()} *{filename.name}")
