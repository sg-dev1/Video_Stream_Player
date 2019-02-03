#!/usr/bin/bash
#
# Must be placed in Bento4's root folder

if [[ "$#" -ne 5 ]]; then
  echo "Usage: $0 PROVIDER CONTENT_ID KID KEY FILE_TO_ENCRYPT" >&2
  exit 1
fi

provider=$1
contentId=$2
kid=$3
key=$4
fileToEncrypt=$5

python2 Source/Python/utils/mp4-dash.py --exec-dir cmakebuild --widevine-header provider:$provider#content_id:$contentId --encryption-key $kid:$key $fileToEncrypt
