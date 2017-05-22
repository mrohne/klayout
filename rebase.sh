git stash
git fetch origin
git branch | grep / | while read b; do git rebase origin/master $b; done
git branch -D merged.old
git branch -M merged merged.old
git checkout -b merged origin/master
git branch | grep / | while read b; do git merge $b; done
git stash pop
git commit -am"Mirror only, please undo"
git push --mirror mirror
git push --mirror mirror
git push --mirror mirror
git reset --soft HEAD~1
