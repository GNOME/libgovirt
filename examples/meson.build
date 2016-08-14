examples = ['sync-test',
            'async-test',
            'list-vms',
            'list-vms-async']

foreach e : examples
  executable(e, '@0@.c'.format(e), dependencies: govirt_lib_dep)
endforeach
