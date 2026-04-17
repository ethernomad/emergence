em_game_inc = include_directories('.')

em_client_inc = include_directories('em-client')
em_server_inc = include_directories('em-server')

libcommon_game = static_library('common-game',
  common_src,
  c_args: common_c_args + ['-DEMGAME'],
  include_directories: [common_inc, em_client_inc, em_server_inc],
)

shared_src = files(
  'shared/bsp.c',
  'shared/config.c',
  'shared/cvar.c',
  'shared/parse.c',
  'shared/sgame.c',
  'shared/timer.c',
  'shared/network.c',
  'shared/alarm.c',
  'shared/openssl.c',
  'shared/fileinfo.c',
  'shared/servers.c',
)

shared_inc = include_directories('shared')

if not is_x86
  shared_src += files('shared/rdtsc_fallback.c')
endif

libshared_client = static_library('shared-client',
  shared_src,
  c_args: common_c_args + ['-DEMCLIENT'],
  include_directories: [common_inc, shared_inc, em_client_inc],
)

libshared_server = static_library('shared-server',
  shared_src,
  c_args: common_c_args + ['-DEMSERVER'],
  include_directories: [common_inc, shared_inc, em_server_inc],
)

sdl_dep = dependency('sdl', required: false)
if not sdl_dep.found()
  sdl_dep = dependency('SDL', required: false)
endif

openssl_dep = dependency('openssl', required: false)
if not openssl_dep.found()
  openssl_dep = cc.find_library('ssl', required: false)
  crypto_dep = cc.find_library('crypto', required: true)
else
  crypto_dep = openssl_dep
endif

x11_dep = dependency('x11', required: false)
xext_dep = dependency('xext', required: false)
xrandr_dep = dependency('xrandr', required: false)
gl_dep = dependency('gl', required: false)
glu_dep = dependency('glu', required: false)
alsa_dep = dependency('alsa', required: false)
vorbisfile_dep = dependency('vorbisfile', required: false)
vorbis_dep = dependency('vorbis', required: false)
ogg_dep = dependency('ogg', required: false)
zlib_dep = dependency('zlib', required: false)
png_dep = dependency('libpng', required: false)
m_dep = cc.find_library('m', required: false)
pthread_dep = dependency('threads')

client_src = files(
  'em-client/bsp.c',
  'em-client/entry.c',
  'em-client/game.c',
  'em-client/main.c',
  'em-client/map.c',
  'em-client/ping.c',
  'em-client/stars.c',
  'em-client/rcon.c',
  'em-client/tick.c',
  'em-client/console.c',
  'em-client/event.c',
  'em-client/render.c',
  'em-client/control.c',
  'em-client/line.c',
  'em-client/input.c',
  'em-client/rotate.c',
  'em-client/skin.c',
  'em-client/particles.c',
  'em-client/screenshot.c',
  'em-client/x.c',
  'em-client/ris.c',
  'em-client/sound.c',
  'em-client/floats.c',
  'em-client/key.c',
  'em-client/download.c',
  'em-client/servers.c',
)

em_client = executable('em-client',
  client_src,
  link_with: [libgsub_game, libshared_client, libcommon_game],
  dependencies: [
    sdl_dep,
    openssl_dep,
    crypto_dep,
    x11_dep,
    xext_dep,
    xrandr_dep,
    gl_dep,
    glu_dep,
    alsa_dep,
    vorbisfile_dep,
    vorbis_dep,
    ogg_dep,
    zlib_dep,
    png_dep,
    m_dep,
    pthread_dep,
  ],
  c_args: common_c_args + ['-DEMCLIENT'],
  include_directories: [em_client_inc, em_game_inc, common_inc, gsub_inc, shared_inc],
  install: true,
)

server_src = files(
  'em-server/console.c',
  'em-server/game.c',
  'em-server/main.c',
  'em-server/ping.c',
  'em-server/entry.c',
  'em-server/key.c',
  'em-server/download.c',
  'em-server/servers.c',
)

em_server = executable('em-server',
  server_src,
  link_with: [libshared_server, libcommon_game],
  dependencies: [
    openssl_dep,
    crypto_dep,
    zlib_dep,
    m_dep,
    pthread_dep,
  ],
  c_args: common_c_args + ['-DEMSERVER'],
  include_directories: [em_server_inc, em_game_inc, common_inc, shared_inc],
  install: true,
)

install_data(
  'share/smallfont.png',
  'share/default-controls.config',
  'share/default-client.autoexec',
  'share/default-server.autoexec',
  install_dir: join_paths(get_option('datadir'), 'emergence'),
)

install_data(
  'share/stock-maps/map3.cmap',
  'share/stock-maps/map4.cmap',
  install_dir: join_paths(get_option('datadir'), 'emergence', 'stock-maps'),
)

install_data(
  'share/stock-skins/default.skin',
  install_dir: join_paths(get_option('datadir'), 'emergence', 'stock-skins'),
)

install_data(
  'share/demos/fun2.demo',
  'share/demos/fun4.demo',
  'share/demos/insane2.demo',
  install_dir: join_paths(get_option('datadir'), 'emergence', 'demos'),
)

game_object_textures = [
  'craft-shield.png',
  'mine.png',
  'plasma.png',
  'rocket.png',
  'weapon-shield.png',
]

foreach f : game_object_textures
  install_data(
    join_paths('share/stock-object-textures', f),
    install_dir: join_paths(get_option('datadir'), 'emergence', 'stock-object-textures'),
  )
endforeach

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
  'desktop/em-client.desktop',
  'desktop/em-server.desktop',
  install_dir: join_paths(get_option('datadir'), 'applications'),
)
