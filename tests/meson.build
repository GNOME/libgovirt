test_govirt_sources = ['test-govirt.c',
                       'mock-httpd.c',
                       'mock-httpd.h']

test_c_args = ['-Dabs_srcdir="@0@"'.format(meson.current_source_dir()),
               '-Dsrcdir="@0@"'.format(meson.current_source_dir())]

test_govirt = executable('test-govirt', test_govirt_sources,
                         dependencies: govirt_lib_dep,
                         c_args : test_c_args)

test('test-govirt', test_govirt, env : ['GIO_USE_NETWORK_MONITOR=base'])
