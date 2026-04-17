gtk_dep = dependency('gtk+-2.0', required: get_option('editor'), disabler: true)

if gtk_dep.found()
  edit_src = files(
    'em-edit/bezier.c',
    'em-edit/bsp.c',
    'em-edit/cameron.c',
    'em-edit/conic.c',
    'em-edit/conns.c',
    'em-edit/curves.c',
    'em-edit/fills.c',
    'em-edit/floats.c',
    'em-edit/glade.c',
    'em-edit/grid.c',
    'em-edit/interface.c',
    'em-edit/lines.c',
    'em-edit/main.c',
    'em-edit/main_lock.c',
    'em-edit/map.c',
    'em-edit/nodes.c',
    'em-edit/objects.c',
    'em-edit/points.c',
    'em-edit/support.c',
    'em-edit/tiles.c',
    'em-edit/worker.c',
    'em-edit/worker_thread.c',
  )

  em_edit_inc = include_directories('em-edit')

  em_edit = executable('em-edit',
    edit_src,
    link_with: [libgsub_tools, libcommon_tools],
    dependencies: [gtk_dep, zlib_dep, png_dep, m_dep, pthread_dep],
    c_args: common_c_args,
    include_directories: [em_edit_inc, common_inc, gsub_inc],
    install: true,
  )

  shared_object_textures = [
    'rails.png',
    'shield-pickup.png',
    'unowned-minigun.png',
    'unowned-plasma-cannon.png',
    'unowned-rocket-launcher.png',
  ]

  foreach f : shared_object_textures
    install_data(
      join_paths('../stock-object-textures', f),
      install_dir: join_paths(get_option('datadir'), 'emergence', 'stock-object-textures'),
    )
  endforeach

  install_data(
    'pixmaps/emergence.png',
    install_dir: join_paths(get_option('datadir'), 'pixmaps'),
  )

  install_data(
    'desktop/em-edit.desktop',
    install_dir: join_paths(get_option('datadir'), 'applications'),
  )
endif
