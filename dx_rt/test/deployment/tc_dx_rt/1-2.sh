#!/bin/bash
## command : ./1-2.sh <target directory>
##
## e.g. $./1-2.sh dx_rt

# Check: CHANGELOG.md

TARGET_PATH=$1

cd $TARGET_PATH

# release.ver 파일에서 문자열을 읽어옵니다.
release_version=$(cat release.ver)
#release_version="v0.1.0"

# CHANGELOG.md 파일에서 release_version 문자열을 찾습니다.
if ! grep -q "$release_version" CHANGELOG.md; then
    #echo "The version $release_version does not exist in CHANGELOG.md"
    echo "FAIL"
    exit 0
fi


# 임시 변수를 사용하여 라인 수를 카운트합니다.
count=0
found=0

# CHANGELOG.md 파일을 줄 단위로 읽습니다.
while IFS= read -r line; do
    # release_version이 나타난 후에 카운트 시작
    if [[ $found -eq 1 ]]; then
        # 버전 형식의 줄이 나타나면 중단
        if [[ $line =~ ^.*v[0-9]+\.[0-9]+\.[0-9]+.*$ ]]; then
            # 낮은 버전이 나타나면 종료
            if [[ $line < $release_version ]]; then
                break
            fi
        fi
        count=$((count + 1))
    fi

    # release_version 줄을 찾았는지 확인
    if [[ $line == *"$release_version"* ]]; then
        found=1
    fi
done < CHANGELOG.md

# 6줄 이상인지 확인
if [[ $count -ge 6 ]]; then
   #"There are 6 or more lines before a lower version appears."a
   echo "PASS"
else
   #"There are fewer than 6 lines before a lower version appears."
   echo "FAIL"
fi

