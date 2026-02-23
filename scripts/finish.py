# Complete the PortableWiki process by copying assets to the SD Card
import os
import sys
import glob
import shutil
import traceback
from pathlib import Path

# We expect an input and output directory on the command line
if len(sys.argv) != 3:
    print(f"Usage: {sys.argv [0]} <input directory> <output directory>")
    exit()

# We also need SQLite to be install
if not shutil.which("sqlite3"):
    print("Please install SQLite")
    exit()

# Make sure both directories exist
indir = sys.argv [1]
if not indir.endswith("/"):
    indir += "/"

if not os.path.exists(indir):
    print(f"{indir} does not exist")
    exit()

outdir = sys.argv [2]
if not outdir.endswith("/"):
    outdir += "/"

if not os.path.exists(outdir):
    print(f"{outdir} does not exist")
    exit()

# Check that the input directory has been processed
# by zim2asset.py
csvfile = indir + "ASSET.CSV"
if not os.path.exists(csvfile):
    print(f"Please run zim2asset.py to create the correct content")
    exit

# Processing starts now
try:
    # Remove any old database file
    dbfile = indir + "ASSET.DB"
    if os.path.exists(dbfile):
        os.remove(dbfile)

    # Create the new database file
    print("Initializing database")
    cmd = f"cd {indir} && sqlite3 ASSET.DB < ASSET.SQL"
    result = os.system(cmd)
    if result != 0:
       print("Could not initialize database")
       exit()

    # Calculate the size of the input directory
    inpath = Path(indir)
    files = inpath.glob("*.DB")
    dbsize = sum(file.stat().st_size for file in files if file.is_file()) 

    files = inpath.glob("*.BIN")
    binsize = sum(file.stat().st_size for file in files if file.is_file()) 

    files = inpath.glob("*.TXT")
    txtsize = sum(file.stat().st_size for file in files if file.is_file()) 

    # Get the available space on the output
    totalbytes, usedbytes, freebytes = shutil.disk_usage(outdir)

    # Make sure the content will fit
    totalin = dbsize + binsize + txtsize
    if (totalin > freebytes):
        print(f"Not enough space on {outdir}. Needs {totalin - freebytes} bytes more space.")
        exit()

    # Copy the MAIN.TXT file
    mainfile = indir + "MAIN.TXT"
    if os.path.exists(mainfile):
        shutil.copy(mainfile, outdir)

    # Copy the database file
    print(f"Copying {dbfile}")
    shutil.copy(dbfile, outdir)

    # Copy content fragments which takes a long time, so
    # give the user some feedback
    search = indir + "ASSET-*.BIN"
    copy = glob.glob(search)
    copy.sort()
    count = 0
    total = len(copy)
    for file in copy:
       print(f"Copying file {count + 1}/{total} {file}")
       shutil.copy2(file, outdir)
       count += 1

except Exception as e:
  print(f"Exception: {e}")
  traceback.print_exc()
  exit()
