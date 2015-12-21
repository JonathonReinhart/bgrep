#!/usr/bin/env python
import sys
import os
from os.path import dirname, realpath, join
from subprocess import Popen, PIPE
import re

class TestFailure(Exception):
    pass


def do_bgrep(pattern, paths, options=[], retcode=0):
    args = [bgrep_path]
    args += list(options)
    args.append(pattern.encode('hex'))
    args += list(paths)
    
    p = Popen(args, stdout=PIPE)
    stdout, stderr = p.communicate()

    if p.returncode != retcode:
        raise TestFailure('Return code: {0}, expected: {1}'.format(p.returncode, retcode))

    pat = re.compile('^(.*):(0x[0-9A-Fa-f]+).*')

    result = {}

    for line in stdout.splitlines():
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


def single_test(data, pattern, offsets):
    filename = 'test.bin'
    with open(filename, 'wb') as f:
        f.write(data)

    for retfilename, retoffsets in do_bgrep(pattern, [filename]).iteritems():
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
    multiple_offsets,
    no_overlap,
]

################################################################################

def main():
    global bgrep_path
    bgrep_path = join(dirname(__file__), '..', 'bgrep')
    print 'bgrep path:', bgrep_path

    failures = 0
    passes = 0

    for t in all_tests:
        name = t.__name__
        print '{0}: Starting'.format(name)
        try:
            t()
        except TestFailure as tf:
            print '{0}: Failure: {1}'.format(name, tf)
            failures += 1
        else:
            print '{0}: Success'.format(name)
            passes += 1

    print ''
    print '{0}/{1} tests passed.'.format(passes, len(all_tests))

    sys.exit(1 if failures else 0)

if __name__ == '__main__':
    main()
