import re
import os
import sys
import glob
import gzip
import shutil
import traceback
from pathlib import Path
from urllib.parse import unquote
from libzim.reader import Archive

# We expect an input ZIM file and output directory on the command line
if len(sys.argv) != 3:
    print(f"Usage: {sys.argv [0]} <input ZIM file> <output directory>")
    exit()

# Make sure the input ZIM file exists
zimfile = sys.argv [1]
if not os.path.exists(zimfile):
    print(f"{zimfile} does not exist")
    exit()

# We can create the output directory
outdir = sys.argv [2]
if not outdir.endswith("/"):
    outdir += "/"
outpath = Path(outdir)
outpath.mkdir(parents=True, exist_ok=True)

# Initial output fragment name
fragfn = outdir + "ASSET-00.BIN"

# Processing starts now
try:
    # As we append to fragment files, make sure any
    # of those files are deleted
    search = outdir + "ASSET-*.BIN"
    delete = glob.glob(search)
    for file in delete:
       os.remove(file)

    # Open the CSV file used to import data in SQLite
    csvfile = open(outdir + "ASSET.CSV", "w")

    # Create the column headings to help with data import
    csvfile.write("origname,frag,gzip,cty,start,size\n")

    # Open the ZIM file
    zim = Archive(zimfile)

    # Output any main page
    if zim.has_main_entry:
        entry = zim.main_entry
        item = entry.get_item()
        mainfile = open(outdir + "MAIN.TXT", "w")
        mainfile.write(item.path + "\n")
        mainfile.close()

    # Iterate through all relevant entries
    frag = 0
    start = 0
    entries = zim.entry_count
    print(f"Will extract {entries} entries")
    for asset in range(entries):
        # Let the user know something is happening
        if (asset % 1000) == 0:
            pc = (asset / entries) * 100
            print (f"\rComplete: {asset}/{entries} ({pc:3.2f}%)", end="")

        # Get the entry
        entry = zim._get_entry_by_id(asset)

        # Skip redirected entries
        if entry.is_redirect:
            continue

        # Get the content
        item = entry.get_item()
        data = item.content

		# ZIM paths might be URL encoded, which we don't want
		adjpath = unquote(item.path)
		
        # We have to adjust the path to replace characters like ,
        # that can cause issues when importing the CSV file
        adjpath = adjpath.replace(",", "x").replace("\"", "y").replace("'", "z")

        # Get the content type. Some content types have a URL
        # which will not help in a closed system
        pattern = r'profile="https?://.*?"'
        cty = re.sub(pattern, '', item.mimetype)

        # The context type cannot be more than 32 characters
        if len(cty) > 32:
            print(f"Context-type {cty} is too long")
            exit()

        # See if we can compress this data
        zipped = 0
        gzlist = ["text/plain", "text/html", "image/svg+xml", "text/css", "application/javascript"]
        if any(item in cty for item in gzlist):
            zipped = 1
            with gzip.open(fragfn, "ab") as outstream:
                outstream.write(data)
        else:
            # Append the content to the current fragment
            with open(fragfn, "ab") as outstream:
                 outstream.write(data)

        # Get current fragment file size
        total = os.path.getsize(fragfn)

        # Calculate asset size
        size = total - start

        # Output data into the CSV file (no spaces
        # between items)
        csvfile.write(f"\"/{adjpath}\",{frag},{zipped},\"{cty}\",{start},{size}\n")

        # New starting point
        start = total

        # Roll to the next fragment if we are over a threshold. We have 
        # to limit the fragments to 4GB as the Arduino file.seek() API
        # only uses uint32_t
        if start > 4280000000:
           frag += 1

           # We limit ourselves to a certain number of fragments:
           #
           #   SD Card       After formatting    Max Content
           #   =======       ================    ===========
           #   64GB          ~60GB               14 fragments (~56GB)
           #   128GB         ~119GB              28 fragments (~112GB)
           #
           # This leaves enough space for the content and SQLite database
           #
           if frag >= 28:
               print("Content will not fit on the SD Card")
               exit()

           # Create a new filename
           fragfn = outdir + f"ASSET-{frag:02d}.BIN"

           # Reset the start position
           start = 0

    # Output some information
    print (f"\rComplete: {entries}/{entries} (100.00%)")

	# Create and write the SQL file
    sqlfile = open(outdir + "ASSET.SQL", "w")
    sqlfile.write("create table assets\n");
    sqlfile.write("(\n");
    sqlfile.write("  origname text primary key,\n");
    sqlfile.write("  frag     integer,\n");
    sqlfile.write("  gzip     integer,\n");
    sqlfile.write("  cty      text,\n");
    sqlfile.write("  start    integer,\n");
    sqlfile.write("  size     integer\n");
    sqlfile.write(");\n");
    sqlfile.write("PRAGMA journal_mode = OFF;\n");
    sqlfile.write("PRAGMA synchronous = 0;\n");
    sqlfile.write("PRAGMA cache_size = 1000000;\n");
    sqlfile.write("PRAGMA locking_mode = EXCLUSIVE;\n");
    sqlfile.write("PRAGMA temp_store = MEMORY;\n");
    sqlfile.write(".mode csv\n");
    sqlfile.write(".separator ','\n");
    sqlfile.write(".import ASSET.CSV assets\n");

    # Close files
    csvfile.close()
    sqlfile.close()

except Exception as e:
  print(f"Exception: {e}")
  traceback.print_exc()
  exit()
