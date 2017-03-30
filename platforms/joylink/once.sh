#!/bin/bash
MAKE_RULE=./Makefile.rule

set_project_path()
{
    sed -i "s%\(^PROJECT_ROOT_PATH:=\).*%\1$PWD%" $MAKE_RULE
}

create_project_path()
{
    if [ ! -d ./target/lib ];then
        mkdir -p ./target/lib
    fi

    if [ ! -d ./target/bin ];then
        mkdir -p ./target/bin
    fi

    if [ ! -d ./test/bin ];then
        mkdir -p ./test/bin
    fi
}

set_project_path
create_project_path

