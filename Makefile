GITHUB=https://raw.githubusercontent.com
SPEC=${GITHUB}/mustache/spec/master


# Mustache specifications.
getspecs: specs/interpolation.j

specs/%.json:
	curl ${SPEC}/$@ > t
	mv t $@


test:
	(cd regress ; make)

clean:
	(cd regress ; make clean)

	



