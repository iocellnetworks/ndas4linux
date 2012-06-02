#!/bin/sh
# Generating the self-extracting file with forcing to agree the license term.
if [ -z $1 ] ; then
	echo "Usage: $0 <file>"
	exit 1;
fi
if [ ! -r $1 ] ; then
	echo "Can't access the file"
	exit 1;
fi
echo "#!/bin/sh"
echo "more <<\"EOF\""
cat EULA.txt
echo "EOF"
echo "agreed="
echo "while [ -z \"\$agreed\" ]; do"
echo "	echo \"Do you agree to the above license terms? [yes or no] \""
echo "	read reply leftover"
echo "	case \$reply in"
echo "	[yY] | [yY][eE][sS])"
echo "		agreed=1"
echo "		;;"
echo "	[nN] | [nN][oO])"
echo "		echo \"If you don't agree to the license you can't use the `basename $1`\";"
echo "		exit 1"
echo "		;;"
echo "	esac"
echo "done"
echo "trap 'rm -f `basename $1`; exit 1' HUP INT QUIT TERM"
echo "tail -n +$((`wc -l EULA.txt | sed -e's|EULA.txt||'`+ 21)) \$0> `basename $1`"
echo "exit 0"
cat $1
