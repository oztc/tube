# -*- mode: python -*-
from SCons import SConf

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
               'http/capi_impl.cc',
               'http/module.c']

http_server_source = ['http/server.cc']

epoll_source = ['core/poller_impl/epoll_poller.cc']
kqueue_source = ['core/poller_impl/kqueue_poller.cc']
port_completion_source = ['core/poller_impl/port_completion_poller.cc']

Import('env', 'GetOS')

def LinuxSpecificConf(ctx):
    global source
    conf = ctx.sconf
    if not SConf.CheckCHeader(ctx, 'sys/epoll.h'):
        ctx.Message('Failed because kernel doesn\'t suport epoll')
        return False
    if SConf.CheckCHeader(ctx, 'sys/sendfile.h'):
        conf.Define('USE_LINUX_SENDFILE')
    if not SConf.CheckLib(ctx, 'dl'):
        ctx.Message('Cannot find dl')
        return False
    conf.Define('USE_EPOLL')
    source += epoll_source
    ctx.Result(0)
    return True

def FreeBSDSpecificConf(ctx):
    global source
    conf = ctx.sconf
    if not SConf.CheckCHeader(ctx, ['sys/types.h', 'sys/event.h']):
        ctx.Message('Failed because kernel doesn\'t support kqueue')
        return False
    if Sconf.CheckFunction(ctx, 'sendfile'):
        conf.Define('USE_FREEBSD_SENDFILE')
    conf.Define('USE_KQUEUE')
    source += kqueue_source
    return True

def SolarisSpecificConf(ctx):
    global source
    conf = ctx.sconf
    if not SConf.CheckLib(ctx, 'socket'):
        ctx.Message('Socket library not found')
        return False
    if not SConf.CheckCHeader(ctx, 'port.h'):
        ctx.Message('Failed because kernel doesn\'t support port completion framework')
        return False
    if SConf.CheckLibWithHeader(ctx, 'sendfile', 'sys/sendfile.h', 'c'):
        conf.Define('USE_LINUX_SENDFILE')
    conf.Define('USE_PORT_COMPLETION')
    global source
    source += port_completion_source
    return True

if not env.GetOption('clean'):
    specific_conf = None;
    boost_headers = ['boost/noncopyable.hpp', 'boost/function.hpp', 'boost/bind.hpp', 'boost/shared_ptr.hpp', 'boost/xpressive/xpressive.hpp']
    
    if GetOS() == 'Linux':
        specific_conf = LinuxSpecificConf
    elif GetOS() == 'FreeBSD':
        specific_conf = FreeBSDSpecificConf
    elif GetOS() == 'SunOS':
        specific_conf = SolarisSpecificConf
    else:
        print 'Kernel not supported yet'
        Exit(1)
    
    conf = env.Configure(config_h='config.h', custom_tests={'SpecificConf': specific_conf})
    if not conf.CheckLibWithHeader('yaml-cpp', 'yaml-cpp/yaml.h', 'cxx'):
        Exit(1)
    if not conf.CheckLib('boost_thread-mt') and not conf.CheckLib('boost_thread'):
        Exit(1)
    for header in boost_headers:
        if not conf.CheckCXXHeader(header):
            Exit(1)
    if not conf.SpecificConf():
        Exit(1)
    env = conf.Finish()

env.Command('http/http_parser.c', 'http/http_parser.rl', 'ragel -s -G2 $SOURCE -o $TARGET')
libtube = env.SharedLibrary('tube', source=source)
libtube_web = env.SharedLibrary('tube-web', source=http_source)
tube_server = env.Program('tube-server', source=http_server_source, LINKFLAGS=env['LINKFLAGS'] + ' -Lbuild', LIBS=['tube', 'tube-web'])

def GenTestProg(name, src):
    ldflags = [libtube, libtube_web]
    if GetOS() == 'Linux' or GetOS() == 'SunOS':
        ldflags.append('-Wl,-rpath=.')
    env.Program(name, source=src, LINKFLAGS=[env['LINKFLAGS']] + ldflags)

GenTestProg('test/hash_server', 'test/hash_server.cc')
GenTestProg('test/pingpong_server', 'test/pingpong_server.cc')
GenTestProg('test/test_buffer', 'test/test_buffer.cc')
GenTestProg('test/file_server', 'test/file_server.cc')
GenTestProg('test/test_http_parser', 'test/test_http_parser.cc')
GenTestProg('test/test_config', 'test/test_config.cc')
GenTestProg('test/test_web', 'test/test_web.cc')

# Install
env.Alias('install', [
        env.Install('$LIBDIR/', [libtube, libtube_web]),
        env.Install('$PREFIX/bin/', tube_server)
        ])
