bgrep [![Build Status](https://travis-ci.org/JonathonReinhart/bgrep.svg?branch=master)](https://travis-ci.org/JonathonReinhart/bgrep) [![codecov.io](https://codecov.io/github/JonathonReinhart/bgrep/coverage.svg?branch=master)](https://codecov.io/github/JonathonReinhart/bgrep?branch=master)
=====

A binary grep-like tool


### Build

bgrep uses [SCons](http://scons.org/) to build. With SCons installed,
simply type 'scons' in the top-level directory to build.


### Usage

```
bgrep PATTERN FILE...
```

**PATTERN** is a sequence of hex bytes, e.g.  `1234FF`.

You can also use `.` to match any nibble. For example,
`DE.D` would match the bytes `DE AD` or `DE ED`.
