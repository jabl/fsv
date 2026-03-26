# SPDX-License-Identifier: MIT

project('fsv', 'c', version: '3.1alpha')
gtkdep = [dependency('gtk+-3.0'),
          dependency('gdk-pixbuf-2.0'), dependency('epoxy')]
cglm_dep = dependency('cglm', fallback : ['cglm', 'cglm_dep'])
compiler = meson.get_compiler('c')
conf = configuration_data()
conf.set_quoted('PACKAGE', meson.project_name())
conf.set_quoted('VERSION', meson.project_version())
conf.set_quoted('LOCALEDIR', get_option('datadir') + '/locale')
conf.set_quoted('DOCDIR', get_option('datadir') + '/doc/fsv')

has_gettext = compiler.has_function('gettext')
if has_gettext
  conf.set('ENABLE_NLS', 1)
endif

has_scandir = compiler.has_function('scandir')
if has_scandir
  conf.set('HAVE_SCANDIR', 1)
endif

has_file = find_program('file')
if has_file.found()
  conf.set('HAVE_FILE_COMMAND', 1)
  conf.set_quoted('FILE_COMMAND', has_file.path() + ' %s')
endif

if get_option('buildtype') == 'debug' or get_option('buildtype') == 'debugoptimized'
    add_project_arguments('-DDEBUG', language: 'c')
endif

configure_file(output : 'config.h',
               configuration : conf)

libm = compiler.find_library('m')

subdir('po')
subdir('debug')
subdir('lib')
subdir('src')

# This doesn't work with meson 0.61 and python 3.10
#rpm = import('rpm')
#rpm.generate_spec_template()
