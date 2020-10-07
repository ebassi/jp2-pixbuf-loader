project('jp2-pixbuf-loader', 'c')
gdk_pixbuf = dependency('gdk-pixbuf-2.0', method: 'pkg-config')
gdk_pixbuf_moduledir = gdk_pixbuf.get_pkgconfig_variable('gdk_pixbuf_moduledir')
gdk_pixbuf_query_loaders = gdk_pixbuf.get_pkgconfig_variable('gdk_pixbuf_query_loaders')
gdk_pixbuf_query_loaders = find_program(get_option('gdk_pixbuf_query_loaders_path'), gdk_pixbuf_query_loaders, gdk_pixbuf_query_loaders+'-32', gdk_pixbuf_query_loaders+'-64')

openjpeg = dependency('libopenjp2')

pixbuf_loader_openjpeg = shared_library(
    'pixbufloader-jp2',
    'src/io-jp2.c',
    include_directories: 'src/',
    dependencies: [gdk_pixbuf, openjpeg],
    install: true,
    install_dir: gdk_pixbuf_moduledir,
)

cdata = configuration_data()
cdata.set('bindir', get_option('prefix') / get_option('bindir'))
configure_file(
  input: 'jp-pixbuf.thumbnailer.in',
  output: 'jp-pixbuf.thumbnailer',
  configuration: cdata,
  install: true,
  install_dir: get_option('datadir') / 'thumbnailers',
)

meson.add_install_script(gdk_pixbuf_query_loaders.path(), '--update-cache')

subdir('tests')
