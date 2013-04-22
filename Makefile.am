
@include@

SUBDIRS = src

.DEFAULT_GOAL = all-dist

all-dist: all
	echo "installing '$(pkgnam)' to working dir"
	mkdir ./bin 2>/dev/null
	mv ./src/$(pkgnam) ./bin

clean-local:
	rm -f ./bin/$(pkgnam)
	rmdir ./bin

.PHONY: all-dist

