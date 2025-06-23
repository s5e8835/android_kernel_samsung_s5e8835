#!/bin/bash

if [ -z "$DEVICE" ]; then
    echo "Error: Target device is not set."
    echo "You can set it by: export DEVICE=*"
    echo "exiting.."
    exit 1
fi

./kernel_build/build.sh "$(pwd)" || exit 1
