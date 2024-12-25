intviewer_headers = files(
    'bm_byte.h',
    'bm_chartype.h',
    'cp437.h',
    'datapresentation.h',
    'fileops.h',
    'gvtypes.h',
    'image-render.h',
    'inputmodes.h',
    'libgviewer.h',
    'search-dlg.h',
    'searcher.h',
    'text-render.h',
    'viewer-utils.h',
    'viewer-widget.h',
    'viewer-window.h'
)

intviewer_sources = files(
    'bm_byte.cc',
    'bm_chartype.cc',
    'cp437.cc',
    'datapresentation.cc',
    'fileops.cc',
    'image-render.cc',
    'inputmodes.cc',
    'search-dlg.cc',
    'searcher.cc',
    'text-render.cc',
    'viewer-utils.cc',
    'viewer-widget.cc',
    'viewer-window.cc'
)

intviewer_lib = static_library('gviewer',
  dependencies: [common_deps, sources_dep],
  include_directories: [configuration_inc, sources_inc, libgcmd_inc],
  sources: [intviewer_headers, intviewer_sources]
)

intviewer_dep = declare_dependency(
  include_directories: [include_directories('.')],
)
