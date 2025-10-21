project('gst-lv-compositor', 'c',
  version : '1.0.0',
  license : 'LGPL',
  meson_version : '>= 0.54.0',
  default_options : [ 
    'warning_level=1',  # Réduit pour éviter les warnings
    'buildtype=debugoptimized',
    'c_std=gnu99'
  ]
)

# Dépendances GStreamer
gst_dep = dependency('gstreamer-1.0', version : '>=1.18.0', required : true)
gst_base_dep = dependency('gstreamer-base-1.0', version : '>=1.18.0', required : true)
gst_video_dep = dependency('gstreamer-video-1.0', version : '>=1.18.0', required : true)

# Répertoire d'installation
plugins_install_dir = get_option('libdir') / 'gstreamer-1.0'

# Configuration - définir PACKAGE
cdata = configuration_data()
cdata.set('PACKAGE', '"gst-lv-compositor"')
cdata.set('VERSION', '"1.0.0"')
cdata.set('PACKAGE_NAME', '"gst-lv-compositor"')
cdata.set('PACKAGE_VERSION', '"1.0.0"')
cdata.set('PLUGIN_NAME', '"lvcompositor"')
cdata.set('LICENSE', '"LGPL"')

# Générer le fichier config.h si nécessaire
configure_file(output : 'config.h', configuration : cdata)

# Flags de compilation
plugin_c_args = [
  '-DHAVE_CONFIG_H=1',
  '-DGST_USE_UNSTABLE_API',
  '-DPACKAGE="@0@"'.format(meson.project_name()),
  '-DPACKAGE_NAME="@0@"'.format(meson.project_name()),
  '-DPACKAGE_VERSION="@0@"'.format(meson.project_version()),
  '-DGST_LICENSE="LGPL"',
  '-DGST_PACKAGE_NAME="GStreamer"',
  '-DGST_PACKAGE_ORIGIN="http://gstreamer.net/"',
]

sources = [
  'src/gstlvcompositor.c',
  'src/sei_merge.c',  # Ajoutez cette ligne
]


# Build du plugin
shared_library('gstlvcompositor',
  sources,
  c_args : plugin_c_args,
  include_directories : include_directories('src'),
  dependencies : [gst_dep, gst_base_dep, gst_video_dep],
  install : true,
  install_dir : plugins_install_dir,
  name_prefix : '',
)
