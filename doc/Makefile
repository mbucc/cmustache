all: state.svg state.png

state.svg: state.dot
	dot -Tsvg state.dot -o state.svg

state.png: state.dot
	dot -Tpng state.dot -o state.png

