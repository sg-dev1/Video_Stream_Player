import platform
import fnmatch
import sys

def detectHostArch():
    hostArch = platform.machine().lower()
    if hostArch in ('amd64', 'x86_64'):
        hostArch = 'x64'
    elif fnmatch.fnmatch(hostArch, 'i?86') or hostArch == 'i86pc':
        hostArch = 'ia32'
    elif hostArch.startswith('arm'):
        hostArch = 'arm'
    elif hostArch.startswith('mips'):
        hostArch = 'mips'
    return hostArch

if __name__ == ***REMOVED***__main__***REMOVED***:
    sys.stdout.write(detectHostArch())