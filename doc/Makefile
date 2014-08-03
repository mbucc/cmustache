all: state.svg state.png

state.svg: state.dot
	dot -Tsvg state.dot -o state.svg

state.png: state.svg
	cp state.svg state.png
	#dot -Tpng state.dot -o state.png

clean:
	rm -f state.png state.svg

