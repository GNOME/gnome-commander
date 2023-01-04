tags_headers = files(
	'gnome-cmd-tags.h',
	'gnome-cmd-tags-doc.h',
	'gnome-cmd-tags-exiv2.h',
	'gnome-cmd-tags-file.h',
	'gnome-cmd-tags-poppler.h',
	'gnome-cmd-tags-taglib.h'
)

install_headers(
    tags_headers,
    subdir: meson.project_name() / 'src' / 'tags'
)

tags_sources = files(
    'gnome-cmd-tags.cc',
    'gnome-cmd-tags-doc.cc',
    'gnome-cmd-tags-exiv2.cc',
    'gnome-cmd-tags-file.cc',
    'gnome-cmd-tags-poppler.cc',
    'gnome-cmd-tags-taglib.cc'
)

libgcmdtags_deps = [common_deps, sources_dep]

if have_taglib
    libgcmdtags_deps += [taglib]
endif
if have_gsf
    libgcmdtags_deps += [libgsf]
endif
if have_exif2
    libgcmdtags_deps += [exiv2]
endif
if have_pdf
    libgcmdtags_deps += [pdf]
endif


libgcmdtags = static_library('gcmd-tags',
  dependencies: libgcmdtags_deps,
  include_directories: [configuration_inc, sources_inc],
  sources: [tags_headers, tags_sources],
)

libgcmdtags_dep = declare_dependency(
  include_directories: [include_directories('.')],
)
