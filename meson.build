#
# project definition
#
project('libgovirt', 'c',
        version : '0.3.8',
        license : 'LGPLv2.1',
        meson_version: '>= 0.49.0')

#
# global vars
#
compiler = meson.get_compiler('c')
govirt_version = meson.project_version()
govirt_include = [include_directories('.')]
govirt_includedir = get_option('prefix') / get_option('includedir')
govirt_include_subdir = 'govirt-1.0' / 'govirt'
govirt_deps = []

# libtool versioning - this applies to libgovirt
#
# See http://sources.redhat.com/autobook/autobook/autobook_91.html#SEC91
# for details
#
# increment;
# CURRENT	If the API or ABI interface has changed (reset REVISION to 0)
# REVISION	If the API and ABI remains the same, but bugs are fixed.
# AGE		Don't use.
govirt_current = 5
govirt_revision = 0
govirt_age = 3
govirt_so_version = '@0@.@1@.@2@'.format(govirt_current - govirt_age,
                                         govirt_age,
                                         govirt_revision)

#
# check for dependencies
#
glib_version_info = '>= 2.38'
glib_encoded_version = 'GLIB_VERSION_2_38'

deps = {'gobject-2.0' : glib_version_info,
        'gio-2.0' : glib_version_info,
        'gthread-2.0' : glib_version_info,
        'gobject-introspection-1.0' : '>= 1.30.0',
        'rest-0.7' : '>= 0.7.92',}

foreach dep, version : deps
  govirt_deps += dependency(dep, version : version)
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
                        '-Wno-strict-aliasing',
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
