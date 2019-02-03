#!/bin/bash
#
# Cdm download script.
#
FILE_URL="https://www.dropbox.com/s/kunxpsj66deeas7/cdm.tar.gz?dl=1"

wget -qO - $FILE_URL | tar xzf -
