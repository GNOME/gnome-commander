# *** Internal Viewer Tests *** Most of these only consist of serialised
# function calls for acceptance tests, actually. Functions of the internal
# viewer library are not fully tested by unit tests.
test('test_iv_viewerwidget',
  executable('test-iv-imagerenderer',
    ['iv_imagerenderer_test.cc', 'gcmd_tests_main.cc'],
    dependencies: [common_deps, intviewer_dep, gtest],
    include_directories: [configuration_inc, libgcmd_inc, sources_inc],
    c_args: ['-DTEST_DIR="@0@"'.format(meson.current_source_dir())],
    link_with: [intviewer_lib],
    override_options : ['c_std=c17', 'cpp_std=c++17'],
  ),
)

test('test_iv_inputmodes',
  executable('test-iv-inputmodes',
    ['iv_inputmodes_test.cc', 'gcmd_tests_main.cc'],
    dependencies: [common_deps, intviewer_dep, gtest],
    include_directories: [configuration_inc, libgcmd_inc, sources_inc],
    c_args: ['-DTEST_DIR="@0@"'.format(meson.current_source_dir())],
    link_with: [intviewer_lib],
    override_options : ['c_std=c17', 'cpp_std=c++17'],
  ),
)

test('test_iv_textrenderer',
  executable('test-iv-textrenderer',
    ['iv_textrenderer_test.cc', 'gcmd_tests_main.cc'],
    dependencies: [common_deps, intviewer_dep, gtest],
    include_directories: [configuration_inc, libgcmd_inc, sources_inc],
    c_args: ['-DTEST_DIR="@0@"'.format(meson.current_source_dir())],
    link_with: [intviewer_lib],
    override_options : ['c_std=c17', 'cpp_std=c++17'],
  ),
  workdir : meson.current_source_dir()
)

test('test_iv_viewerwidget_test',
  executable('test-iv-viewerwidget-test',
    ['iv_viewerwidget_test.cc', 'gcmd_tests_main.cc'],
    dependencies: [common_deps, intviewer_dep, gtest],
    include_directories: [configuration_inc, libgcmd_inc, sources_inc],
    c_args: ['-DTEST_DIR="@0@"'.format(meson.current_source_dir())],
    link_with: [intviewer_lib],
    override_options : ['c_std=c17', 'cpp_std=c++17'],
  ),
  workdir : meson.current_source_dir()
)

test('test_iv_fileops_test',
  executable('test-iv-fileops-test',
    ['iv_fileops_test.cc', 'gcmd_tests_main.cc'],
    dependencies: [common_deps, intviewer_dep, gtest],
    include_directories: [configuration_inc, libgcmd_inc, sources_inc],
    c_args: ['-DTEST_DIR="@0@"'.format(meson.current_source_dir())],
    link_with: [intviewer_lib],
    override_options : ['c_std=c17', 'cpp_std=c++17'],
  ),
  workdir : meson.current_source_dir()
)

test('test_iv_bm_search_test',
  executable('test-iv-bm-search-test',
    ['iv_bm_search_test.cc', 'gcmd_tests_main.cc'],
    dependencies: [common_deps, intviewer_dep, gtest],
    include_directories: [configuration_inc, libgcmd_inc, sources_inc],
    c_args: ['-DTEST_DIR="@0@"'.format(meson.current_source_dir())],
    link_with: [intviewer_lib],
    override_options : ['c_std=c17', 'cpp_std=c++17'],
  ),
)

test('test_iv_datapresentation_test',
  executable('test-iv-datapresentation-test',
    ['iv_datapresentation_test.cc', 'gcmd_tests_main.cc'],
    dependencies: [common_deps, intviewer_dep, gtest],
    include_directories: [configuration_inc, libgcmd_inc, sources_inc],
    c_args: ['-DTEST_DIR="@0@"'.format(meson.current_source_dir())],
    link_with: [intviewer_lib],
    override_options : ['c_std=c17', 'cpp_std=c++17'],
  ),
)

test('test_utils_no_dependencies_tests',
  executable('test-utils-no-dependencies-tests',
    ['utils_no_dependencies_tests.cc', '../src/utils-no-dependencies.cc', 'gcmd_tests_main.cc'],
    dependencies: [common_deps, intviewer_dep, gtest],
    include_directories: [configuration_inc, libgcmd_inc, sources_inc],
    c_args: ['-DTEST_DIR="@0@"'.format(meson.current_source_dir())],
    link_with: [intviewer_lib],
    override_options : ['c_std=c17', 'cpp_std=c++17'],
  ),
)
