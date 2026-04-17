misc_libs = [libgsub_misc, libcommon_misc]
misc_deps = [zlib_dep, png_dep, m_dep]

shield = executable('shield',
  'shield/main.c',
  link_with: misc_libs,
  dependencies: misc_deps,
  c_args: common_c_args,
  include_directories: [common_inc, gsub_inc],
  install: true,
)

plasma = executable('plasma',
  'plasma/main.c',
  link_with: misc_libs,
  dependencies: misc_deps,
  c_args: common_c_args,
  include_directories: [common_inc, gsub_inc],
  install: true,
)
