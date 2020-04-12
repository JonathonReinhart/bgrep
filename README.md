bgrep [![Build Status](https://img.shields.io/travis/JonathonReinhart/bgrep.svg)](https://travis-ci.org/JonathonReinhart/bgrep) [![codecov.io](https://img.shields.io/codecov/c/github/JonathonReinhart/bgrep.svg)](https://codecov.io/github/JonathonReinhart/bgrep?branch=master) [![Coverty](https://img.shields.io/coverity/scan/7430.svg)](https://scan.coverity.com/projects/jonathonreinhart-bgrep)
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

### License
This software is released under the [Apache License 2.0](http://www.apache.org/licenses/LICENSE-2.0.html).
