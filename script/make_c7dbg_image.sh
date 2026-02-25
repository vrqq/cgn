#!/bin/sh

## We suggest to use root to build image, then we can mount this one to folder
## by mount_c7dbg.sh
##
if [ "$EUID" -ne 0 ]; then 
  read -p "we suggest to root user (sudo mount in later), continue?"
fi

## uncommit these to enable http proxy (it will apply inside container)
# export HTTPS_PROXY="http://192.168.122.1:20172"
# export HTTP_PROXY="http://192.168.122.1:20172"
# export http_proxy="http://192.168.122.1:20172"
# export https_proxy="http://192.168.122.1:20172"

pushd "$(dirname "$(readlink -f "$0")")"

podman build -t c7dbg -f ./Dockerfile.centos7 .

popd
