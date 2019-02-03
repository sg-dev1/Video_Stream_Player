import binascii
import os
import struct
import sys

# reset working directory to scripts directory
dir_path = os.path.dirname(os.path.realpath(__file__))
os.chdir(dir_path)

sys.stderr.write("[license_req.py] Using working directory: %s\n" % os.getcwd())

from amazon import Amazon

if __name__ == "__main__":
    az = Amazon()
    az.logIn()

    if len(sys.argv) != 3:
        raise Exception("Usage: python %s resId challenge" % sys.argv[0])

    resId = sys.argv[1]
    challenge = sys.argv[2]
    sys.stderr.write("[license_req.py] challenge (%d): %s\n" % (len(challenge), challenge) )
    serverResponse = az.getWidevine2License(resId, challenge)

    assert ("widevine2License" in serverResponse)
    assert ("license" in serverResponse["widevine2License"])
    assert ("metadata" in serverResponse["widevine2License"])
    assert ("keyMetadata" in serverResponse["widevine2License"]["metadata"])

    license = serverResponse["widevine2License"]["license"]
    sys.stderr.write("[license_req.py] license: (%d): %s\n" %
        (len(license), license) )
    print(license)
