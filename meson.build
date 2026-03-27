
project(
	'CodeOrb - Microcontroller Debugging',
	'cpp',
)

fs = import('fs')
cxx = meson.get_compiler('cpp')

# Sources

srcs = [
	'deps/SystemCore/Processing.cpp',
	'deps/SystemCore/Log.cpp',
	'deps/SystemCore/SystemDebugging.cpp',
	'deps/SystemCore/SystemCommanding.cpp',
	'deps/SystemCore/TcpListening.cpp',
	'deps/SystemCore/TcpTransfering.cpp',
	'deps/LibNaegCommon/Res.cpp',
	'deps/LibNaegCommon/LibTime.cpp',
	'deps/LibNaegCommon/LibFilesys.cpp',
	'deps/LibNaegCommon/LibDspc.cpp',
	'deps/LibNaegCommon/KeyUser.cpp',
	'deps/LibNaegCommon/TelnetFiltering.cpp',
	'deps/LibNaegCommon/ThreadPooling.cpp',
	'deps/LibNaegCommon/ListIdx.cpp',
	'deps/LibNaegCommon/widgets-term/Widget.cpp',
	'deps/LibNaegCommon/widgets-term/TextBox.cpp',
	'src/main.cpp',
	'src/GwSupervising.cpp',
	'src/GwMsgDispatching.cpp',
	'src/SingleWireScheduling.cpp',
	'src/LibSingleWireScheduling.cpp',
	'src/RemoteCommanding.cpp',
	'src/LibUart.cpp',
	'src/InfoGathering.cpp',
]

# Arguments

args = [
	'-DdPackageName="' + meson.project_name() + '"',
	'-DdAppName="codeorb"',
	'-DdVersion="CodeOrb-v1.26.3.1"',
	'-DCONFIG_PROC_HAVE_LOG=1',
	'-DCONFIG_PROC_HAVE_CORE_LOG=1',
	'-DCONFIG_PROC_LOG_HAVE_STDOUT=1',
	'-DCONFIG_PROC_LOG_HAVE_CHRONO=1',
	'-DCONFIG_CMD_SIZE_HISTORY=20',
	'-DCONFIG_CMD_SIZE_BUFFER_OUT=2048',
	'-DCONFIG_PROC_INFO_BUFFER_SIZE=1024',
]

# https://gcc.gnu.org/onlinedocs/gcc/Warning-Options.html
warningsGcc = [
	'-Wall',
	'-Wextra',
	'-Wpedantic',
	'-Werror',
	'-Wfatal-errors',
	'-Wreorder',
	'-Wswitch-enum',
	'-Wuseless-cast',
	'-Wparentheses',
	'-Wshift-overflow',
	'-Wsign-compare',
	'-Wzero-as-null-pointer-constant',
	'-Wcast-align',
	'-Wcast-qual',
	'-Wcatch-value',
	'-Wchar-subscripts',
	'-Wswitch-default',
	'-Wctor-dtor-privacy',
	'-Wduplicated-branches',
	'-Wduplicated-cond',
	'-Wempty-body',
	'-Wextra-semi',
	'-Wfloat-equal',
	'-Wformat',
	'-Wformat-extra-args',
	'-Wimplicit-fallthrough',
	'-Wmissing-field-initializers',
	'-Wnull-dereference',
	'-Wshadow',
]

warningsClang = [
	'-Wall',
	'-Wextra',
	'-Wpedantic',
	'-Werror',
	'-Wno-gnu-zero-variadic-macro-arguments',
	'-Wshadow',
	'-Wsign-conversion',
	'-Wnull-dereference',
	'-Wdouble-promotion',
	'-Wimplicit-fallthrough',
	'-Wcast-qual',
	'-Wcast-align',
	'-Wnon-virtual-dtor',
	'-Woverloaded-virtual',
	'-Wfloat-equal',
	'-Wswitch-enum',
	'-Wmissing-declarations',
	'-Wunreachable-code',
	'-Wdocumentation',
	'-Wthread-safety',
]

if host_machine.system() == 'windows'
	args += '-D_WIN32_WINNT=_WIN32_WINNT_WIN10'
	args += '-DWINVER=_WIN32_WINNT_WIN10'

	# wrestool -l CodeOrb.exe
	if cxx.get_id() == 'msvc'
		windres = find_program('rc', required: true)
		optWindres = [windres, '/fo', '@OUTPUT@', '@INPUT@']
	else
		windres = find_program('x86_64-w64-mingw32-windres', required: true)
		optWindres = [windres, '-i', '@INPUT@', '-O', 'coff', '-o', '@OUTPUT@']
		args += '-D__STDCPP_THREADS__=1'
	endif

	icon = custom_target('icon-resources',
		input: 'src/codeorb.rc',
		output: 'codeorb.res',
		depend_files: 'src/codeorb.ico',
		command: optWindres
	)

	srcs += icon
else
	args += '-std=gnu++11'
endif

if cxx.get_id() == 'msvc'
	args += '/std:c++17'
elif cxx.get_id() == 'gcc'
	args += warningsGcc
elif cxx.get_id() == 'clang'
	args += warningsClang
endif

# Dependencies

deps = []
deps += dependency('threads')

if host_machine.system() == 'windows'
	deps += cxx.find_library('ws2_32')
endif

if fs.is_dir('deps/tclap_loc')
	message('submodule tclap found')

	incTclap = include_directories(['deps/tclap_loc/include'], is_system: true)
	deps += declare_dependency(include_directories: incTclap)
	args += '-DAPP_HAS_TCLAP=1'
else
	warning('submodule tclap not found. Limited CLI options')
endif

# Application

nameExe = 'codeorb'
if host_machine.system() == 'windows'
	nameExe = 'CodeOrb'
endif

myApp = executable(
	nameExe,
	[
		srcs,
	],
	include_directories : include_directories([
		'./deps/SystemCore',
		'./deps/LibNaegCommon',
		'./deps/LibNaegCommon/widgets-term',
		'./src',
	]),
	dependencies : [
		deps,
	],
	cpp_args : [
		args,
	],
)

