#!/bin/bash

# 首先注意清理
make clean
# 从github上同步代码
echo "从github origin/SDFTL上同步代码,lab206的git密码为:123456"
git pull origin SDFTL
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


