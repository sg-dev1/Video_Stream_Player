import binascii
import os
import struct
import sys

# reset working directory to scripts directory
dir_path = os.path.dirname(os.path.realpath(__file__))
os.chdir(dir_path)

sys.stderr.write(***REMOVED***[license_req.py] Using working directory: %s\n***REMOVED*** % os.getcwd())

from amazon import Amazon

if __name__ == ***REMOVED***__main__***REMOVED***:
    az = Amazon()
    az.logIn()

    if len(sys.argv) != 3:
        raise Exception(***REMOVED***Usage: python %s resId challenge***REMOVED*** % sys.argv[0])

    resId = sys.argv[1]
    challenge = sys.argv[2]
    sys.stderr.write(***REMOVED***[license_req.py] challenge (%d): %s\n***REMOVED*** % (len(challenge), challenge) )
    serverResponse = az.getWidevine2License(resId, challenge)

    assert (***REMOVED***widevine2License***REMOVED*** in serverResponse)
    assert (***REMOVED***license***REMOVED*** in serverResponse[***REMOVED***widevine2License***REMOVED***])
    assert (***REMOVED***metadata***REMOVED*** in serverResponse[***REMOVED***widevine2License***REMOVED***])
    assert (***REMOVED***keyMetadata***REMOVED*** in serverResponse[***REMOVED***widevine2License***REMOVED***][***REMOVED***metadata***REMOVED***])

    license = serverResponse[***REMOVED***widevine2License***REMOVED***][***REMOVED***license***REMOVED***]
    sys.stderr.write(***REMOVED***[license_req.py] license: (%d): %s\n***REMOVED*** %
        (len(license), license) )
    print(license)
