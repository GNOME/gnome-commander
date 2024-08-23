# Find flex, configure lex generator
flex_min_version = '2.6.0'

flex_program = find_program('flex', required : true)
flexversion_response = run_command(flex_program, '--version', check: true)
flexversion_array = flexversion_response.stdout().strip().split(' ')
if flexversion_array.length() < 2
  error('Cannot parse "flex --version" output, expected something like "flex $version", got: '
        + flexversion_response.stdout().strip())
endif
flexversion = flexversion_array[1]
if flexversion.version_compare('>=' + flex_min_version)
  message('flex version @0@ >= @1@: YES'.format(flexversion, flex_min_version))
else
  error('flex version @0@ >= @1@: NO'.format(flexversion, flex_min_version))
endif

sources_headers = files(
  'cap.h',
  'dict.h',
  'dirlist.h',
  'eggcellrendererkeys.h',
  'filter.h',
  'gnome-cmd-about-plugin.h',
  'gnome-cmd-advrename-lexer.h',
  'gnome-cmd-advrename-profile-component.h',
  'gnome-cmd-app.h',
  'gnome-cmd-application.h',
  'gnome-cmd-chmod-component.h',
  'gnome-cmd-chown-component.h',
  'gnome-cmd-cmdline.h',
  'gnome-cmd-collection.h',
  'gnome-cmd-combo.h',
  'gnome-cmd-con-device.h',
  'gnome-cmd-con.h',
  'gnome-cmd-con-home.h',
  'gnome-cmd-con-list.h',
  'gnome-cmd-con-remote.h',
  'gnome-cmd-con-smb.h',
  'gnome-cmd-smb-net.h',
  'gnome-cmd-smb-path.h',
  'gnome-cmd-convert.h',
  'gnome-cmd-data.h',
  'gnome-cmd-dir.h',
  'gnome-cmd-dir-indicator.h',
  'gnome-cmd-file-collection.h',
  'gnome-cmd-file.h',
  'gnome-cmd-file-list.h',
  'gnome-cmd-file-list-actions.h',
  'gnome-cmd-file-popmenu.h',
  'gnome-cmd-file-selector.h',
  'gnome-cmd-hintbox.h',
  'gnome-cmd-includes.h',
  'gnome-cmd-main-menu.h',
  'gnome-cmd-main-win.h',
  'gnome-cmd-owner.h',
  'gnome-cmd-path.h',
  'gnome-cmd-plain-path.h',
  'gnome-cmd-quicksearch-popup.h',
  'gnome-cmd-regex.h',
  'gnome-cmd-selection-profile-component.h',
  'gnome-cmd-treeview.h',
  'gnome-cmd-types.h',
  'gnome-cmd-user-actions.h',
  'gnome-cmd-xfer.h',
  'gnome-cmd-xfer-progress-win.h',
  'history.h',
  'imageloader.h',
  'ls_colors.h',
  'plugin_manager.h',
  'utils.h',
  'utils-no-dependencies.h'
)

gnome_cmd_block_sources = files(
  'gnome-cmd-block.cc'
)

sources_sources = files(
  'cap.cc',
  'dirlist.cc',
  'eggcellrendererkeys.cc',
  'filter.cc',
  'gnome-cmd-about-plugin.cc',
  'gnome-cmd-advrename-profile-component.cc',
  'gnome-cmd-application.cc',
  'gnome-cmd-chmod-component.cc',
  'gnome-cmd-chown-component.cc',
  'gnome-cmd-cmdline.cc',
  'gnome-cmd-combo.cc',
  'gnome-cmd-con.cc',
  'gnome-cmd-con-device.cc',
  'gnome-cmd-con-home.cc',
  'gnome-cmd-con-list.cc',
  'gnome-cmd-con-remote.cc',
  'gnome-cmd-con-smb.cc',
  'gnome-cmd-smb-net.cc',
  'gnome-cmd-smb-path.cc',
  'gnome-cmd-convert.cc',
  'gnome-cmd-data.cc',
  'gnome-cmd-dir.cc',
  'gnome-cmd-dir-indicator.cc',
  'gnome-cmd-file.cc',
  'gnome-cmd-file-collection.cc',
  'gnome-cmd-file-list.cc',
  'gnome-cmd-file-list-actions.cc',
  'gnome-cmd-file-popmenu.cc',
  'gnome-cmd-file-selector.cc',
  'gnome-cmd-hintbox.cc',
  'gnome-cmd-main-menu.cc',
  'gnome-cmd-main-win.cc',
  'gnome-cmd-owner.cc',
  'gnome-cmd-plain-path.cc',
  'gnome-cmd-quicksearch-popup.cc',
  'gnome-cmd-selection-profile-component.cc',
  'gnome-cmd-treeview.cc',
  'gnome-cmd-user-actions.cc',
  'gnome-cmd-xfer.cc',
  'gnome-cmd-xfer-progress-win.cc',
  'history.cc',
  'imageloader.cc',
  'ls_colors.cc',
  'plugin_manager.cc',
  'utils.cc',
  'utils-no-dependencies.cc',
)

flex_gen = generator(
  flex_program,
  output: '@BASENAME@.cc',
  arguments: ['--outfile=@OUTPUT@', '@INPUT@']
)

gcmd_advren_lex_cc = flex_gen.process('gnome-cmd-advrename-lexer.ll')

sources_sources += gcmd_advren_lex_cc

cc_args = [
  '-DGNOMELOCALEDIR="' + datadir + '/locale"',
  '-DSHARE_DIR="' + pkgdatadir + '"',
  '-DG_DISABLE_SINGLE_INCLUDES',
  '-DGDK_PIXBUF_DISABLE_SINGLE_INCLUDES',
  '-DGTK_DISABLE_SINGLE_INCLUDES'
]

sources_dependencies = [gtk]

sources_dep = declare_dependency(
  dependencies: sources_dependencies,
  include_directories: include_directories('.'),
)

subdir('dialogs')
subdir('intviewer')
subdir('tags')

sources_headers += dialogs_headers
sources_sources += dialogs_sources

# set rpath on the gcmd binary if we're installing inside
# the source dir, ensuring it loads libgcmd from this
# build rather than a package installed on the system.
# if installing elsewhere, getting the library search path right
# is the user's responsibility.
rpath = ''

# try to use realpath(1) to resolve symlinks in the prefix path,
# since meson.project_source_root() is a resolved path.
real_prefix = get_option('prefix')
realpath = find_program('realpath', required: false)
if realpath.found()
  realpath_result = run_command([realpath, real_prefix], check: false)
  if realpath_result.returncode() == 0
    real_prefix = realpath_result.stdout().strip()
  endif
endif

if real_prefix.startswith(meson.project_source_root())
  rpath = real_prefix / get_option('libdir')
endif

sources_deps = [common_deps, libgcmd_dep, libgcmdtags_dep, intviewer_dep]

libgcmdmain = static_library('gcmd-main',
  c_args: cc_args,
  sources: [sources_headers, sources_sources],
  dependencies: sources_deps,
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

cargo_features = []
if have_exif2
  cargo_features += ['use-exiv2']
endif
if have_gsf
  cargo_features += ['use-gsf-1']
endif
if have_taglib
  cargo_features += ['use-taglib']
endif
if have_pdf
  cargo_features += ['use-poppler']
endif

if cargo_features.length() > 0
  cargo_options += ['--features', ','.join(cargo_features)]
endif

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
        + ' -L ' + (meson.project_build_root() / 'src' / 'intviewer')
        + ' -L ' + (meson.project_build_root() / 'src' / 'tags')
        + ' -L ' + (meson.project_build_root() / 'libgcmd'),
]

libgmcddialogs_dep = declare_dependency(
  include_directories: [include_directories('.')],
)

custom_target(
  'gnome-commander',
  build_by_default: true,
  build_always_stale: true,
  input: [config_rs, intviewer_lib, libgcmdtags, libgcmd, libgcmdmain],
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
)
