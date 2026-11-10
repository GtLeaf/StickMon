#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Fault injection for the exact transaction used by SDGOODS USB updates."""
from pathlib import Path
import subprocess
ROOT=Path(__file__).resolve().parents[1]
def main():
    out=ROOT/'.stick-local/artifacts/sdgoods-resource-storage-20261105/update-tests';out.mkdir(parents=True,exist_ok=True)
    binary=out/'update-host'
    subprocess.run(['c++','-std=c++17','-O1','-g','-fsanitize=address,undefined','-Isrc','tools/resource_update_host.cpp','-o',str(binary)],cwd=ROOT,check=True)
    subprocess.run([str(binary)],check=True)
if __name__=='__main__':main()
