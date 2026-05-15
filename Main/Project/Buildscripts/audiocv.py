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
import argparse
import subprocess
import json
import tempfile
import shutil
import threading
import concurrent.futures

import BuildCommon as com

def run(cmd):
    process = subprocess.Popen(cmd, shell=True, stderr=subprocess.PIPE, stdout=subprocess.PIPE)
    (output, err) = process.communicate()
    process.wait()
    if process.returncode != 0:
        print(err.decode("utf-8"))
        raise Exception("External program error")
    return (output, err)


def run2(cmd):
    process = subprocess.Popen(cmd, shell=True, stderr=sys.stderr, stdout=sys.stdout)
    #(output, err) = process.communicate()
    process.wait()
    if process.returncode != 0:
        raise Exception("External program error")
    #return (output, err)

ap = argparse.ArgumentParser()

# Add the arguments to the parser
ap.add_argument("i", type=str, metavar="input", help="Input file or dir")
ap.add_argument("out", type=str, metavar="outout", help="Output file", default="")
ap.add_argument("-l", "--loop", required=False, default=0, type=int, nargs=2, metavar=("start", "end"), dest="loop", help="Loop start and end in samples") 
ap.add_argument("-w", "--wavoutput", required=False, help="Output in wav format", dest="w", action='store_true')
ap.add_argument("-k", "--keepintermediate", required=False, help="Keep intermediate files", dest="k", action='store_true')

#print(str(sys.argv[1:]))

try:
    args = vars(ap.parse_args(sys.argv[1:]))
except:
    ap.print_help()
    sys.exit(2)

#print(str(args))

args['i'] = args['i'].strip('"')
args['out'] = args['out'].strip('"')

def process(fin, fout, args):
    print(">> " + fin)
    # Step 1 - Downmix stereo to mono, remove silence
    tfname = tempfile.mktemp() + ".wav"
    run([com.quotePath2(com.ffmpegToolPath), "-i", fin, 
        "-ac", "1",
        #"-ar", "8000",        
        #"-af", "silenceremove=start_periods=1:start_duration=0:start_threshold=-60dB:detection=peak,areverse,silenceremove=start_periods=1:start_duration=0:start_threshold=-60dB:detection=peak,areverse,dynaudnorm=p=0.95:m=100:s=12:g=15:f=10",
        #"-af", "dynaudnorm=p=0.95:m=100:s=12:g=15:f=10",
        #"-af", "silenceremove=start_periods=1:start_silence=0.01:start_threshold=-96dB,areverse,silenceremove=start_periods=1:start_silence=0.01:start_threshold=-96dB,areverse",
        "-af", "silenceremove=1:0:-40dB,areverse,silenceremove=1:0:-40dB,areverse", #+
        #",equalizer=f=200:width_type=h:width=50:g=-3" +
        #",equalizer=f=300:width_type=h:width=200:g=-6" +
        #",equalizer=f=600:width_type=h:width=400:g=-9" +
        #",equalizer=f=1100:width_type=h:width=200:g=-20" +
        #",equalizer=f=2000:width_type=h:width=500:g=-10" +
        #",equalizer=f=3000:width_type=h:width=500:g=-4",
        tfname])        
        
    # Step 2 - Normalize loudness
    # Set FFMPEG PATH
    gScriptDir = os.path.dirname(os.path.abspath(__file__))
    os.environ['FFMPEG_PATH'] = com.ffmpegToolPath #os.path.normalize(os.path.join(gScriptDir,"..","..","..", "Tools","ffmpeg","win64","ffmpeg.exe"))
    tfnorm = tempfile.mktemp() + ".wav"
    run(["ffmpeg-normalize", tfname, "-nt", "rms", "-t", "-11", "-f", "-o", tfnorm])

#EQ:
# Freq Gain Width
# 200 -3 50
# 300 -6 200

# 600 -9 400

# 1100 -20 200
# 2000 -10 500
# 3000 -4 500

    # Step 2 resample to 8 KHz, volume boost and encoding into GSM
    tfname2 = tempfile.mktemp()
    
    if args['w']:
        tfname2 += "_2.wav"
    else:
        tfname2 += "_2.gsm"

    run([com.quotePath2(com.soxToolPath), tfnorm, "-r", "8000", tfname2
        #, "silence", "1", "0.3", "2%", "1", "0.3", "8%"
        #, "vol", "1.2dB"
        ])
    
    if args['w']:
        shutil.copy2(tfname2, fout)
    else:
        # Step 3 - Generate header and dump final file                
        with open(fout, 'wb') as f:
            d = [0xAD,  # Magic
                0x00,   # Version
                0x00,   # Format (GSM 13kbps)
                ]
            if args['loop']:
                d += [0x01]
                d += [bytes(args['loop'][0]), bytes(args['loop'][1])]
            else:
                d += [0x00]            
            
            f.write(bytes(d))
            with open(tfname2, "rb") as f2:
                f.write(f2.read())
                f2.close()
            f.flush()
            f.close()    

    if not args['k']:
        os.remove(tfname)
        os.remove(tfname2)

def walklevel(some_dir, level=1):
    some_dir = some_dir.rstrip(os.path.sep)
    assert os.path.isdir(some_dir)
    num_sep = some_dir.count(os.path.sep)
    for root, dirs, files in os.walk(some_dir):
        yield root, dirs, files
        num_sep_this = root.count(os.path.sep)
        if num_sep + level <= num_sep_this:
            del dirs[:]

inFiles = []
if os.path.isdir(args['i']):
    if not os.path.isdir(args['out']):        
        os.mkdir(args['out'])
    for dirpath, dnames, fname in walklevel(args['i']):
        for f in fname:
            if f.endswith(".mp3") or f.endswith(".wav"):            
                inFiles.append(os.path.join(dirpath, f))   
else:
    if os.path.isfile(args['i']):
        inFiles.append(args['i'])
    else:
        raise("Invalid input file or directory")

jobs = []
with concurrent.futures.ThreadPoolExecutor(max_workers=5) as executor:
    for f in inFiles:
        srcFName = os.path.splitext(os.path.basename(f))[0]
        if os.path.isdir(args['i']):   # Dir mode
            if args['out'] == "": # No output specified. Defaults to input dir            
                fout = os.path.join(args['i'], srcFName)
            else: # Output dir specified
                fout = os.path.join(args['out'], srcFName)
        else: # fIle mode
            fout = os.path.join(os.path.dirname(args['out']), srcFName)

        fout += ("_pod_.wav" if args['w'] else ".ppa")
        jobs.append(executor.submit(process, f, fout, args))
    
    for job in concurrent.futures.as_completed(jobs):
        # Read result from future
        result_done = job.result()

#except:
    #print("ERROR")
    #sys.exit(3)

