#
# project definition
#
project('libgovirt', 'c',
        version : '0.3.4',
        license : 'LGPLv2.1',
        meson_version: '>= 0.46.0')

#
# global vars
#
compiler = meson.get_compiler('c')
govirt_version = meson.project_version()
govirt_include = [include_directories('.')]
govirt_includedir = join_paths(get_option('prefix'), get_option('includedir'))
govirt_include_subdir = join_paths('govirt-1.0', 'govirt')
govirt_deps = []

#
# check for dependencies
#
glib_version_info = '>= 2.38'
glib_encoded_version = 'GLIB_VERSION_2_38'

deps = [['gobject-2.0', glib_version_info],
        ['gio-2.0', glib_version_info],
        ['gthread-2.0', glib_version_info],
        ['gobject-introspection-1.0', '>= 1.30.0'],
        ['rest-0.7', '>= 0.7.92'],]

foreach dep : deps
  govirt_deps += dependency(dep[0], version : dep[1])
endforeach

#
# global C defines
#
govirt_global_cflags = ['-std=c99',
                        '-DG_LOG_DOMAIN="libgovirt"',
                        '-DGLIB_VERSION_MIN_REQUIRED=@0@'.format(glib_encoded_version),
                        '-DGLIB_VERSION_MAX_ALLOWED=@0@'.format(glib_encoded_version),
                        '-Wall',
                        '-Wextra',
                        '-Werror',
                        '-Wcast-align',
                        '-Wmissing-declarations',
                        '-Wpointer-arith',
                        '-Wcast-align',
                        '-Wwrite-strings',
                        '-Winit-self',
                        '-Wreturn-type',
                        '-Wformat-nonliteral',
                        '-Wformat-security',
                        '-Wmissing-include-dirs',
                        '-Wmissing-format-attribute',
                        '-Wclobbered',
                        '-Wempty-body',
                        '-Wignored-qualifiers',
                        '-Wsign-compare',
                        '-Wtype-limits',
                        '-Wuninitialized',
                        '-Waggregate-return',
                        '-Wdeclaration-after-statement',
                        '-Wshadow',
                        '-Winline',
                        '-Wmissing-parameter-type',
                        '-Woverride-init',
                        '-Wno-unused-parameter']

foreach arg : govirt_global_cflags
  add_project_arguments(arg, language : 'c')
endforeach

subdir('govirt')
subdir('po')
subdir('examples')
subdir('tests')


#
# write config.h
#
config_data = configuration_data()
config_data.set_quoted('GETTEXT_PACKAGE', 'libgovirt')
config_data.set_quoted('PACKAGE_STRING', 'libgovirt @0@'.format(govirt_version))

configure_file(output : 'config.h', configuration : config_data)
