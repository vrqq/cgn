#!/bin/sh
if [ "$#" -ne 1 ]; then
    echo "Usage: sudo $0 <TARGET_DIR>"
    exit 1
fi

if [ "$EUID" -ne 0 ]; then 
  echo "root required"
  exit 1
fi

USER_ID=$(id -u)
TARGET_DIR=$1
IMAGE_NAME=c7dbg

# podman image pull centos:7.9.2009

mkdir -p "$TARGET_DIR"
IMAGE_MNT=$(podman image mount $IMAGE_NAME)

if [ $? -eq 0 ] && [ -n "$IMAGE_MNT" ]; then
    # 3. 執行 bind mount
    mount --bind "$IMAGE_MNT" "$TARGET_DIR"
    
    echo "成功！"
    echo "映像檔路徑: $IMAGE_MNT"
    echo "已綁定至: $TARGET_DIR"
    echo "卸載請執行: sudo umount $TARGET_DIR && sudo podman image umount $IMAGE_NAME"
else
    echo "掛載失敗，請檢查映像檔名稱是否正確"
    exit 1
fi
