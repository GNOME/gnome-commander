cc_args = [
  '-DGNOMELOCALEDIR="' + datadir + '/locale"',
  '-DSHARE_DIR="' + pkgdatadir + '"',
]

rpath = get_option('prefix') / get_option('libdir')

executable('gcmd-block', 'gnome-cmd-block.cc',
  c_args: cc_args,
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
  input: [config_rs],
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
