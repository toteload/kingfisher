#!/usr/bin/env python3

import ninja_syntax as n
from subprocess import run
import sys
import os
import platform

join = os.path.join

def outd(p):
    return join('$outdir', p.replace('\\', '__'))

def create_build_ninja():
    fout = open('build.ninja', 'w')
    out = n.Writer(fout)

    out._line('builddir = .ninja')

    out.variable(
        key   = 'outdir',
        value = 'out',
        )

    out.variable(
        key   = 'msvc_deps_prefix',
        value = 'Note: including file:',
        )

    out.rule(
        name    = 'compile_c',
        deps    = 'msvc',
        command = ' '.join([
                  'cl',
                  '/nologo',
                  '/showIncludes',
                  '/W4',
                  '/wd4201 /wd4100 /wd4189',
                  '/Iext /Iext/SDL /Iext/embree-4.4.0/include',
                  '/Oi',
                  '/Zi /FS',
                  '/arch:AVX512',
                  '$cflags',
                  '-c',
                  '$in',
                  '/Fo:$out',
                  ])
        )

    out.rule(
        name = 'build_binary',
        command = ' '.join([
            'cl',
            '/nologo',
            '/Fe:$out',
            '$cflags',
            '$in',
            '$libs',
            '/link',
            '$lflags',
            ])
        )

    inputs = [
        *[join('src', x) for x in [
            'main.c',
            'cie_xyz_lut.c',
            'bvh.c',
            'aabb.c',
            'camera.c',
            'ui.c',
            'worker.c',
        ]],
        join('ext', 'ufbx.c'),
    ]

    outputs = []

    for f in inputs:
        fout = outd(f'{f}.obj')
        outputs.append(fout)
        out.build(
            outputs   = fout,
            rule      = 'compile_c',
            inputs    = f,
            variables = {
                'cflags': '/O2',
            },
            )

    out.build(
        outputs = 'kingfisher.exe',
        rule    = 'build_binary',
        inputs  = [outd(f'{f}.obj') for f in inputs],
        variables = {
            'libs': [
                'ext/SDL/debug/SDL3.lib', 
                'ext/embree-4.4.0/lib/embree4.lib',
                'ext/embree-4.4.0/lib/tbb12.lib',
                'kernel32.lib', # IsDebuggerPresent
                ],
            'cflags': '/Zi',
            'lflags': '/SUBSYSTEM:CONSOLE',
        },
        )

if __name__ == '__main__':
    create_build_ninja()
    run(['ninja'] + sys.argv[1:])
