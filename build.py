import ninja_syntax as n
from subprocess import run
import sys
import os
import platform

join = os.path.join

is_windows = platform.system() == 'Windows'

def exe(name):
    if is_windows:
        return f'{name}.exe'
    else:
        raise Exception("Windows only")

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

    out.rule(
        name    = 'compile_c',
        depfile = '$out.d',
        command = ' '.join([
                  'clang',
                  '-MD -MF $out.d',
                  '-Wall -Wextra -Wpedantic',
                  '-Wsign-conversion',
                  '-Wimplicit-function-declaration',
                  '-Wno-unused-function',
                  '-Wno-sign-conversion', # temporarily disabled
                  '-Werror=switch', # Enforce all enum values are handled
                  '-Werror=incompatible-pointer-types',
                  '-fansi-escape-codes -fcolor-diagnostics',
                  '-std=c23',
                  '-march=native',
                  '-Iext -Iext/SDL -Iext/embree-4.4.0/include -Iext/cglm',
                  '$cflags',
                  '-c',
                  '$in',
                  '-o $out',
                  ])
        )

    #'/Iext /Iext/SDL /Iext/embree-4.4.0/include /Iext/cglm',
    out.rule(
        name = 'build_binary',
        command = ' '.join([
            'clang',
            '$in',
            '-o $out',
            '$libs'
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
        fout = outd(f'{f}.o')
        outputs.append(fout)
        out.build(
            outputs   = fout,
            rule      = 'compile_c',
            inputs    = f,
            variables = {
                'cflags': '-O2',
            },
            )

    out.build(
        outputs = outd(exe('kingfisher')),
        rule    = 'build_binary',
        inputs  = [outd(f'{f}.o') for f in inputs],
        variables = {
            'libs': ['-l' + x for x in [
                'ext/SDL/debug/SDL3.lib', 
                'ext/embree-4.4.0/lib/embree4.lib',
                'ext/embree-4.4.0/lib/tbb12.lib',
                'ext/cglm/cglm.lib',
                'kernel32.lib', # IsDebuggerPresent
                ]],
            #'lflags': '/SUBSYSTEM:CONSOLE',
        },
        )

if __name__ == '__main__':
    create_build_ninja()
    run(['ninja'] + sys.argv[1:])
