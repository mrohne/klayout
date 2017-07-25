git stash
git fetch origin
git branch | grep / | while read b; do git rebase origin/master $b; done
git branch -D merged.old
git branch -M merged merged.old
git checkout -b merged origin/master
git branch | grep / | while read b; do git merge $b; done
git stash pop
