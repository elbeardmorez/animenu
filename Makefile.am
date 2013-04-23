
@include@

SUBDIRS = src

.DEFAULT_GOAL = all-dist

all-dist: all
	@echo "installing '$(pkgnam)' to './bin' dir"
	@[ ! -d ./bin ] && mkdir ./bin
	@mv ./src/$(pkgnam) ./bin

clean-local:
	[ -d ./bin ] && rm -rf ./bin

.PHONY: all-dist

