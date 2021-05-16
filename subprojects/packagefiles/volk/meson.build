project('vma', ['c','cpp'])

vulkan_dep = dependency('vulkan').partial_dependency(compile_args : true)

vma_inc = include_directories('include')

vma_src = files('vk_mem_alloc.cpp')

mod_deps = [
  vulkan_dep,
]

vma_lib = library(
  'vma',
  vma_src,
  include_directories: vma_inc,
  dependencies: mod_deps
)

vma_dep = declare_dependency(
  link_with: vma_lib,
  include_directories: vma_inc,
  dependencies: mod_deps
)

meson.override_dependency('vma', vma_dep)
