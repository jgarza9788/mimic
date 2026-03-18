project('scrollwm', 'cpp',
  version: '0.1.0',
  default_options: [
    'cpp_std=c++20',
    'warning_level=3',
    'buildtype=debugoptimized',
    'b_lto=false'
  ]
)

add_project_arguments([
  '-Wall',
  '-Wextra',
  '-Wpedantic',
  '-Wshadow',
  '-Wconversion',
  '-Wsign-conversion',
], language: 'cpp')

inc = include_directories('src', 'include')

xcb_dep = dependency('xcb', required: true)
xcb_keysyms_dep = dependency('xcb-keysyms', required: true)
xcb_icccm_dep = dependency('xcb-icccm', required: false)

deps = [xcb_dep, xcb_keysyms_dep]
if xcb_icccm_dep.found()
  deps += xcb_icccm_dep
endif

scrollwm_sources = files(
  'src/main.cpp',
  'src/wm.cpp',
  'src/x11/connection.cpp',
  'src/x11/atoms.cpp',
  'src/x11/events.cpp',
  'src/layout/scroll_layout.cpp',
  'src/model/client.cpp',
  'src/model/workspace.cpp',
  'src/config/config.cpp',
  'src/util/log.cpp',
)

executable('scrollwm',
  scrollwm_sources,
  include_directories: inc,
  dependencies: deps,
  install: true,
)

scrollwm_unit_tests = executable('scrollwm-unit-tests',
  files(
    'tests/unit_tests.cpp',
    'src/layout/scroll_layout.cpp',
    'src/model/workspace.cpp',
    'src/model/client.cpp',
    'src/config/config.cpp',
  ),
  include_directories: inc,
  dependencies: [xcb_dep],
)
test('scrollwm-unit-tests', scrollwm_unit_tests)

install_data('assets/xsessions/scrollwm.desktop',
  install_dir: join_paths(get_option('datadir'), 'xsessions')
)

install_data('config/config.toml.example',
  install_dir: join_paths(get_option('datadir'), 'doc', 'scrollwm')
)

install_data('README.md',
  install_dir: join_paths(get_option('datadir'), 'doc', 'scrollwm')
)

install_data('docs/milestones.md',
  install_dir: join_paths(get_option('datadir'), 'doc', 'scrollwm')
)

install_data('scripts/scrollwm-session',
  install_dir: get_option('bindir'),
  install_mode: 'rwxr-xr-x'
)
