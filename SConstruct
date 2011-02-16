# -*- mode: python -*-

import os

source = ['utils/logger.cc',
          'utils/misc.cc',
          'core/poller.cc',
          'core/buffer.cc',
          'core/pipeline.cc',
          'core/server.cc',
          'core/stages.cc',
          'core/wrapper.cc']

linux_source = ['core/poller_impl/epoll_poller.cc']

cflags = '-g -DLOG_ENABLED'
inc_path = ['.']
libflags = ['pthread', 'boost_thread-mt']

if ARGUMENTS.get('release', 0) == '1':
    cflags = '-O3 -march=native'

source = source + linux_source

env = Environment(ENV=os.environ, CFLAGS=cflags, CXXFLAGS=cflags,
                  CPPPATH=inc_path, LIBS=libflags)
libpipeserv = env.SharedLibrary('pipeserv', source=source)

def GenTestProg(name, src):
    env.Program(name, source=src, LINKFLAGS=[libpipeserv, '-Wl,-rpath=.'])

GenTestProg('test/hash_server', 'test/hash_server.cc')
GenTestProg('test/pingpong_server', 'test/pingpong_server.cc')
