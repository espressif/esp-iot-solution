#!/bin/bash

for i in en/latex/*.tex
do
    if [ $i != "main.tex" ] && [ $i != "structure.tex" ]
    then
	echo $i;
        target=`basename $i`	
	pandoc $i --verbose --number-sections --section-divs --reference-links -t rst -o en/rst/${target/tex/rst};
	# Change \label to and anchor before the section/chapter names
	perl -0777 -i -pe 's/\n\n([^\n]*\n[-~]*\n\n)\[sec:([^\]]*)\][ ]*/\n\n.. _sec_$2:\n\n$1/igs' en/rst/${target/tex/rst};
	# Change the \ref to restructured text's :ref:
	perl -0777 -i -pe 's/\[sec:([^\]]*)\]/:ref:`sec_$1`/igs' en/rst/${target/tex/rst};
	# Disable figure references, since there is no easy way to do this
	perl -0777 -i -pe 's/[ ]*\[fig:([^\]]*)\]//igs' en/rst/${target/tex/rst};
    fi
done
