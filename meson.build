project('jfs-lib', 'c', version: '0.1')

inc = include_directories('include')
src = files('src/error.c')

jcl = static_library('jcl', src, include_directories: inc, install: true)

jcl_dep = declare_dependency(
  include_directories: inc,
  link_with: jcl
)

meson.override_dependency('jcl', jcl_dep)
