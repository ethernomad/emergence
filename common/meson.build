common_src = files(
  'buffer.c',
  'polygon.c',
  'stringbuf.c',
  'abs2rel.c',
  'rel2abs.c',
  'inout.c',
  'user.c',
  'prefix.c',
  'resource.c',
)

common_inc = include_directories('.')

libcommon_tools = static_library('common-tools',
  common_src,
  c_args: common_c_args,
  include_directories: common_inc,
)

libcommon_misc = static_library('common-misc',
  files('buffer.c', 'polygon.c', 'stringbuf.c', 'abs2rel.c', 'rel2abs.c', 'inout.c'),
  c_args: common_c_args,
  include_directories: common_inc,
)
