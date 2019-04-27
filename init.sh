#!/bin/bash

echo "# mbclient" >> README.md
git init
#git add README.md
git add .
git commit -am "first commit"
git remote add origin git@github.com:reario/mbclient.git
git push -u origin master
