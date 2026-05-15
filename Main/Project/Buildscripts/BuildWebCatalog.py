## Build script for Melbits Pod firmware images
## 2020 - Miguel Angel Exposito
## SPDX-FileCopyrightText: 2017-2020 Melbot Studios, S.L.
## SPDX-License-Identifier: LicenseRef-Melbot-Portfolio
##
## Original author: Miguel Angel Exposito (2017-2020)
##
## This source code is the property of Melbot Studios, S.L. and is published
## publicly for portfolio and educational review purposes with the express
## written authorization of Melbot Studios, S.L.
##
## All rights remain with Melbot Studios, S.L. Redistribution, modification,
## commercial use, or derivative works are not permitted without prior
## written consent of the copyright holder. See LICENSE for full terms.
import sys
import os

import zipfile
import json
import shutil
import tempfile
import hashlib

from collections import OrderedDict 

import BuildCommon as com
import AppVersions as ver

def validateCommandLine():
    if len(sys.argv) < 3:
        com.die("Usage: " + os.path.basename(__file__) + " <.zip DFU file> <output dir> <flags> [devcnt]")
        exit(1)
		
#com.preChecks()

validateCommandLine()

inSourceFile 	= com.normalizePath(sys.argv[1])
inOutputFile 	= com.normalizePath(sys.argv[2])
inFlags 		= sys.argv[3]
inDevCnt        = '0' if len(sys.argv) < 5 else sys.argv[4]

# Read version and feature level from the update package
# Build json with:
#	- fw version
#	- file_name
#	- flags (update priority)
#	- hash

with zipfile.ZipFile(inSourceFile, "r", zipfile.ZIP_STORED, False) as xf:
    with xf.open("manifest.json") as jf:
        inJsonRootObj = json.load(jf)

inJsonAppObj = inJsonRootObj['manifest']['application']

outJsonCatalogObj = {'ver' : inJsonAppObj['ver'],
                     'fl' : inJsonAppObj['fl'],
                     'pc' : inJsonAppObj['pc'],
                     'flg' : inFlags,
                     'file' : os.path.basename(inSourceFile),
                     'dc' : inDevCnt
                    }

b = bytearray()
# Compute hash
for key in sorted(outJsonCatalogObj.keys()):
    v = outJsonCatalogObj[key]
    if isinstance(v, str):
        b = b + bytearray(v, 'utf-8')

h = hashlib.new("sha256", b)
outJsonHash = h.hexdigest()             # + Hash value

outJsonRootObj = {'catalog' : outJsonCatalogObj,
                  'h' : outJsonHash}

with open(inOutputFile, 'w') as jsf:
    json.dump(outJsonRootObj, jsf, indent=4)
