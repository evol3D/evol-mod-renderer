project('vma', ['c','cpp'])

vulkan_dep = dependency('vulkan').partial_dependency(compile_args : true)

vma_inc = include_directories('include')

vma_src = files('vk_mem_alloc.cpp')

vma_lib = library(
  'vma',
  vma_src,
  include_directories: vma_inc,
  dependencies: vulkan_dep
)

vma_dep = declare_dependency(
  link_with: vma_lib,
  include_directories: vma_inc,
  dependencies: vulkan_dep
)

meson.override_dependency('vma', vma_dep)
