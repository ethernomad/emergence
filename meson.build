project('emergence', 'c',
  version: '0.9',
  default_options: [
    'warning_level=2',
    'c_std=gnu11',
  ],
)

cc = meson.get_compiler('c')

pkgdatadir = join_paths(get_option('prefix'), get_option('datadir'), 'emergence')

common_c_args = ['-DLINUX', '-DVERSION="' + meson.project_version() + '"', '-DPKGDATADIR="' + pkgdatadir + '"']

if get_option('nonauthenticating')
  common_c_args += '-DNONAUTHENTICATING'
endif

host_cpu = host_machine.cpu_family()
is_x86 = host_cpu.startswith('x86')

zlib_dep = dependency('zlib', required: false)
png_dep = dependency('libpng', required: false)
m_dep = cc.find_library('m', required: false)
pthread_dep = dependency('threads')
gtk_dep = dependency('gtk+-2.0', required: get_option('editor'), disabler: true)

subdir('common')
subdir('gsub')
subdir('em-game')
subdir('em-tools')
subdir('misc')
