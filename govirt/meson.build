gnome = import('gnome')

govirt_headers = [
  'govirt.h',
  'ovirt-api.h',
  'ovirt-cdrom.h',
  'ovirt-cluster.h',
  'ovirt-collection.h',
  'ovirt-data-center.h',
  'ovirt-disk.h',
  'ovirt-error.h',
  'ovirt-host.h',
  'ovirt-options.h',
  'ovirt-proxy.h',
  'ovirt-resource.h',
  'ovirt-rest-call-error.h',
  'ovirt-storage-domain.h',
  'ovirt-types.h',
  'ovirt-vm-display.h',
  'ovirt-vm.h',
  'ovirt-vm-pool.h',
]

install_headers(govirt_headers, subdir : govirt_include_subdir)

govirt_enum_types = gnome.mkenums_simple('ovirt-enum-types',
                                         sources : govirt_headers,
                                         install_header : true,
                                         install_dir : govirt_includedir / govirt_include_subdir)

govirt_private_headers = [
  'govirt-private.h',
  'ovirt-action-rest-call.h',
  'ovirt-api-private.h',
  'ovirt-cluster-private.h',
  'ovirt-collection-private.h',
  'ovirt-data-center-private.h',
  'ovirt-disk-private.h',
  'ovirt-host-private.h',
  'ovirt-proxy-private.h',
  'ovirt-resource-private.h',
  'ovirt-rest-call.h',
  'ovirt-resource-rest-call.h',
  'ovirt-storage-domain-private.h',
  'ovirt-utils.h',
  'ovirt-vm-private.h',
]

govirt_enum_types_private = gnome.mkenums_simple('ovirt-enum-types-private',
                                                 sources : govirt_private_headers)

#
# libgovirt.so
#
govirt_sources = [
  'ovirt-action-rest-call.c',
  'ovirt-api.c',
  'ovirt-cdrom.c',
  'ovirt-cluster.c',
  'ovirt-collection.c',
  'ovirt-data-center.c',
  'ovirt-disk.c',
  'ovirt-error.c',
  'ovirt-host.c',
  'ovirt-options.c',
  'ovirt-proxy.c',
  'ovirt-proxy-deprecated.c',
  'ovirt-resource.c',
  'ovirt-resource-rest-call.c',
  'ovirt-rest-call.c',
  'ovirt-storage-domain.c',
  'ovirt-utils.c',
  'ovirt-vm.c',
  'ovirt-vm-display.c',
  'ovirt-vm-pool.c',
]

govirt_lib_sources = [
  govirt_enum_types,
  govirt_enum_types_private,
  govirt_headers,
  govirt_private_headers,
  govirt_sources,
]

govirt_syms = files('govirt.sym')
govirt_syms_path = meson.current_source_dir() / 'govirt.sym'
govirt_version_script = '-Wl,--version-script=@0@'.format(govirt_syms_path)

govirt_lib = library('govirt', govirt_lib_sources,
                     version : govirt_so_version,
                     dependencies : govirt_deps,
                     include_directories : govirt_include,
                     link_args : compiler.get_supported_link_arguments(govirt_version_script),
                     link_depends : govirt_syms,
                     install : true)

govirt_lib_dep = declare_dependency(link_with : govirt_lib,
                                   include_directories : govirt_include,
                                   dependencies : govirt_deps,
                                   sources : [govirt_enum_types[1], govirt_enum_types_private[1]])

#
# GoVirt-1.0.gir
#
govirt_introspection_sources = [
    govirt_headers,
    govirt_sources,
]

gnome.generate_gir(govirt_lib,
                   build_by_default : true,
                   sources : govirt_introspection_sources,
                   namespace : 'GoVirt',
                   nsversion : '1.0',
                   identifier_prefix : 'Ovirt',
                   symbol_prefix : 'ovirt',
                   export_packages : 'govirt-1.0',
                   includes : ['Gio-2.0', 'GObject-2.0', 'Rest-1.0' ],
                   install : true,
                   extra_args : ['--c-include=govirt/govirt.h'])

#
# write govirt-1.0.pc
#
pkgconfig = import('pkgconfig')
pkgconfig.generate(govirt_lib,
                   filebase : 'govirt-1.0',
                   description : 'goVirt is a GLib binding for oVirt REST API',
                   subdirs : 'govirt-1.0',
                   requires : [
                       gobject_dep,
                       gio_dep,
                       rest_dep,
                   ],
                   variables : 'exec_prefix=${prefix}')

