gsub_src = files(
  'gsub.c',
  'surfaces.c',
  'blit_ops.c',
  'text.c',
  'line.c',
  'resample.c',
)

gsub_asm_src = files(
  'alpha_pixel_plot_x86.S',
  'convert_16bit_to_32bit_mmx.S',
  'alpha_rect_draw_mmx.S',
  'fb_update_mmx.S',
  'surface_blit_mmx.S',
  'alpha_surface_blit_mmx.S',
  'bb_clear_mmx.S',
)

gsub_inc = include_directories('.')

is_x86_32 = host_machine.cpu() == 'x86'

libgsub_game = static_library('gsub-game',
  gsub_src + (is_x86_32 ? gsub_asm_src : []),
  c_args: common_c_args,
  include_directories: [common_inc, gsub_inc],
)

if gtk_dep.found()
  libgsub_tools = static_library('gsub-tools',
    gsub_src + (is_x86_32 ? gsub_asm_src : []),
    c_args: common_c_args + ['-DUSE_GDK_PIXBUF'],
    dependencies: gtk_dep,
    include_directories: [common_inc, gsub_inc],
  )
endif

libgsub_misc = static_library('gsub-misc',
  gsub_src + (is_x86_32 ? gsub_asm_src : []),
  c_args: common_c_args,
  include_directories: [common_inc, gsub_inc],
)
