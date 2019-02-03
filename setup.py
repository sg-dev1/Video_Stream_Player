from distutils.core import setup, Extension
import os

# force build environ to use g++
os.environ["CXX"] = "g++"

EXTRA_COMPILE_ARGS = ['-std=c++11']
DEFINE_MACROS = []
SOURCES = [
  'wvAdapter/pythonInterface.cc',
  'wvAdapter/lib/src/common/logging.cc'
]
INCLUDE_DIRS = [
  'wvAdapter/lib/include/common',
  'wvAdapter/lib/include/core'
]
LIBRARIES = ['WvAdapter']
LIBRARY_DIRS = ['wvAdapter/lib/build']

widevineAdapter = Extension('widevineAdapter',
                    extra_compile_args = EXTRA_COMPILE_ARGS,
                    define_macros = DEFINE_MACROS,
                    include_dirs = INCLUDE_DIRS,
                    libraries = LIBRARIES,
                    library_dirs = LIBRARY_DIRS,
                    sources = SOURCES)

setup (name = 'widevineAdapter',
       version = '1.0',
       description = 'Adapter for widevine cdm.',
       author = 'Stefan Gschiel',
       author_email = 'stefan.gschiel.sg@gmail.com',
       ext_modules = [widevineAdapter])

#
# copy task(s)
#
import sys
sys.path.append('wvAdapter/lib/scripts')

import subprocess
from detect_host_arch import detectHostArch
from pi_version import pi_version

LIBRARY_DIR = "/usr/local/lib"
hostArch = detectHostArch()
piVersion = pi_version()

rootDir = os.path.dirname(os.path.realpath(__file__))
print ("Using root dir %s" % rootDir)
#rootDir = os.path.join(relativeRootDir, "..", "..")
if hostArch == "arm":
    widevineCdmBinaryPath = os.path.join(rootDir, "lib", "WidevineCdm", "arm", "libwidevinecdm.so")
elif hostArch == "x64":
    widevineCdmBinaryPath = os.path.join(rootDir, "lib", "WidevineCdm", "x86_64", "libwidevinecdm.so")
else:
    print ("Error: Host architecture {0} is not supported.".format(hostArch))
    sys.exit(1)

# copy files
assert( not subprocess.check_call(
    ["cp", widevineCdmBinaryPath, LIBRARY_DIR], stdout=sys.stdout, stderr=sys.stderr) )

assert( not subprocess.check_call(
    ["cp", "%s/lib/Bento4/cmakebuild/libap4_shared.so" % rootDir, LIBRARY_DIR],
    stdout=sys.stdout, stderr=sys.stderr) )

assert( not subprocess.check_call(
    ["cp", "%s/wvAdapter/lib/build/libWvAdapter.so" % rootDir, LIBRARY_DIR],
    stdout=sys.stdout, stderr=sys.stderr) )
