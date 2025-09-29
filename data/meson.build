gschema_files = [
  'org.gnome.gnome-commander.gschema.xml',
  'org.gnome.gnome-commander.enums.xml',
]

glib_compile_schemas = find_program(
  'glib-compile-schemas',
  required: true
)

# Compile enums and schema into gschemas.compiled
custom_target('gschemas.compiled',
  input: gschema_files,
  output: 'gschemas.compiled',
  command: [
    glib_compile_schemas,
    '--strict',
    '--targetdir=' + meson.current_build_dir(),
    meson.current_source_dir()],
)

po_dir = meson.project_source_root() / 'po'

# Create metainfo file
metainfo_file = app_id + '.metainfo.xml'
metainfo_file = i18n.merge_file(
  input: metainfo_file + '.in',
  output: metainfo_file,
  po_dir: po_dir,
  install: true,
  install_dir: metainfodir
)

# Create desktop file
desktop_file = app_id + '.desktop'
desktop_file = i18n.merge_file(
  input: desktop_file + '.in',
  output: desktop_file,
  po_dir: po_dir,
  install: true,
  install_dir: desktopdir,
  type: 'desktop'
)

# Create Gentoo ebuild
configure_file(
  input: meson.project_name() + '.ebuild.in',
  output: meson.project_name() + '-' + meson.project_version() + '.ebuild',
  install: false,
  configuration: conf_data
)

# Validate desktop file
desktop_file_validate = find_program('desktop-file-validate', required: false)
if (desktop_file_validate.found())
  test(
    'validate-desktopfile',
    desktop_file_validate,
    args: [desktop_file.full_path()],
    workdir: meson.current_build_dir(),
    depends: desktop_file
  )
endif

# Validate metainfo file
appstreamcli = find_program('appstreamcli', required: false)
if (appstreamcli.found())
  test('validate-metainfo',
    appstreamcli,
    args: ['validate', '--no-net', '--explain', metainfo_file.full_path()],
    workdir: meson.current_build_dir()
  )
endif

install_data(gschema_files, install_dir: schemadir)
