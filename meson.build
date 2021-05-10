project('PretextView', ['c', 'cpp'],
    license: 'MIT',
    meson_version: '>=0.57.1',
    version: '0.2.0-dev'
)

flags = ['-Ofast']
if get_option('buildtype') == 'debug'
    flags = ['-O0', '-g', '-DDEBUG']
endif
flags += ['-DPV=' + meson.project_version()]

math_dep = meson.get_compiler('cpp').find_library('m', required : true)
dl_dep = meson.get_compiler('cpp').find_library('dl', required : true)
x11_dep = meson.get_compiler('cpp').find_library('X11', required : true) 

libdeflate =  static_library('deflate', 'libdeflate/lib/deflate_compress.c', 'libdeflate/lib/deflate_decompress.c', 'libdeflate/lib/utils.c', c_args : flags, link_with : 
                [   static_library('deflate_x86', 'libdeflate/lib/x86/cpu_features.c', c_args : flags, include_directories : 'libdeflate'),
                    static_library('deflate_arm', 'libdeflate/lib/arm/cpu_features.c', c_args : flags, include_directories : 'libdeflate')], include_directories : 'libdeflate')

thread_dep = dependency('threads')
cmake = import('cmake')
glfw_ops = cmake.subproject_options()
glfw_ops.add_cmake_defines({'GLFW_BUILD_EXAMPLES' : false, 'GLFW_BUILD_DOCS' : false, 'GLFW_INSTALL' : false, 'GLFW_BUILD_TESTS' : false})
glfw_ops.append_compile_args('c', flags)
glfw_ops.append_compile_args('cpp', flags)
glfw_dep = cmake.subproject('glfw', options : glfw_ops).dependency('glfw')

executable('PretextView', ['PretextView.cpp', 'glad.c'], dependencies : [thread_dep, glfw_dep, math_dep, dl_dep, x11_dep], cpp_args : flags, link_with : libdeflate, include_directories : ['include', 'libdeflate'], install : true)

meson.add_install_script('setup.sh', meson.project_build_root() + '/PretextView', meson.project_version(), meson.project_source_root() + '/pretextview_icon.svg')
