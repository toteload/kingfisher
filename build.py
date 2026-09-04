import ninja_syntax as n
from subprocess import run, call
import sys
import os
import platform

join = os.path.join

VULKAN_PATH = os.environ['VULKAN_SDK']
OUT_PATH = 'out'
VENDOR_PATH = 'vendor'

def exe(name):
    return f'{name}.exe'

def outd(p):
    return join('$outdir', p.replace('\\', '__').replace('$', ''))

def out_shader(p):
    return join('$outdir', os.path.basename(p))

def merge_vars(a,b):
    return {k: a.get(k, "") + " " + b.get(k, "") for k in a.keys() | b.keys()}

def merge(a,b):
    if a == None:
        return b
    elif b == None:
        return a
    else:
        pass

def create_build_ninja():
    fout = open('build.ninja', 'w')
    out = n.Writer(fout)

    out._line('builddir = .ninja')

    out.variable(
        key   = 'outdir',
        value = OUT_PATH,
        )

    out.variable(
        key   = 'vendor',
        value = VENDOR_PATH,
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
                  '-Werror=switch', # Enforce all enum values are handled
                  '-Werror=incompatible-pointer-types',
                  '-fansi-escape-codes -fcolor-diagnostics',
                  '-std=c23',
                  '-march=native',
                  '-I$vendor -I$vendor/SDL -I$vendor/embree-4.4.0/include -I$vendor/cglm',
                  f'-isystem {VULKAN_PATH}/Include',
                  '$cflags',
                  '-c',
                  '$in',
                  '-o $out',
                  ])
        )

    out.rule(
        name = 'build_binary',
        command = ' '.join([
            'clang',
            '$lflags',
            '-g',
            '$in',
            '-o $out',
            '$libs'
            ])
        )

    out.rule(
        name = 'compile_shader',
        depfile = '$out.d',
        command = ' '.join([
            f'{VULKAN_PATH}/Bin/glslc',
            '-MD -MF $out.d',
            '-O',
            '--target-env=vulkan1.3',
            '--target-spv=spv1.4',
            '-fshader-stage=$stage',
            '$in',
            '-o $out',
        ])
    )

    shaders = [
        (join('src', 'test.comp.glsl'), 'compute'),
        (join('src', 'trace_test.comp.glsl'), 'compute'),
    ]

    for f, stage in shaders:
        out.build(
            outputs = out_shader(f'{f}.spv'),
            rule = 'compile_shader',
            inputs = f,
            variables = {'stage': stage},
        )

    inputs = [
        *[(join('src', x), {'cflags': '-g -gcodeview -O2'}) for x in [
            'main.c',
            'cie_xyz_lut.c',
            'bvh.c',
            'aabb.c',
            'camera.c',
            'ui.c',
            'worker.c',
            'toteload.c',
            'vk.c',
            'model.c',
        ]],
        (join('$vendor', 'ufbx.c'), {'cflags': '-Wno-language-extension-token -O2'}),
        (join('$vendor', 'volk.c'), {'cflags': '-Wno-language-extension-token -O2'}),
    ]

    variables = { 'cflags': '', }

    for (f, vars) in inputs:
        fout = outd(f'{f}.o')
        out.build(
            outputs   = fout,
            rule      = 'compile_c',
            inputs    = f,
            variables = merge_vars(variables, vars),
            )

    out.build(
        outputs = outd(exe('kingfisher')),
        rule    = 'build_binary',
        inputs  = [outd(f'{f}.o') for f,_ in inputs],
        variables = {
            'libs': ['-l' + x for x in [
                '$vendor/SDL/debug/SDL3.lib', 
                '$vendor/embree-4.4.0/lib/embree4.lib',
                '$vendor/embree-4.4.0/lib/tbb12.lib',
                '$vendor/cglm/cglm.lib',
            ]],
            'lflags': '-Wl,/SUBSYSTEM:CONSOLE',
        },
        )

if __name__ == '__main__':
    create_build_ninja()
    run(['ninja'] + sys.argv[1:])
    comp_commands = open('compile_commands.json', 'w')
    call(['ninja', '-t', 'compdb', 'compile_c'], stdout=comp_commands)
