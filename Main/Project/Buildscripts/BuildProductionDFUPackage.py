## Build script for Melbits Pod firmware images
## 2019 - Miguel Angel Exposito
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
    if len(sys.argv) < 6:
        com.die("Usage: " + os.path.basename(__file__) + " <.hex firmware file> <output dir> <outputFilenamePrefix> <app_version> <product_id>")
        exit(1)

# Pre checks
com.preChecks()

validateCommandLine()
#com.validateProductId(sys.argv[5])

inSourceFile = com.normalizePath(sys.argv[1])
inOutputDir = com.normalizePath(sys.argv[2])
inOutputFilenamePrefix = sys.argv[3]
inAppVersion = sys.argv[4]
#inProductCode = int(sys.argv[5], 16) # In Hex
inProductCode = sys.argv[5] # In Hex
inFeatureLevel = sys.argv[6]

gScriptDir = os.path.dirname(os.path.abspath(__file__))

# Constants
#gOutputFilePath = com.normalizePath(os.path.join(inOutputDir, inOutputFilenamePrefix + "_h" + format(inProductCode, 'x') + '_v' + inAppVersion + ".zip"))
gOutputFilePath = com.normalizePath(os.path.join(inOutputDir, inOutputFilenamePrefix + "_h" + inProductCode + '_v' + inAppVersion + ".zip"))

# Derived paths
gKeyFilePath = com.normalizePath(gScriptDir + "/../../../DFU/keys/private.key")

print ("inSourceFile = " + inSourceFile)
print ("inOutputDir = " + inOutputDir)
print ("")
print ("Build script running from " + gScriptDir)


#if os.path.isfile(gOutputFilePath):    
#    com.die("The output file " + gOutputFilePath + " already exists. Please clean it first.")

com.deleteFileIfExists(gOutputFilePath)

com.createDirIfNotExist(inOutputDir)

os.chdir(gScriptDir)

print("BUILD DFU PACKAGE")

com.runPythonOrDie(com.normalizePath(os.path.join(com.nRFUtilToolPath, '..')),
                [com.nRFUtilToolPath,
                'pkg', 'generate',
                '--hw-version', str(inProductCode),
                '--application-version', str(inAppVersion),
                '--application', com.quotePath(inSourceFile),
                '--sd-req', '0xB8',
                '--key-file', com.quotePath(gKeyFilePath),
                com.quotePath(gOutputFilePath)])


com.fileExistsOrDie(gOutputFilePath)

with tempfile.TemporaryDirectory() as tempDir:
    # Extract all files to temp dir for postprocessing
    with zipfile.ZipFile(gOutputFilePath, "r", zipfile.ZIP_STORED, False) as xf:
        xf.extractall(tempDir)

    manifestFileName = "manifest.json"
    pathToTempManifest = os.path.join(tempDir, manifestFileName)

    with open(pathToTempManifest) as jsf:
        data = json.load(jsf)

    appObj = data['manifest']['application']
    appObj['ver'] = str(inAppVersion)       # + App version
    appObj['fl'] = str(inFeatureLevel)      # + Feature Level
    appObj['pc'] = "MLB202101"              # + Product code
    appObj['pcn'] = str(inProductCode)      # + Product code numeric
    
    b = bytearray()

    # Read main fw file and store bytes
    with open(os.path.join(tempDir, appObj['bin_file']), "rb") as fp:
        b = fp.read()
    
    # Read data file and store bytes
    with open(os.path.join(tempDir, appObj['dat_file']), "rb") as fp:
        b = b + fp.read()        

    # Add the bytes of every string inside of manifest/application
    for key in sorted(appObj.keys()):
        v = appObj[key]
        if isinstance(v, str):
            b = b + bytearray(v, 'utf-8')

    # Compute hash
    h = hashlib.new("sha256", b)
    data['h'] = h.hexdigest()             # + Hash value
    
    with open(pathToTempManifest, 'w') as jsf:
        json.dump(data, jsf, indent=4)

    os.remove(gOutputFilePath)

    # Rebuild archive
    with zipfile.ZipFile(gOutputFilePath, "w", zipfile.ZIP_STORED, False) as xf:
        onlyfiles = [f for f in os.listdir(tempDir) if os.path.isfile(os.path.join(tempDir, f))]
        for f in onlyfiles:
            xf.write(os.path.join(tempDir, f), f, zipfile.ZIP_STORED)
