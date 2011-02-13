# -*- mode: python -*-

import os

source = ['utils/logger.cc',
          'utils/misc.cc',
          'core/buffer.cc',
          'core/pipeline.cc',
          'core/server.cc',
          'core/stages.cc',
          'core/wrapper.cc']

cflags = '-g'
inc_path = ['.']
libflags = ['pthread', 'boost_thread-mt']

if ARGUMENTS.get('release', 0) == '1':
    cflags = '-O3 -march=native'

env = Environment(ENV=os.environ, CFLAGS=cflags, CXXFLAGS=cflags,
                  CPPPATH=inc_path, LIBS=libflags)
libpipeserv = env.SharedLibrary('pipeserv', source=source)

def GenTestProg(name, src):
    env.Program(name, source=src, LINKFLAGS=[libpipeserv, '-Wl,-rpath=.'])

GenTestProg('test/hash_server', 'test/hash_server.cc')
GenTestProg('test/pingpong_server', 'test/pingpong_server.cc')
