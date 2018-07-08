#!/bin/bash

# 首先注意清理
make clean
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
git pull origin master
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


