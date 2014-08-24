DOCD=.

DOCCO=/usr/local/bin/docco

all: state.svg state.png cmustache.html

state.svg: state.dot
	dot -Tsvg state.dot -o state.svg

state.png: state.dot
	head -1 state.dot > t.dot
	echo '	graph [ dpi = 400 ];' >> t.dot
	tail -n+2 state.dot >> t.dot
	dot -Tpng t.dot -o state.png
	rm t.dot

%.html: ../%.c
	${DOCCO} -o ${DOCD} ../$(basename $(notdir $@)).c

${DOCCO}: 
	sudo npm install -g docco


clean:
	rm -f state.png state.svg t.dot
	rm -f *.html *.css

