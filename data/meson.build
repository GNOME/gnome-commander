gsettings_enum_files = [
  '../src/gnome-cmd-types.h',
  '../src/gnome-cmd-data.h',
  '../src/intviewer/search-dlg.h'
]

gsettings_enum_file_ready = 'org.gnome.gnome-commander.enums.xml'
glib_mkenums = find_program('glib-mkenums')

# Generate gschema enums file
schemas = custom_target(
  gsettings_enum_file_ready,
  input: gsettings_enum_files,
  output: gsettings_enum_file_ready,
  command: [glib_mkenums,
            '--comments', '<!-- @comment@ -->',
            '--fhead', '<schemalist>',
            '--vhead', '<@type@ id="org.gnome.gnome-commander.@EnumName@">',
            '--vprod', '    <value nick="@valuenick@" value="@valuenum@"/>',
            '--vtail', '  </@type@>',
            '--ftail', '</schemalist>', '@INPUT@'],
  capture: true,
  install: true,
  install_dir: join_paths(datadir, 'glib-2.0', 'schemas'))

glib_compile_schemas = find_program(
  'glib-compile-schemas',
  required: true
)

# Compile enums and schema into gschemas.compiled
gschemas_compiled = custom_target('gschemas.compiled',
  input: [
    'org.gnome.gnome-commander.gschema.xml',
    schemas
  ],
  output: 'gschemas.compiled',
  command: [
    glib_compile_schemas,
    '--targetdir=' + meson.current_build_dir(),
    meson.current_source_dir()],
  depends: schemas
)

po_dir = meson.project_source_root() / 'po'

# Create appdata file
appdata_file = app_id + '.appdata.xml'
appdata_file = i18n.merge_file(
  input: appdata_file + '.in',
  output: appdata_file,
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

# Validate appdata file
appstreamcli = find_program('appstreamcli', required: false)
if (appstreamcli.found())
  test('validate-appdata',
    appstreamcli,
    args: ['validate', '--no-net', '--explain', appdata_file.full_path()],
    workdir: meson.current_build_dir()
  )
endif

install_data(app_id + '.gschema.xml', install_dir: schemadir)
