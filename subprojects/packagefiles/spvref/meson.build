project('spvref', ['c','cpp'])

spvref_src = files('spirv_reflect.c')

spvref_lib = library(
  'spvref',
  spvref_src,
)

spvref_dep = declare_dependency(
  link_with: spvref_lib,
)

meson.override_dependency('spvref', spvref_dep)
