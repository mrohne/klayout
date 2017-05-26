bundle=$1
oldpath=/usr/local/Cellar/qt/5.8.0_2/lib/
newpath=@executable_path/../Frameworks/
macdeployqt $1 -no-strip -libpath=.
find $1 -type f | 
    while read file; do 
	otool -h $file >/dev/null 2>&1 && otool -L $file 2>/dev/null | grep $oldpath | 
		while read deps version; do 
		    repl=$(echo $deps | sed -e "s#$oldpath#$newpath#");
		    install_name_tool -change $deps $repl $file;
		done;
    done
