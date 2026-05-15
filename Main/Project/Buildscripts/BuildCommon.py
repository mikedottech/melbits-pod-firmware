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
import subprocess
import sys
import os
import platform

def runCommand(cmd):
    if getPlatform() == "osx":
        sep = ' '
        cmdline = sep.join(cmd)
        print(">> RUNNING " + cmdline)
        res = os.system(cmdline)
        return res
    else:
        print(">> RUNNING " + str(cmd))
        process = subprocess.Popen(cmd, shell=False, stderr=sys.stderr, stdout=sys.stdout)
        process.wait()	
        return process.returncode

def runPython(workingdir, cmd):
    sep = ' '
    cmdline = sep.join(cmd)
    #pycmd = 'python -m pipenv run python'
    pycmd = 'py -2 '
    cwd = os.getcwd()
    print(">> CHANGING TO DIR " + workingdir)
    os.chdir(os.path.dirname(workingdir))
    print(">> RUNNING " + cmdline)
    res = os.system(pycmd + " " + cmdline)
    print("<< BACK TO DIR " + cwd)
    os.chdir(cwd)
    return res 

def normalizePath(p):
    return os.path.normpath(os.path.abspath(p))

def die(msg):
    sys.stderr.write("ERROR: " + msg)
    exit(1)
    
def runCommandOrDie(cmd):
    cmd[0] = quotePath2(normalizePath(cmd[0]))    
    sys.stdout.flush()
    sys.stderr.flush()
    err = runCommand(cmd)
    if err != 0:
        die("Process returned error code " + str(err))
    sys.stdout.flush()
    sys.stderr.flush()

def runPythonOrDie(workingdir, cmd):
    cmd[0] = quotePath(normalizePath(cmd[0]))
    sys.stdout.flush()
    sys.stderr.flush()
    err = runPython(workingdir, cmd)
    if err != 0:
        die("Process returned error code " + str(err))
    sys.stdout.flush()
    sys.stderr.flush()

def createDirIfNotExist(path):
    if not os.path.isdir(path):
        print("Creating directory " + path)
        os.makedirs(path)

def deleteFileIfExists(path):
    if os.path.isfile(path):
        print("Deleting file " + path)
        os.remove(path)

def fileExistsOrDie(path):
    if not os.path.isfile(path):
        die("File " + path + " does not exist")

def validateNRFEnvironment():
        # Pre checks
        if os.environ.get('NRF5SDK') == None:
                die("NRF5SDK environment variable must be set and pointing to the nRF5 SDK")

        if os.environ.get('NRFCLTOOLS') == None:
                die("NRFCLTOOLS environment variable must be set and pointing to the nRF command line tools directory")

def validateProductId(idstr):
        intval = int(sys.argv[5], 16)
        print("Product ID is " + format(intval, 'x'))
        if len(idstr) < 6:
                die("Invalid product ID length for " + idstr)
        if len(idstr) > 6 and len(idstr) < 8:
                die("Invalid extended product ID length for " + idstr)

def getPlatform():
    platforms = {
        'linux1' : 'Linux',
        'linux2' : 'Linux',
        'darwin' : 'osx',
        'win32' : 'win64' #hack 
    }
    if sys.platform not in platforms:
        return sys.platform
    
    return platforms[sys.platform]

def preChecks():
    if getPlatform() == "osx":
        os.environ['PATH'] = '/usr/local/bin/:' + os.environ['PATH']

#    if sys.version_info[0] > 2:
#        die("Python 3 is not supported. Please use python 2")
    ret = os.system("pipenv --version")
    if ret != 0:
        die("pipenv not found. Please install pipenv")

def quotePath(pth):
    return '"' + pth + '"'	

def quotePath2(pth):
    if getPlatform() != "win64":
        return '"' + pth + '"'
    else:
        return pth

# Runs the command without normalizing the path to the executable
def runCommandOrDie2(cmd):
    cmd[0] = quotePath2(cmd[0])    
    sys.stdout.flush()
    sys.stderr.flush()
    err = runCommand(cmd)
    if err != 0:
        die("Process returned error code " + str(err))
    sys.stdout.flush()
    sys.stderr.flush()
    

def runSetup():
    runCommandOrDie2(['python', '-m', 'ensurepip'])
    runCommandOrDie2(['python', '-m', 'pip', 'install', 'pipenv'])
    cwd = os.getcwd()    
    os.chdir(os.path.normpath(os.path.join("..", "..", "..", "Tools", "NRFUtil")))
    runCommandOrDie2(['python', '-m', 'pipenv', 'sync'])
    os.chdir(cwd)
    runCommandOrDie2(['python', '-m', 'pip', 'install', 'ffmpeg-normalize'])

gExeSuffix = ""
if getPlatform() == "win64":
    gExeSuffix = ".exe"

mergeHexToolPath = os.path.join('..', '..', '..', 'Tools', 'NRF5CLI', getPlatform() ,'nRF-Command-Line-Tools_10_3_0/', 'mergehex', 'mergehex' + gExeSuffix)
nRFJProgToolPath = os.path.join('..', '..', '..', 'Tools', 'NRF5CLI', getPlatform() ,'nRF-Command-Line-Tools_10_3_0/', 'nrfjprog', 'nrfjprog' + gExeSuffix)
nRFUtilToolPath = os.path.join('..', '..', '..', 'Tools', 'NRFUtil', 'nordicsemi', '__main__.py')
ffmpegToolPath = os.path.join('..', '..', '..', 'Tools', 'ffmpeg', getPlatform() , 'ffmpeg' + gExeSuffix)
soxToolPath = os.path.join('..', '..', '..', 'Tools', 'sox', getPlatform() , 'sox' + gExeSuffix)
