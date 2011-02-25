# -*- mode: python -*-

import platform
import os

source = ['utils/logger.cc',
          'utils/misc.cc',
          'core/poller.cc',
          'core/buffer.cc',
          'core/pipeline.cc',
          'core/inet_address.cc',
          'core/server.cc',
          'core/stages.cc',
          'core/wrapper.cc']

epoll_source = ['core/poller_impl/epoll_poller.cc']
kqueue_source = ['core/poller_impl/kqueue_poller.cc']

cflags = '-g -DLOG_ENABLED'
inc_path = ['.', '/usr/local/include']
libflags = ['pthread', 'boost_thread-mt']

if ARGUMENTS.get('release', 0) == '1':
    cflags = '-O3 -march=native'

env = Environment(ENV=os.environ, CFLAGS=cflags, CXXFLAGS=cflags,
                  CPPPATH=inc_path, LIBS=libflags)

if not env.GetOption('clean'):
    conf = env.Configure(config_h='config.h')
    if conf.CheckCHeader('sys/epoll.h'):
        conf.Define('USE_EPOLL')
        source += epoll_source
    if conf.CheckCHeader(['sys/types.h', 'sys/event.h']):
        conf.Define('USE_KQUEUE')
        source += kqueue_source
    env = conf.Finish()

def PassEnv(name, dstname):
    if name in os.environ:
        env[dstname] = os.environ[name]

PassEnv('CFLAGS', 'CFLAGS')
PassEnv('CXXFLAGS', 'CXXFLAGS')
PassEnv('LDFLAGS', 'LINKFLAGS')

env.VariantDir('build', '.')
libpipeserv = env.SharedLibrary('pipeserv', source=source)

def GetOS():
    return platform.uname()[0]

def GenTestProg(name, src):
    ldflags = [libpipeserv]
    if GetOS() == 'Linux':
        ldflags.append('-Wl,-rpath=.')
    env.Program(name, source=src, LINKFLAGS=[env['LINKFLAGS']] + ldflags)

GenTestProg('test/hash_server', 'test/hash_server.cc')
GenTestProg('test/pingpong_server', 'test/pingpong_server.cc')
GenTestProg('test/test_buffer', 'test/test_buffer.cc')
