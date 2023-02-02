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
    'scroll-box.h',
    'search-dlg.h',
    'search-progress-dlg.h',
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
    'scroll-box.cc',
    'search-dlg.cc',
    'search-progress-dlg.cc',
    'searcher.cc',
    'text-render.cc',
    'viewer-utils.cc',
    'viewer-widget.cc',
    'viewer-window.cc'
)

intviewer_lib = static_library('gviewer',
  dependencies: [common_deps, sources_dep],
  include_directories: [configuration_inc, sources_inc],
  sources: [intviewer_headers, intviewer_sources]
)

intviewer_dep = declare_dependency(
  include_directories: [include_directories('.')],
)
