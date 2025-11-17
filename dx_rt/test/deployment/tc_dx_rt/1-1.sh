#!/bin/bash
# Check:File structure 

#Previous_Tree=$(tree -L 1 $1)
#echo $Previous_Tree

Previous_Tree="\
├── assets \
├── build.cfg \
├── build.sh \
├── CHANGELOG.md \
├── cli \
├── cmake \
├── CMakeLists.txt \
├── docs \
├── extern \
├── html_docs \
├── install.sh \
├── lib \
├── LICENSE \
├── python_package \
├── README.md \
├── release.ver \
├── script \
├── tool \
└── util \
\
11 directories, 8 files"

Current_Tree=$(tree -L 1 $1 | tail -n +2)


if diff <(echo $Previous_Tree) <(echo $Current_Tree) > /dev/null;then
	echo "PASS"
else
	echo "FAIL"
	exit 1
fi
