#!/usr/bin/env python3

import sys
import subprocess

try:
    if subprocess.check_call(('git', 'rev-parse', '--is-inside-work-tree'),
                             stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL) == 0:
        count = subprocess.check_output(('git', 'rev-list', '--count', 'HEAD')).decode().strip()
        hash_ = subprocess.check_output(('git', 'rev-parse', '--short', 'HEAD')).decode().strip()
        print('r{}.{}'.format(count, hash_))
except:
    sys.exit(1)
