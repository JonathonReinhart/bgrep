#!/usr/bin/env python
import sys
import os
import errno
import os.path
from os.path import dirname, realpath, join
from subprocess import Popen, PIPE
import re
import shutil
import tempfile

class TestFailure(Exception):
    pass


# http://stackoverflow.com/a/600612/119527
def mkdir_p(path):
    try:
        os.makedirs(path)
    except OSError as exc:
        if exc.errno == errno.EEXIST and os.path.isdir(path):
            pass
        else: raise


def do_bgrep(pattern, paths, options=[], retcode=0, quiet=False, raw_output=False):
    args = [bgrep_path]
    args += list(options)
    if pattern:
        args.append(pattern)
    args += list(paths)
    
    if not quiet:
        print '$ ' + ' '.join(args)

    p = Popen(args, stdout=PIPE)
    stdout, stderr = p.communicate()

    if p.returncode != retcode:
        raise TestFailure('Return code: {0}, expected: {1}'.format(p.returncode, retcode))

    if raw_output:
        if not quiet:
            print stdout,
        return stdout

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
                result[filename] = set()

            result[filename].add(offset)

    return result


def assert_equal(expected, actual):
    if not expected == actual:
        raise TestFailure('Expected: {0}, Actual {1}'.format(expected, actual))


def single_test(data, pattern, offsets, retcode=0, options=[]):
    filename = 'test.bin'
    with open(filename, 'wb') as f:
        f.write(data)

    result = do_bgrep(pattern, [filename], retcode=retcode, options=options)

    for retfilename, retoffsets in result.iteritems():
        assert_equal(filename, retfilename)
        assert_equal(set(offsets), retoffsets)

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
    single_test(data, pattern.encode('hex'), offsets)
    single_test(data, pattern, offsets, options=['-s'])

def no_find():
    data = 'notherenotgonnafindit'
    single_test(data, 'HOLYGRAIL'.encode('hex'), [], retcode=1)
    single_test(data, 'HOLYGRAIL', [], retcode=1, options=['-s'])

def multiple_offsets():
    offsets = [4, 27, 369, 630, 750]
    pattern = '\x12\x34\x56\x78'
    data = gen_padded_data(offsets, pattern)
    single_test(data, pattern.encode('hex'), offsets)
    single_test(data, pattern, offsets, options=['-s'])

def no_overlap():
    offsets = [123, 456]
    data = gen_padded_data(offsets, 'cacac')
    single_test(data, 'cac'.encode('hex'), offsets)
    single_test(data, 'cac', offsets, options=['-s'])

def pattern_wild_full():
    for p in xrange(0, 10):
        pad = '00'*p
        data = (pad + '12345678' + pad).decode('hex')
        pattern = '..345678'
        offsets = [p]
        single_test(data, pattern, offsets)

def pattern_wild_high():
    for p in xrange(0, 10):
        pad = '00'*p
        data = (pad + '12345678' + pad).decode('hex')
        pattern = '.2345678'
        offsets = [p]
        single_test(data, pattern, offsets)

def pattern_wild_low():
    for p in xrange(0, 10):
        pad = '00'*p
        data = (pad + '12345678' + pad).decode('hex')
        pattern = '1.345678'
        offsets = [p]
        single_test(data, pattern, offsets)

def recursive():
    basedir = tempfile.mkdtemp(prefix='bgreptest')

    files = {
        join(basedir,'some.bin'):       {0x60, 0x20},
        join(basedir,'other.bin'):      {0x68, 0x78, 0x92},
        join(basedir,'a/data.bin'):     {0x70},
        join(basedir,'a/b/data.bin'):   {0x80, 0x74},
        join(basedir,'a/b/c/foo.bin'):  {0x120},
        join(basedir,'a/b/c/bar.bin'):  {0x230},
        join(basedir,'x/data.bin'):     {0x90},
        join(basedir,'x/y/foo.bin'):    {0x100},
        join(basedir,'x/y/bar.bin'):    {0x110},
    }

    pattern = '\x12\x34\x56\x78'

    for path,offsets in files.iteritems():
        dname, fname = os.path.split(path)
        data = gen_padded_data(offsets, pattern)
        mkdir_p(dname)
        with open(path, 'wb') as f:
            f.write(data)

    result = do_bgrep(pattern.encode('hex'), [basedir], options=['-r'])
    assert_equal(files, result)

    shutil.rmtree(basedir)

def invalid_pattern_byte():
    do_bgrep('abcdeZ', ['n/a'], retcode=1)

def invalid_pattern_highnib():
    do_bgrep('abcdZ.', ['n/a'], retcode=1)

def invalid_pattern_lownib():
    do_bgrep('abcd.Z', ['n/a'], retcode=1)

def invalid_pattern_oddlen():
    do_bgrep('abcde', ['n/a'], retcode=1)

def test_version():
    result = do_bgrep(None, [], options=['-v'], raw_output=True)
    if not 'bgrep version' in result:
        raise TestFailure('Unexpcted -v output: ' + result)


all_tests = [
    basic_test,
    no_find,
    multiple_offsets,
    no_overlap,

    pattern_wild_full,
    pattern_wild_high,
    pattern_wild_low,

    recursive,

    invalid_pattern_byte,
    invalid_pattern_highnib,
    invalid_pattern_lownib,
    invalid_pattern_oddlen,

    test_version,
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
