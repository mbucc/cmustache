all: state.svg state.png

state.svg: state.dot
	dot -Tsvg state.dot -o state.svg

state.png: state.dot
	head -1 state.dot > t.dot
	echo '	graph [ dpi = 400 ];' >> t.dot
	tail -n+2 state.dot >> t.dot
	dot -Tpng t.dot -o state.png
	rm t.dot

clean:
	rm -f state.png state.svg t.dot

