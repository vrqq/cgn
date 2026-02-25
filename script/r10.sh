#!/bin/sh

podman_build() {
    ## uncommit these to enable http proxy (it will apply inside container)
    # HTTP_PROXY="http://192.168.122.1:20172"
    # export HTTPS_PROXY=$HTTP_PROXY
    # export HTTP_PROXY=$HTTP_PROXY
    # export http_proxy=$HTTP_PROXY
    # export https_proxy=$HTTP_PROXY
    pushd "$(dirname "$(readlink -f "$0")")"

    podman build -t lldb_rhel10:local -f ./Dockerfile.rhel10 .

    popd
}

create_vscode_container() {
    # get ../.. dir path of current script, and use it as the mount source
    CONTAINER_NAME=r10
    REPOROOT=$(readlink -f "$(dirname "$(readlink -f "$0")")/../..")

    read -p "Replace container '$CONTAINER_NAME' and mount '$REPOROOT', continue?"

    # Instal plugin ms-vscode.cpptools and llvm-vs-code-extensions.lldb-dap in default
    podman run --hostname $CONTAINER_NAME --name $CONTAINER_NAME -d --replace \ 
    --sig-proxy=false -a STDOUT -a STDERR \
    --mount source=${REPOROOT},target=${REPOROOT},type=bind \
    --mount type=volume,src=vscode,dst=/vscode \
    -l devcontainer.local_folder="${REPOROOT}" \
    -l devcontainer.metadata="[{\"customizations\":{\"vscode\":{\"extensions\":[\"ms-vscode.cpptools\",\"llvm-vs-code-extensions.lldb-dap\"]}}}]" \
    --security-opt label=disable --cap-add=SYS_PTRACE --security-opt=seccomp=unconfined \
    --entrypoint /bin/sh lldb_rhel10:local \
    -c 'echo "Container started"; trap "exit 0" 15; exec "$@"; while sleep 1 & wait $!; do :; done' -
}

run_bash() {
    podman exec -it r10 /bin/bash
}

run_lldb_platform(port) {
    # TODO: lldb-server gdbserver 0.0.0.0:$port
    podman exec -it r10 lldb-server p --server --listen "0.0.0.0:$port"
}

# if arg1 == "build" run podman_build
# if arg1 == "create" run create_vscode_container
# if arg1 == "bash" or "shell" run run_bash
# if arg1 == "lldb_platform" run run_lldb_platform with arg2 as port
# else print usage
case "$1" in
    build)
        podman_build
        ;;
    create)
        create_vscode_container
        ;;
    bash|shell)
        run_bash
        ;;
    lldb_platform)
        if [ -z "$2" ]; then
            echo "Usage: $0 lldb_platform <port>"
            exit 1
        fi
        run_lldb_platform "$2"
        ;;
    *)
        echo "Usage: $0 {build|create|bash|shell|lldb_platform}"
        exit 1
        ;;
esac
