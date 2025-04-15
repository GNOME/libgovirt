#
# project definition
#
project('libgovirt', 'c',
        version : '0.3.9',
        license : 'LGPL-2.1-or-later',
        meson_version: '>= 0.49.0',
        default_options : ['c_std=c99',
                           'werror=true',
                           'warning_level=2'])


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
govirt_revision = 1
govirt_age = 3
govirt_so_version = '@0@.@1@.@2@'.format(govirt_current - govirt_age,
                                         govirt_age,
                                         govirt_revision)

#
# check for dependencies
#
glib_version_info = '>= 2.66'
glib_encoded_version = 'GLIB_VERSION_2_66'

gobject_dep = dependency('gobject-2.0', version : glib_version_info)
gio_dep = dependency('gio-2.0', version : glib_version_info)
gthread_dep = dependency('gthread-2.0', version : glib_version_info)
rest_dep = dependency('rest-1.0', version : '>= 0.9')

govirt_deps += [
    gobject_dep,
    gio_dep,
    gthread_dep,
    rest_dep,
]

#
# global C defines
#
govirt_global_cflags = ['-DG_LOG_DOMAIN="libgovirt"',
                        '-DGLIB_VERSION_MIN_REQUIRED=@0@'.format(glib_encoded_version),
                        '-DGLIB_VERSION_MAX_ALLOWED=@0@'.format(glib_encoded_version),
                        '-Wcast-align',
                        '-Wno-error=cast-align',
                        '-Wmissing-declarations',
                        '-Wpointer-arith',
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

add_project_arguments(compiler.get_supported_arguments(govirt_global_cflags),
                      language : 'c')

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
