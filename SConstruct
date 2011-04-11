import os
import platform

AddOption('--prefix', dest='prefix', metavar='DIR', help='installation prefix')
AddOption('--libdir', dest='libdir', metavar='DIR', help='libdata dir')

opts = Variables('configure.conf')
opts.Add('PREFIX', default='/usr/local')
opts.Add('LIBDIR', default='/usr/local/lib')

def GetOS():
    return platform.uname()[0]

def PassEnv(name, dstname):
    if name in os.environ:
        env[dstname] = os.environ[name]

def CompilerMTOption():
    if GetOS() == 'Linux' or os == 'FreeBSD':
        return ' -pthread'
    elif GetOS() == 'SunOS':
        return ' -pthreads'
    else:
        return ''

essential_cflags = ' -pipe -Wall'
cflags = '-g -DLOG_ENABLED'
inc_path = ['.', '/usr/local/include']

if ARGUMENTS.get('release', 0) == '1':
    cflags = '-Os -mtune=generic'

env = Environment(ENV=os.environ, CFLAGS=cflags, CXXFLAGS=cflags,
                  CPPPATH=inc_path)

opts.Update(env)

PassEnv('CFLAGS', 'CFLAGS')
PassEnv('CXXFLAGS', 'CXXFLAGS')
PassEnv('LDFLAGS', 'LINKFLAGS')

env.Append(CFLAGS=essential_cflags + CompilerMTOption(), CXXFLAGS=essential_cflags + CompilerMTOption())

if GetOption('prefix') is not None:
    env['PREFIX'] = GetOption('prefix')
    env['LIBDIR'] = GetOption('prefix') + '/lib'
    
if GetOption('libdir') is not None:
    env['LIBDIR'] = GetOption('libdir')

opts.Save('configure.conf', env)

Export('opts', 'env', 'GetOS')
SConscript('./SConscript', variant_dir='build', duplicate=0)
