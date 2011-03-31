# -*- mode: python -*-

import platform
import os

source = ['utils/logger.cc',
          'utils/misc.cc',
          'core/poller.cc',
          'core/buffer.cc',
          'core/pipeline.cc',
          'core/inet_address.cc',
          'core/stream.cc',
          'core/filesender.cc',
          'core/server.cc',
          'core/stages.cc',
          'core/wrapper.cc']

http_source = ['http/http_parser.c',
               'http/connection.cc',
               'http/http_wrapper.cc',
               'http/interface.cc',
               'http/static_handler.cc',
               'http/configuration.cc',
               'http/io_cache.cc',
               'http/http_stages.cc',
               'http/capi_impl.cc']

epoll_source = ['core/poller_impl/epoll_poller.cc']
kqueue_source = ['core/poller_impl/kqueue_poller.cc']

essential_cflags = ' -Wall'
cflags = '-g -DLOG_ENABLED'
inc_path = ['.', '/usr/local/include']
libflags = ['pthread', 'boost_thread-mt', 'yaml-cpp']

if ARGUMENTS.get('release', 0) == '1':
    cflags = '-O3 -march=native'

def PassEnv(name, dstname):
    if name in os.environ:
        env[dstname] = os.environ[name]

def GetOS():
    return platform.uname()[0]    

cflags += essential_cflags

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
    if GetOS() == 'Linux' and conf.CheckCHeader('sys/sendfile.h'):
        conf.Define('USE_LINUX_SENDFILE')
    elif GetOS() == 'FreeBSD' and conf.CheckFunction('sendfile'):
        conf.Define('USE_FREEBSD_SENDFILE')
    env = conf.Finish()

PassEnv('CFLAGS', 'CFLAGS')
PassEnv('CXXFLAGS', 'CXXFLAGS')
PassEnv('LDFLAGS', 'LINKFLAGS')

env.Command('http/http_parser.c', 'http/http_parser.rl', 'ragel -s -G2 $SOURCE -o $TARGET')
libpipeserv = env.SharedLibrary('pipeserv', source=source)
libpipeserv_web = env.SharedLibrary('pipeserv_web', source=http_source)

def GenTestProg(name, src):
    ldflags = [libpipeserv, libpipeserv_web]
    if GetOS() == 'Linux':
        ldflags.append('-Wl,-rpath=.')
    env.Program(name, source=src, LINKFLAGS=[env['LINKFLAGS']] + ldflags)

GenTestProg('test/hash_server', 'test/hash_server.cc')
GenTestProg('test/pingpong_server', 'test/pingpong_server.cc')
GenTestProg('test/test_buffer', 'test/test_buffer.cc')
GenTestProg('test/file_server', 'test/file_server.cc')
GenTestProg('test/test_http_parser', 'test/test_http_parser.cc')
GenTestProg('test/test_config', 'test/test_config.cc')
GenTestProg('test/test_web', 'test/test_web.cc')
