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

def validateCommandLine():
    if len(sys.argv) < 2:
        com.die("Usage: " + os.path.basename(__file__) + " <.hex firmware file to flash>")
        exit(1)

# Pre checks
com.preChecks()

validateCommandLine()
inSourceFile = os.path.normpath(sys.argv[1])

gScriptDir = os.path.dirname(os.path.abspath(__file__))

print ("inSourceFile = " + inSourceFile)
print ("")
print ("Build script running from " + gScriptDir)


# Pre checks
#com.validateNRFEnvironment()

com.fileExistsOrDie(inSourceFile)

os.chdir(gScriptDir)

print("ERASE CHIP")
if com.runCommand([com.nRFJProgToolPath, '-e']) == 16:
        # try recovering first
        com.runCommandOrDie([com.nRFJProgToolPath,
                        '--recover'])
        com.runCommandOrDie([com.nRFJProgToolPath,
                        '-e'])

print("FLASH HEX IMAGE")

com.runCommandOrDie([com.nRFJProgToolPath,
                '-f', 'NRF52',
                '--program', com.quotePath2(inSourceFile)])

print("RESET CPU")

com.runCommandOrDie([com.nRFJProgToolPath, '-r'])

