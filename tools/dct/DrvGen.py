#! /usr/bin/python
# -*- coding: utf-8 -*-

import os, sys
import getopt
import traceback
import subprocess
import xml.dom.minidom

sys.dont_write_bytecode = True

sys.path.append('.')
sys.path.append('..')

from obj.ChipObj import ChipObj
from obj.ChipObj import Everest
from obj.ChipObj import Olympus
from obj.ChipObj import MT6757_P25
from obj.ChipObj import Rushmore
from obj.ChipObj import Whitney

from utility.util import LogLevel
from utility.util import log

def usage():
    print '''
usage: DrvGen [dws_path] [file_path] [log_path] [paras]...

options and arguments:

dws_path    :    dws file path
file_path   :    where you want to put generated files
log_path    :    where to store the log files
paras        :    parameter for generate wanted file
'''

def is_oldDws(path, gen_spec):
    if not os.path.exists(path):
        log(LogLevel.error, 'Can not find %s' %(path))
        sys.exit(-1)

    try:
        root = xml.dom.minidom.parse(dws_path)
    except Exception, e:
        log(LogLevel.warn, '%s is not xml format, try to use old DCT!' %(dws_path))
        if len(gen_spec) == 0:
            log(LogLevel.warn, 'Please use old DCT UI to gen all files!')
            return True
        old_dct = os.path.join(sys.path[0], 'old_dct', 'DrvGen')
        cmd = old_dct + ' ' + dws_path + ' ' + gen_path + ' ' + log_path + ' ' + gen_spec[0]
        if 0 == subprocess.call(cmd, shell=True):
            return True
        else:
            log(LogLevel.error, '%s format error!' %(dws_path))
            sys.exit(-1)

    return False

if __name__ == '__main__':
    opts, args = getopt.getopt(sys.argv[1:], '')

    if len(args) == 0:
        msg = 'Too less arguments!'
        usage()
        log(LogLevel.error, msg)
        sys.exit(-1)

    dws_path = ''
    gen_path = ''
    log_path = ''
    gen_spec = []

    # get DWS file path from parameters
    dws_path = os.path.abspath(args[0])

    # get parameters from input
    if len(args) == 1:
        gen_path = os.path.dirname(dws_path)
        log_path = os.path.dirname(dws_path)

    elif len(args) == 2:
        gen_path = os.path.abspath(args[1])
        log_path = os.path.dirname(dws_path)

    elif len(args) == 3:
        gen_path = os.path.abspath(args[1])
        log_path = os.path.abspath(args[2])

    elif len(args) >= 4:
        gen_path = os.path.abspath(args[1])
        log_path = os.path.abspath(args[2])
        for i in range(3,len(args)):
            gen_spec.append(args[i])

    log(LogLevel.info, 'DWS file path is %s' %(dws_path))
    log(LogLevel.info, 'Gen files path is %s' %(gen_path))
    log(LogLevel.info, 'Log files path is %s' %(log_path))

    for item in gen_spec:
        log(LogLevel.info, 'Parameter is %s' %(item))



    # check DWS file path
    if not os.path.exists(dws_path):
        log(LogLevel.error, 'Can not find "%s", file not exist!' %(dws_path))
        sys.exit(-1)

    if not os.path.exists(gen_path):
        log(LogLevel.error, 'Can not find "%s", gen path not exist!' %(gen_path))
        sys.exit(-1)

    if not os.path.exists(log_path):
        log(LogLevel.error, 'Can not find "%s", log path not exist!' %(log_path))
        sys.exit(-1)

    if is_oldDws(dws_path, gen_spec):
        sys.exit(0)

    chipId = ChipObj.get_chipId(dws_path)
    log(LogLevel.info, 'chip id: %s' %(chipId))
    chipObj = None
    if cmp(chipId, 'MT6797') == 0:
        chipObj = Everest(dws_path, gen_path)
    elif cmp(chipId, 'MT6757') == 0:
        chipObj = Olympus(dws_path, gen_path)
    elif cmp(chipId, 'MT6757-P25') == 0:
        chipObj = MT6757_P25(dws_path, gen_path)
    elif cmp(chipId, 'KIBOPLUS') == 0:
        chipObj = MT6757_P25(dws_path, gen_path)
    elif cmp(chipId, 'MT6570') == 0:
        chipObj = Rushmore(dws_path, gen_path)
    elif cmp(chipId, 'MT6799') == 0:
        chipObj = Whitney(dws_path, gen_path)
    else:
        chipObj = ChipObj(dws_path, gen_path)

    if not chipObj.parse():
        log(LogLevel.error, 'Parse %s fail!' %(dws_path))
        sys.exit(-1)

    if not chipObj.generate(gen_spec):
        log(LogLevel.error, 'Generate files fail!')
        sys.exit(-1)

    sys.exit(0)

