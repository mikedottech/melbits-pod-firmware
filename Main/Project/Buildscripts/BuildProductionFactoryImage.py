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

import BuildCommon as com
import AppVersions as ver

import shutil

def validateCommandLine():
    if len(sys.argv) < 7:
        com.die("Usage: " + os.path.basename(__file__) + " <.hex firmware file> <temp dir> <output dir> <outputFilenamePrefix> <app version> <product_id>")
        exit(1)

# Pre checks
com.preChecks()

validateCommandLine()
#com.validateProductId(sys.argv[6])

inSourceFile = com.normalizePath(sys.argv[1])
inTempDir = com.normalizePath(sys.argv[2])
inOutputDir = com.normalizePath(sys.argv[3])
inOutputFilenamePrefix = sys.argv[4]
inAppVersion = sys.argv[5]
#inProductCode = int(sys.argv[6], 16) # In Hex
inProductCode = sys.argv[6] # In Hex

# Constants

#gOutputFilePath = com.normalizePath(os.path.join(inOutputDir, inOutputFilenamePrefix  + "_h" + format(inProductCode, 'x') + '_v' + inAppVersion + ".hex"))
gOutputFilePath = com.normalizePath(os.path.join(inOutputDir, inOutputFilenamePrefix  + "_h" + inProductCode + '_v' + inAppVersion + ".hex"))

gScriptDir = os.path.dirname(os.path.abspath(__file__))

##### Derived paths
gSoftDeviceHexPath = os.path.join('..', '..', '..', 'NRF5SDK', 'components', 'softdevice', 's112', 'hex', 's112_nrf52_6.1.1_softdevice.hex')
gBootloaderHexPath = com.normalizePath(gScriptDir + "/../../../Secure Bootloader/pca10040e_ble/ses/Output/Release_Pod_h" + sys.argv[6] + "/Exe/secure_bootloader_ble_s112_pca10040e.hex")
gFinalImagePath = os.path.join(inTempDir, "final.hex")

# Temp files
gBootloaderWithSettingsPath = os.path.join(inTempDir,'bootloader_with_settings.hex')
gAppWithSoftDevicePath = os.path.join(inTempDir,'app_sd.hex')

gBootloaderSettingsPath = os.path.join(inTempDir, "bootloader_settings.hex")

print ("inSourceFile = " + inSourceFile)
print ("inTempDir = " + inTempDir)
print ("inOutputDir = " + inOutputDir)
print ("")
print ("SoftDevice path = " + gSoftDeviceHexPath)
print ("Bootloader path = " + gBootloaderHexPath)
print ("Merge hex tool path = " + com.mergeHexToolPath)
print ("")
print ("Build script running from " + gScriptDir)




#if os.path.isfile(gOutputFilePath):
#    com.die("The output file " + gOutputFilePath + " already exists. Please clean it first.")

com.deleteFileIfExists(gOutputFilePath)

com.createDirIfNotExist(inOutputDir)
com.createDirIfNotExist(inTempDir)

os.chdir(gScriptDir)

print("GENERATE BOOTLOADER SETTINGS")

com.runPythonOrDie(com.normalizePath(os.path.join(com.nRFUtilToolPath, '..')),
                [com.nRFUtilToolPath,
                'settings', 'generate',
                '--family', 'NRF52810',
                '--application', com.quotePath(inSourceFile),
                '--application-version', str(inAppVersion),
                '--bootloader-version', str(ver.gBootloaderVersion),
                '--bl-settings-version', str(ver.gBootloaderSettingsVersion),
                com.quotePath(gBootloaderSettingsPath)])

com.fileExistsOrDie(gBootloaderSettingsPath)

print("MERGE BOOTLOADER WITH SETTINGS")

com.runCommandOrDie([com.mergeHexToolPath,
                '-m', com.quotePath2(gBootloaderHexPath), 
                com.quotePath2(gBootloaderSettingsPath),
                '-o', com.quotePath2(gBootloaderWithSettingsPath) ])

com.fileExistsOrDie(gBootloaderWithSettingsPath)

print("MERGE APP WITH SOFTDEVICE")
com.runCommandOrDie([com.mergeHexToolPath,
                '-m', com.quotePath2(inSourceFile),
                com.quotePath2(gSoftDeviceHexPath),
                '-o', com.quotePath2(gAppWithSoftDevicePath) ])

com.fileExistsOrDie(gAppWithSoftDevicePath)

print("MERGE FINAL IMAGE")
com.runCommandOrDie([com.mergeHexToolPath,
                '-m', com.quotePath2(gBootloaderWithSettingsPath),
                com.quotePath2(gAppWithSoftDevicePath),
                '-o', com.quotePath2(gFinalImagePath) ])

com.fileExistsOrDie(gFinalImagePath)

shutil.copy2(gFinalImagePath, gOutputFilePath)

com.fileExistsOrDie(gOutputFilePath)