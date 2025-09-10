sources_headers = files(
  'gnome-cmd-con.h',
  'gnome-cmd-dir.h',
  'gnome-cmd-includes.h',
  'gnome-cmd-main-win.h',
  'gnome-cmd-path.h',
  'gnome-cmd-types.h',
  'utils.h',
)

sources_sources = files(
  'gnome-cmd-application.cc',
  'gnome-cmd-dir.cc',
  'utils.cc',
)

cc_args = [
  '-DGNOMELOCALEDIR="' + datadir + '/locale"',
  '-DSHARE_DIR="' + pkgdatadir + '"',
  '-DG_DISABLE_SINGLE_INCLUDES',
  '-DGDK_PIXBUF_DISABLE_SINGLE_INCLUDES',
  '-DGTK_DISABLE_SINGLE_INCLUDES'
]

rpath = get_option('prefix') / get_option('libdir')

libgcmdmain = static_library('gcmd-main',
  c_args: cc_args,
  sources: [sources_headers, sources_sources],
  dependencies: [common_deps],
  include_directories: [configuration_inc, sources_inc, libgcmd_inc],
)

executable('gcmd-block', 'gnome-cmd-block.cc',
  include_directories: [configuration_inc],
  install: true,
  install_rpath: rpath
)

config_rs = configure_file(
  input: 'config.rs.in',
  output: 'config.rs',
  configuration: conf_data
)
run_command(
  'cp',
  meson.project_build_root() / 'src' / 'config.rs',
  meson.project_source_root() / 'src' / 'config.rs',
  check: true
)

cargo_options = [
  '--manifest-path', meson.project_source_root() / 'Cargo.toml',
  '--target-dir', meson.project_build_root() / 'src',
]

if profile == 'debug'
  rust_target = 'debug'
  message('Building in debug mode')
else
  cargo_options += [ '--release' ]
  rust_target = 'release'
  message('Building in release mode')
endif

cargo_env = [
  'CARGO_HOME=' + meson.project_build_root() / 'cargo-home',
  'RUSTFLAGS=-L ' + (meson.project_build_root() / 'src')
        + ' -L ' + (meson.project_build_root() / 'libgcmd')
        + ' -lgcmd'
        + ' -C link-arg=-Wl,-rpath=' + rpath,
]

libgmcddialogs_dep = declare_dependency(
  include_directories: [include_directories('.')],
)

commander = custom_target(
  'gnome-commander',
  build_by_default: true,
  build_always_stale: true,
  input: [config_rs, libgcmdmain],
  output: meson.project_name(),
  console: true,
  install: true,
  install_dir: bindir,
  command: [
    'env',
    cargo_env,
    cargo, 'build', cargo_options,
    '&&',
    'cp', 'src' / rust_target / meson.project_name(), '@OUTPUT@',
  ]
)

test(
  'cargo-test', cargo,
  env: cargo_env,
  args: ['test', '--no-fail-fast'] + cargo_options,
  depends: [commander, libgcmd],
  timeout: 180
)
