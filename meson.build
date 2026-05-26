project('PretextView', ['c', 'cpp'],
    license: 'MIT',
    meson_version: '>=0.57.1',
    version: '0.2.5'
)

flags = ['-Ofast']
link_flags = ['-Ofast']
if get_option('buildtype') == 'debug'
    flags = ['-O0', '-g', '-DDEBUG', '-DInternal']
    link_flags = ['-O0']
endif
flags += ['-DPV=' + meson.project_version()]


dependencies = [dependency('threads')]
if host_machine.system() == 'linux'  # linux
    dependencies += [meson.get_compiler('cpp').find_library('m', required : true)]
    dependencies += [meson.get_compiler('cpp').find_library('dl', required : true)]
    dependencies += [meson.get_compiler('cpp').find_library('X11', required : true)] 
elif host_machine.system() == 'darwin' # macOS
    flags += ['-mmacosx-version-min=10.10']
    link_flags += ['-mmacosx-version-min=10.10']
    dependencies += [dependency('appleframeworks', modules : 'Cocoa')]
    dependencies += [dependency('appleframeworks', modules : 'OpenGL')]
    dependencies += [dependency('appleframeworks', modules : 'IOKit')]
    dependencies += [dependency('appleframeworks', modules : 'CoreVideo')]
else # windows 
    dependencies += [meson.get_compiler('cpp').find_library('gdi32', required : true)]
    dependencies += [meson.get_compiler('cpp').find_library('opengl32', required : true)]
    dependencies += [meson.get_compiler('cpp').find_library('kernel32', required : true)]
    dependencies += [meson.get_compiler('cpp').find_library('user32', required : true)]
    dependencies += [meson.get_compiler('cpp').find_library('shell32', required : true)]
    dependencies += [meson.get_compiler('cpp').find_library('Ole32', required : true)]
endif

# lib of deflate
# Created static library libdeflate using the static_library() function.
libdeflate =  static_library('deflate', 'subprojects/libdeflate/lib/deflate_compress.c', 'subprojects/libdeflate/lib/deflate_decompress.c', 'subprojects/libdeflate/lib/utils.c', 
    c_args : flags, 
    link_with : [
        static_library('deflate_x86', 'subprojects/libdeflate/lib/x86/cpu_features.c', c_args : flags, include_directories : 'subprojects/libdeflate'),
        static_library('deflate_arm', 'subprojects/libdeflate/lib/arm/cpu_features.c', c_args : flags, include_directories : 'subprojects/libdeflate')], 
    include_directories : 'subprojects/libdeflate')

# libglfw
cmake = import('cmake')
glfw_ops = cmake.subproject_options()
glfw_ops.add_cmake_defines({'GLFW_BUILD_EXAMPLES' : false, 'GLFW_BUILD_DOCS' : false, 'GLFW_INSTALL' : false, 'GLFW_BUILD_TESTS' : false})
glfw_ops.append_compile_args('c', flags)
glfw_ops.append_compile_args('cpp', flags)
glfw_ops.append_link_args(link_flags, language: ['c', 'cpp'])
dependencies += [cmake.subproject('glfw', options : glfw_ops).dependency('glfw')]

executable('PretextView', ['PretextView.cpp', 'src/layer.cpp', 'glad.c'], 
    dependencies : dependencies, 
    cpp_args : flags + ['--std=c++17'], 
    c_args : flags + ['--std=c17'], 
    link_args : link_flags, 
    link_with : libdeflate, 
    include_directories : ['include', 'src', 'libdeflate'], 
    install : true)

# gernate the macOS application, including the verion number, name, icon, ...
if host_machine.system() == 'darwin' 
    custom_target('plist', command : [find_program('make_macos_app_plist.sh'), meson.project_version()], build_by_default : true, build_always_stale : true, output : ['Info.plist'], install : true, install_dir : 'Contents')
    install_data('icon.icns', install_dir : 'Contents/Resources')
endif
