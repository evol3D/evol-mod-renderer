project('evol-mod-renderer', 'c',
version : '0.1',
default_options : ['warning_level=3', 'c_std=c11'])

src = subproject('evol').get_variable('evolmod_subdir') + '/meson.build'
dst = meson.current_source_dir() + '/import/'
run_command( 'python3', meson.source_root() + '/subprojects/evol/buildscripts/copy.py', src, dst)
subdir('import')

volk_dep = dependency('volk')
vma_dep = dependency('vma')
spvref_dep = dependency('spvref')

mod_src = [
  'src/mod.c',

  'src/Vulkan/Vulkan.c',
  'src/Vulkan/Swapchain.c',
  'src/Vulkan/VulkanQueueManager.c',
  'src/Vulkan/Pipeline.c',
]

mod_incdir = [
  '..',
  'src/Vulkan'
]

mod_deps = [
  evmod_deps,
  volk_dep,
  vma_dep,
  spvref_dep
]

module = shared_module(
  'evmodrenderer', mod_src,
  include_directories: mod_incdir,
  dependencies: mod_deps,
)

mod_dep = declare_dependency(
  include_directories: mod_incdir,
  dependencies: vma_dep
)

meson.override_dependency('evmod_renderer', mod_dep)
