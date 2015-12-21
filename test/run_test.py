#!/usr/bin/env python
import sys
import os
from os.path import dirname, realpath, join
from subprocess import Popen, PIPE
import re

class TestFailure(Exception):
    pass


def do_bgrep(pattern, paths, options=[], retcode=0, quiet=False):
    args = [bgrep_path]
    args += list(options)
    args.append(pattern.encode('hex'))
    args += list(paths)
    
    if not quiet:
        print '$ ' + ' '.join(args)

    p = Popen(args, stdout=PIPE)
    stdout, stderr = p.communicate()

    if p.returncode != retcode:
        raise TestFailure('Return code: {0}, expected: {1}'.format(p.returncode, retcode))

    result = {}
    if p.returncode == 0:
        pat = re.compile('^(.*):(0x[0-9A-Fa-f]+).*')

        for line in stdout.splitlines():
            if not quiet:
                print line

            m = pat.match(line)
            if not m: continue

            filename = m.group(1)
            offset = int(m.group(2), 16)

            if not filename in result:
                result[filename] = []

            result[filename].append(offset)

    return result


def assert_equal(expected, actual):
    if not expected == actual:
        raise TestFailure('Expected: {0}, Actual {1}'.format(expected, actual))


def single_test(data, pattern, offsets, retcode=0):
    filename = 'test.bin'
    with open(filename, 'wb') as f:
        f.write(data)

    result = do_bgrep(pattern, [filename], retcode=retcode)

    for retfilename, retoffsets in result.iteritems():
        assert_equal(filename, retfilename)
        assert_equal(set(offsets), set(retoffsets))

    # Don't put this in a try/finally, so it hangs around on failure
    # for post-mortem
    os.remove(filename)

def gen_padded_data(offsets, insert, padchar='\0'):
    data = ''
    for o in sorted(offsets):
        padamt = o - len(data)
        assert padamt >= 0
        data += padchar*padamt + insert
    return data

################################################################################
# Tests

def basic_test():
    offsets = [100]
    pattern  = '\x12\x34\x56\x78' 
    data = gen_padded_data(offsets, pattern)
    single_test(data, pattern, offsets)

def no_find():
    data = 'notherenotgonnafindit'
    single_test(data, 'HOLYGRAIL', [], retcode=1)

def multiple_offsets():
    offsets = [4, 27, 369, 630, 750]
    pattern = '\x12\x34\x56\x78'
    data = gen_padded_data(offsets, pattern)
    single_test(data, pattern, offsets)

def no_overlap():
    offsets = [123, 456]
    data = gen_padded_data(offsets, 'cacac')
    single_test(data, 'cac', offsets)

all_tests = [
    basic_test,
    no_find,
    multiple_offsets,
    no_overlap,
]

################################################################################

def setup():
    global bgrep_path
    my_path = realpath(join(dirname(__file__), '..'))
    bgrep_path = join(my_path, 'bgrep')
    print 'bgrep path:', bgrep_path

def termcolor(n):
    return '\033[{0}m'.format(n)
RED = 31
GREEN = 32

def main():
    setup()

    failures = 0
    passes = 0

    def message(m, color=0):
        print '[{0:03}] {1}: '.format(n, name) + \
              termcolor(color) + m + termcolor(0)

    for n,test in enumerate(all_tests):
        name = test.__name__
        print '-'*80
        message('Starting')
        try:
            test()
        except TestFailure as tf:
            message('Failure: {0}'.format(tf), RED)
            failures += 1
        else:
            message('Success', GREEN)
            passes += 1

    print '-'*80
    print '{0}/{1} tests passed.'.format(passes, len(all_tests))

    if failures:
        print termcolor(RED) + 'FAILURE' + termcolor(0)
        sys.exit(1)
    else:
        print termcolor(GREEN) + 'Success!' + termcolor(0)
        sys.exit(0)

if __name__ == '__main__':
    main()
