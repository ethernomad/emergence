if zlib_dep.found() and png_dep.found()
  em_skin = executable('em-skin',
    files('em-skin/main.c'),
    link_with: [libgsub_misc, libcommon_tools],
    dependencies: [zlib_dep, png_dep, m_dep],
    c_args: common_c_args,
    include_directories: [common_inc, gsub_inc],
    install: true,
  )
endif

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

  install_data(
    'share/gravity-well-placeholder.png',
    'share/node.png',
    'share/point.png',
    'share/select.png',
    'share/spawn-point-placeholder.png',
    'share/splash.png',
    'share/stock-speedup-ramp.svg',
    'share/teleporter-placeholder.png',
    'share/vect-sat.png',
    'share/width-sat.png',
    install_dir: join_paths(get_option('datadir'), 'emergence'),
  )

  install_data(
    'share/stock-textures/bukuj.png',
    install_dir: join_paths(get_option('datadir'), 'emergence', 'stock-textures'),
  )

  install_data(
    'pixmaps/emergence.png',
    install_dir: join_paths(get_option('datadir'), 'pixmaps'),
  )

  install_data(
    'pixmaps/emergence.png',
    install_dir: join_paths(get_option('datadir'), 'emergence', 'pixmaps'),
  )

  install_data(
    'desktop/em-edit.desktop',
    install_dir: join_paths(get_option('datadir'), 'applications'),
  )
endif
