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

deps = [dependency('threads')]
if host_machine.system() == 'linux'
    deps += [meson.get_compiler('cpp').find_library('m', required : true)]
    deps += [meson.get_compiler('cpp').find_library('dl', required : true)]
    deps += [meson.get_compiler('cpp').find_library('X11', required : true)] 
elif host_machine.system() == 'darwin'
    flags += ['-mmacosx-version-min=10.10']
    deps += [dependency('appleframeworks', modules : 'Cocoa')]
    deps += [dependency('appleframeworks', modules : 'OpenGL')]
    deps += [dependency('appleframeworks', modules : 'IOKit')]
    deps += [dependency('appleframeworks', modules : 'CoreVideo')]
endif

libdeflate =  static_library('deflate', 'libdeflate/lib/deflate_compress.c', 'libdeflate/lib/deflate_decompress.c', 'libdeflate/lib/utils.c', c_args : flags, link_with : 
                [   static_library('deflate_x86', 'libdeflate/lib/x86/cpu_features.c', c_args : flags, include_directories : 'libdeflate'),
                    static_library('deflate_arm', 'libdeflate/lib/arm/cpu_features.c', c_args : flags, include_directories : 'libdeflate')], include_directories : 'libdeflate')

cmake = import('cmake')
glfw_ops = cmake.subproject_options()
glfw_ops.add_cmake_defines({'GLFW_BUILD_EXAMPLES' : false, 'GLFW_BUILD_DOCS' : false, 'GLFW_INSTALL' : false, 'GLFW_BUILD_TESTS' : false})
glfw_ops.append_compile_args('c', flags)
glfw_ops.append_compile_args('cpp', flags)
deps += [cmake.subproject('glfw', options : glfw_ops).dependency('glfw')]

executable('PretextView', ['PretextView.cpp', 'glad.c'], dependencies : deps, cpp_args : flags, link_with : libdeflate, include_directories : ['include', 'libdeflate'], install : true)

if host_machine.system() == 'darwin' 
    run_command(find_program('make_macos_app_plist.sh'), meson.project_version(), check : true)
    install_data('icon.icns', install_dir : 'Contents/Resources')
    install_data('Info.plist', install_dir : 'Contents')
endif
