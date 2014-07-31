HBARS=https://raw.githubusercontent.com/wycats/handlebars.js/master
            https://raw.githubusercontent.com/wycats/handlebars.js/master/src/handlebars.l
LEX_SRC=${HBARS}/src/handlebars.l
YACC_SRC=${HBARS}/src/handlebars.yy

get: handlebars.l handlebars.yy

handlebars.l:
	curl ${LEX_SRC} > $@

handlebars.yy:
	curl ${YACC_SRC} > $@




