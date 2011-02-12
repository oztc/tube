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

test_source = ['test/hash_server.cc']
env.Program('test/hash_server', source=test_source, LINKFLAGS=[libpipeserv, '-Wl,-rpath=.'])
