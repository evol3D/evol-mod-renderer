project('volk', 'c',
  default_options: [
    'default_library=static',
  ],
)

vulkan_dep = dependency('vulkan').partial_dependency(compile_args : true)

volk_inc = include_directories('.')

volk_src = files('volk_impl.c')

volk_lib = library(
  'volk',
  volk_src,
  include_directories: volk_inc,
  dependencies: vulkan_dep
)

volk_dep = declare_dependency(
  link_with: volk_lib,
  include_directories: volk_inc,
  dependencies: vulkan_dep
)

meson.override_dependency('volk', volk_dep)
