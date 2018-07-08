#!/bin/bash

# 首先注意清理
make clean
read -p "select push(0) or pull(1) code " s_flag
if [ $s_flag == "0" ]; then
    echo "往git同步代码 lab206的git代码为：123456"
    echo "zhouheyu 的git 密码为:zhou1993"
    whoaim
    echo "this git dir has remote :"
    git remote -v
    read -p "choose remote (gitee : 0 origin:1)" flag
    if [ $flag == "0" ];then
	    git push gitee  master
    elif [ $flag == "1" ];then
	    git push origin master
    fi
else
    # 从github上同步代码
    echo "从github 上同步代码,lab206的git密码为:123456"
    echo "从github 上同步代码 ，zhouheyu的git密码为L：zhou1993"
    whoami
    echo "this git dir has remote :"
    git remote -v
    read -p "choose remote (gitee :0 origin:1)" flag
    if [ $flag == "0" ];then
        git pull gitee  master
    elif [ $flag == "1" ];then
        git pull origin master
    fi
    #切换目录到上级目录make
    cd ../ ; make
    read -p "if make is successed yes?no" flag

    if [ $flag == "yes" ]; then
        cd test.release ; source runtest
        # 切换回原来目录
        cd ../src
    else
        # 切换回原来目录
        cd src
    fi


fi



