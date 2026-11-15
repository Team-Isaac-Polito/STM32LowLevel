#!/usr/bin/python

import sys
from pathlib import Path
import subprocess

OFFSET = 0x08000000
FILE_PREFIX = "STM32LowLevel-"
FILE_SUFFIX = ".bin"
CHUNK_SIZE = 2 * 1024  # 2K per sector
SCRIPT_DIR = Path(__file__).parent
BUILD_DIR=Path(sys.argv[1]).parent


def diffAndPack(originalFile: str, newFile: str):
    dfuse_script = SCRIPT_DIR / "dfuse-pack.py"
    cmd = [sys.executable, str(dfuse_script)]
    # check of shame (i had to debug the flash .cmd and it was for this :facepalm:)
    if originalFile==newFile:
        print("[diff.py] Cannot diff with itself!")
        exit(1)
    try:
        with open(originalFile, "rb") as f1, open(newFile, "rb") as f2:
            num_sec = 0
            while True:
                originalChunk = f1.read(CHUNK_SIZE)
                newChunk = f2.read(CHUNK_SIZE)

                # exit if both files reach EOF
                if not originalChunk and not newChunk:
                    break

                # if length of the new chunk is zero, it means it hit EOF and the data present in the last firmware should be safe to be ignored.
                if originalChunk != newChunk and len(newChunk)!=0:
                    part_filename = Path(f"{BUILD_DIR}/{FILE_PREFIX}{num_sec}{FILE_SUFFIX}")
                    with open(part_filename, "wb") as outputFile:
                        outputFile.write(newChunk)

                    addr_hex = hex(OFFSET + (num_sec * CHUNK_SIZE))
                    cmd.extend(["-b", f"{addr_hex}:{part_filename}"])

                num_sec += 1
        
        output_file=BUILD_DIR / "output.dfu"
        cmd.append(output_file)
        pack(cmd)
    except OSError as e:
        print(f"[diff.py] {e.strerror} from file: {e.filename}, closing...")
        exit(1)


def pack(command_to_run: list[str]):
    try:
        subprocess.run(command_to_run, check=True) 
    except:
        exit(1)

def cleanup():
    cartella = Path(SCRIPT_DIR)
    for file_path in cartella.glob("STM32LowLevel-*.bin"):
        file_path.unlink()  # this deletes the part binaries and keeps only `output.dfu`


if __name__ == "__main__":
    if len(sys.argv) < 3:
        print(f"python diff.py <original> <new>: <new> chunks will compose the .dfu")
        exit(1)

    diffAndPack(sys.argv[1], sys.argv[2])
    cleanup()
