from sys import byteorder

env_lib = Environment(CCFLAGS='-O3 -Wall -Werror')
env_prog = env_lib.Clone()

def check_pkgconfig(context, version):
	context.Message( 'Checking for pkg-config... ' )
	ret = context.TryAction('pkg-config --atleast-pkgconfig-version=%s' % version)[0]
	context.Result(ret)
	return ret

def check_pkg(context, name):
	context.Message( 'Checking for %s... ' % name )
	ret = context.TryAction('pkg-config --exists \'%s\'' % name)[0]
	context.Result(ret)
	return ret

conf = Configure(env_prog,
	custom_tests = { 'check_pkgconfig' : check_pkgconfig,
							     'check_pkg' : check_pkg })

if byteorder == "big":
	print 'Your target appears to be a big-endian architecture, ' \
	      'which is not supported at the moment.'
	Exit(1)

if not conf.check_pkgconfig('0.15.0'):
	print 'pkg-config >= 0.15.0 not found.'
	Exit(1)

if not conf.check_pkg('glib-2.0'):
	print 'glib-2.0 not found.'
	Exit(1)

if not conf.check_pkg('gtk+-2.0'):
	print 'gtk+-2.0 not found.'
	Exit(1)

if not conf.check_pkg('gdk-pixbuf-2.0'):
	print 'gdk-pixbuf-2.0 not found.'
	Exit(1)

conf.Finish()

env_lib.ParseConfig('pkg-config --cflags --libs glib-2.0 gdk-pixbuf-2.0')
env_lib.StaticLibrary('libthalia.a', Glob("libthalia/*.c"))

env_prog.ParseConfig('pkg-config --cflags --libs gtk+-2.0')
env_prog.Program('thalia', Glob("thalia_gui.c") + ["libthalia.a"])
