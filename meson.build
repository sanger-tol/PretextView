project('PretextView', ['c', 'cpp'],
    license: 'MIT',
    meson_version: '>=0.57.1',
    version: '0.2.5-dev-1'
)

flags = ['-Ofast']
link_flags = ['-Ofast']
if get_option('buildtype') == 'debug'
    flags = ['-O0', '-g', '-DDEBUG', '-DInternal']
    link_flags = ['-O0']
endif
flags += ['-DPV=' + meson.project_version()]

deps = [dependency('threads')]
if host_machine.system() == 'linux'
    deps += [meson.get_compiler('cpp').find_library('m', required : true)]
    deps += [meson.get_compiler('cpp').find_library('dl', required : true)]
    deps += [meson.get_compiler('cpp').find_library('X11', required : true)] 
elif host_machine.system() == 'darwin'
    flags += ['-mmacosx-version-min=10.10']
    link_flags += ['-mmacosx-version-min=10.10']
    deps += [dependency('appleframeworks', modules : 'Cocoa')]
    deps += [dependency('appleframeworks', modules : 'OpenGL')]
    deps += [dependency('appleframeworks', modules : 'IOKit')]
    deps += [dependency('appleframeworks', modules : 'CoreVideo')]
else
    deps += [meson.get_compiler('cpp').find_library('gdi32', required : true)]
    deps += [meson.get_compiler('cpp').find_library('opengl32', required : true)]
    deps += [meson.get_compiler('cpp').find_library('kernel32', required : true)]
    deps += [meson.get_compiler('cpp').find_library('user32', required : true)]
    deps += [meson.get_compiler('cpp').find_library('shell32', required : true)]
    deps += [meson.get_compiler('cpp').find_library('Ole32', required : true)]
endif

libdeflate =  static_library('deflate', 'libdeflate/lib/deflate_compress.c', 'libdeflate/lib/deflate_decompress.c', 'libdeflate/lib/utils.c', c_args : flags, link_with : 
                [   static_library('deflate_x86', 'libdeflate/lib/x86/cpu_features.c', c_args : flags, include_directories : 'libdeflate'),
                    static_library('deflate_arm', 'libdeflate/lib/arm/cpu_features.c', c_args : flags, include_directories : 'libdeflate')], include_directories : 'libdeflate')

cmake = import('cmake')
glfw_ops = cmake.subproject_options()
glfw_ops.add_cmake_defines({'GLFW_BUILD_EXAMPLES' : false, 'GLFW_BUILD_DOCS' : false, 'GLFW_INSTALL' : false, 'GLFW_BUILD_TESTS' : false})
if host_machine.system() == 'windows'
    glfw_ops.add_cmake_defines({'GLFW_USE_HYBRID_HPG' : true, 'GLFW_VULKAN_STATIC' : false, 'USE_MSVC_RUNTIME_LIBRARY_DLL' : false})
    glfw_ops.append_compile_args('c', flags + ['-DWINVER=0x0601'] + ['-D_WIN32_WINNT=0x0601'])
    glfw_ops.append_compile_args('cpp', flags + ['-DWINVER=0x0601'] + ['-D_WIN32_WINNT=0x0601'])
else
    glfw_ops.append_compile_args('c', flags)
    glfw_ops.append_compile_args('cpp', flags)
    glfw_ops.append_link_args(link_flags)
endif
deps += [cmake.subproject('glfw', options : glfw_ops).dependency('glfw')]

if host_machine.system() == 'windows'
    executable('PretextView', ['PretextView.cpp', 'glad.c'] + import('windows').compile_resources('PretextView.rc'), dependencies : deps, cpp_args : flags, link_with : libdeflate, include_directories : ['include', 'libdeflate'], install : true, link_args : '/ENTRY:mainCRTStartup', win_subsystem : 'windows')
else
    executable('PretextView', ['PretextView.cpp', 'glad.c'], dependencies : deps, cpp_args : flags, c_args : flags, link_args : link_flags, link_with : libdeflate, include_directories : ['include', 'libdeflate'], install : true)
endif

if host_machine.system() == 'darwin' 
    custom_target('plist', command : [find_program('make_macos_app_plist.sh'), meson.project_version()], build_by_default : true, build_always_stale : true, output : ['Info.plist'], install : true, install_dir : 'Contents')
    install_data('icon.icns', install_dir : 'Contents/Resources')
endif
